#include "mmc.h"
#include "src/diskio.h"

unsigned char tradeByte(unsigned char b) {
    unsigned int buf = 0;
    SSIDataPut(SPI_BASE, b);
    SSIDataGet(SPI_BASE, &buf);
    return buf; 
}

void errorAndHang() {
    ROM_GPIOPinWrite(LED_BASE, (SUCCESS_LED | BUSY_LED), 0);
    ROM_GPIOPinWrite(LED_BASE, ERROR_LED, ERROR_LED);
    while(1);
}

DSTATUS disk_initialize(void) {
    unsigned char dataBuf = 0;

    // Enable both the SSI peripheral
    // and the port that it resides on
    // NOTE: for some reason these can't be or'd
    SysCtlPeripheralEnable(SPI_SYSCTL);
    SysCtlPeripheralEnable(SPI_SYSCTL_PORT);
    SysCtlPeripheralEnable(LED_SYSCTL); 
    // Set up GPIO pins to be used for SPI 
    // Uncomment and implement if necessary - SSI0 does not have mux'd pins
    // That said, it'll still work. Just not required.
    /*
    GPIOPinConfigure(GPIO_PA2_SSI0CLK);
    GPIOPinConfigure(GPIO_PA3_SSI0FSS);
    GPIOPinConfigure(GPIO_PA4_SSI0RX);
    GPIOPinConfigure(GPIO_PA5_SSI0TX);
    */
    GPIOPinTypeSSI(SSI_GPIO_BASE, SSI_TX | SSI_RX | SSI_FSS | SSI_CLK);
    // CS too...
    GPIOPinTypeGPIOOutput(CS_BASE, CS_PIN);
    // red = error, blue = busy/trigger (PF2), green = probable success
    ROM_GPIOPinTypeGPIOOutput(LED_BASE, ERROR_LED | BUSY_LED | SUCCESS_LED); 
    ROM_GPIOPinWrite(LED_BASE, ERROR_LED | BUSY_LED | SUCCESS_LED, 0);

    // Configure SPI hardware - 8 bits, master, system clock source
    SSIConfigSetExpClk(SPI_BASE,SysCtlClockGet(),SSI_FRF_MOTO_MODE_0,SSI_MODE_MASTER,SPI_SPEED,8);

    // Pull CS high (deselect) and enable SSI->SPI
    ROM_GPIOPinWrite(CS_BASE, CS_PIN, CS_PIN);
    SSIEnable(SPI_BASE);
    // SPI is primed and ready
    // Hold CS low for reset
#ifdef __UART_DEBUG
    UARTprintf("Resetting SD card...\n");
#endif

    ROM_GPIOPinWrite(LED_BASE, BUSY_LED, BUSY_LED);

    // hold MOSI high (0xFF) for >74 clk
    for(int i = 0; i < 10; i++)
        tradeByte(0xff);

    // clear any crap in buffers before selecting the card
    // probably not necessary, but safe
    while(SSIDataGetNonBlocking(SPI_BASE, &dataBuf));
    // select the card
    ROM_GPIOPinWrite(CS_BASE, CS_PIN, 0);

    // send CMD1
    dataBuf = sendCommand(CMD0,0x00);
    if(dataBuf != 0x01) {
#ifdef __UART_DEBUG
        UARTprintf("CMD0 failed with 0x%02x, expected 0x01\n",dataBuf);
#endif
        return STA_NOINIT; 
    }
    // wait for the card to complete internal initialization
    // should add a timeout - This can hang w/o error!
    while(sendCommand(CMD1,0x00) != 0x00);

    // set the read sector size to 512B
    dataBuf = sendCommand(CMD16,512);
    if(dataBuf != 0x00){
#ifdef __UART_DEBUG
        UARTprintf("CMD16 failed with 0x%02x\n",dataBuf);
#endif
        return STA_NOINIT; 
    }
    ROM_GPIOPinWrite(LED_BASE, BUSY_LED, 0);
    return 0; // successful initialization    
}

unsigned char sendCommand(unsigned char cmd, unsigned int arg) {
    unsigned int buf;
    // clear out any buffers
    while(SSIDataGetNonBlocking(SPI_BASE, &buf));

    tradeByte((cmd | 0x40));
    tradeByte(arg>>24); tradeByte(arg>>16); tradeByte(arg>>8); tradeByte(arg);

    // hard-coded CRC values for CMD0/8
    if(cmd == CMD0)
        tradeByte(0x95);
    else if (cmd == CMD8)
        tradeByte(0x87);
    // otherwise, doesn't matter
    else
        tradeByte(0xff);
    for(int i = 0; i < 9; i++) {
        buf = tradeByte(0xff);
        if(buf != 0xff)
            break;
        else continue;
    }
    while(tradeByte(0xff) != 0xff); // throw away any CRC or trailing bits
    return buf;
}

// uchar, ulong, ushort, ushort
DRESULT disk_read(BYTE *buf, DWORD sector, WORD sofs, WORD count){
    // argument validation
    // still work to do here - Also sector fixing
    if((sofs + count) > 512)
      return RES_PARERR;

    ROM_GPIOPinWrite(LED_BASE, BUSY_LED, BUSY_LED);
    unsigned char r = sendCommand(CMD17, sector);
    if(r != 0x00){
#ifdef __UART_DEBUG
        UARTprintf("Recieved error 0x%02x for CMD17(0x%02x)!\n",r,addr);
#endif
        return RES_ERROR;
    }
    // read in what we've requested
    do
        r = tradeByte(0xff);
    while (r == 0xff);
    
    if(r != 0xfe) {
#ifdef __UART_DEBUG
        UARTprintf("Invalid data token read, recieved %02x\n",r);
#endif
        return RES_ERROR;
    }
    // Read until start byte
    for(int i = 0; i < sofs; i++)
      tradeByte(0xff);
      
    for(int i = 0; i < count; i++)
       buf[i] = tradeByte(0xff);

    // Read out whatever remains
    while(tradeByte(0xff) != 0xff);

    ROM_GPIOPinWrite(LED_BASE, BUSY_LED, 0);
    return RES_OK;
}

DRESULT disk_writep(BYTE* buf, DWORD secNum) {
    ROM_GPIOPinWrite(LED_BASE, BUSY_LED, BUSY_LED);
    unsigned char r = 0;
    static unsigned int writeCount = 0;

    if((secNum + writeCount) > 512)
      return RES_PARERR;

    if(!buff) {
      if(secNum) {
        // init. write
        r = sendCommand(CMD24, addr);
        if(r != 0x00){
#ifdef __UART_DEBUG
          UARTprintf("Recieved error 0x%02x for CMD24(0x%02x)!\n",r,addr);
#endif
          return RES_ERROR;
        }
      // block start token
      tradeByte(0xfe);
      } else {
        // send out remaining block as 0, and then CRC (as 0)
        for(writeCount; writeCount < 514; writeCount++)
          tradeByte(0x00);
      }
    } else {
      while(int i = 0; i < secNum; i++)
        tradeByte(buf[i]);
      writeCount += secNum;
    }

    // card is now busy, wait for it to stop
    while(tradeByte(0xff) != 0xff);
    ROM_GPIOPinWrite(LED_BASE, BUSY_LED, 0);
    return RES_OK;
}
