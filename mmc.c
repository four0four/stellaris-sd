#include "mmc.h"

unsigned char tradeByte(unsigned char b) {
    // SSI is configured at 8 bits, so any data pulled in is constrained to the lower 8b
    // Warnings may occur due to casting to unsigned long 
    unsigned char buf = 0;
    SSIDataPut(SPI_BASE, b);
    SSIDataGet(SPI_BASE, &buf);
    return buf; 
}

void errorAndHang() {
    ROM_GPIOPinWrite(LED_BASE, (SUCCESS_LED | BUSY_LED), 0);
    ROM_GPIOPinWrite(LED_BASE, ERROR_LED, ERROR_LED);
    while(1);
}

unsigned char initializeCard() {
    unsigned char dataBuf = 0;
    // 32bit var for OCR values
    unsigned long OCR = 0;

    // Enable both the SSI peripheral
    // and the port that it resides on
    // - May be able to just OR these
    SysCtlPeripheralEnable(SPI_SYSCTL);
    SysCtlPeripheralEnable(SPI_SYSCTL_PORT);
    SysCtlPeripheralEnable(LED_SYSCTL); 
    // Set up GPIO pins to be used for SPI 
    // Uncomment below if you are using mux'd pins - SSI0 is not.
    GPIOPinConfigure(GPIO_PA2_SSI0CLK);
    GPIOPinConfigure(GPIO_PA4_SSI0RX);
    GPIOPinConfigure(GPIO_PA5_SSI0TX);

    GPIOPinTypeSSI(SSI_GPIO_BASE, SSI_TX | SSI_RX | SSI_CLK);
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
    // same potential warnings as mmc.c:6
    while(SSIDataGetNonBlocking(SPI_BASE, &dataBuf));
    // select the card
    ROM_GPIOPinWrite(CS_BASE, CS_PIN, 0);

    // send CMD0
    dataBuf = sendCommand(CMD0,0x00);
    if(dataBuf != 0x01) {
#ifdef __UART_DEBUG
        UARTprintf("\nFailed, read 0x%02x expected 0x01\n",dataBuf);
#endif
        return dataBuf; 
    }

    // SD v2/ SDHC compat
    dataBuf = sendCommand(CMD8, 0x000001aa);
    
    if(dataBuf != 0x05) { // 0x05 == illegal commmand, therefore SDv1/MMC is enough
      if(dataBuf != 0x01) {
        UARTprintf("CMD8 error!\n");
        errorAndHang();
      } else {
        OCR = tradeByte(0xff) << 24;
        OCR |= tradeByte(0xff) << 16;
        OCR |= tradeByte(0xff) << 8;
        OCR |= tradeByte(0xff);
        UARTprintf("OCR recieved from CMD8 = %08x\n",OCR);
        if((OCR & 0xfff) != 0x1aa) {
          UARTprintf("OCR 0x1AA not recieved!\n");
          errorAndHang();
        }

      }
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
        return dataBuf;
    }
    ROM_GPIOPinWrite(LED_BASE, BUSY_LED, 0);
    return 0; // successful initialization    
}

unsigned char sendCommand(unsigned char cmd, unsigned int arg) {
    unsigned int buf;
    // clear out any buffers
    // same potential warnings as mmc.c:6
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

void readSingleBlock(unsigned char *buf, unsigned int addr) {
    ROM_GPIOPinWrite(LED_BASE, BUSY_LED, BUSY_LED);
    unsigned char r = sendCommand(CMD17, addr);
    if(r != 0x00){
#ifdef __UART_DEBUG
        UARTprintf("Recieved error 0x%02x for CMD17(0x%02x)!\n",r,addr);
#endif
        errorAndHang();
    }
    // read in what we've requested
    do
        r = tradeByte(0xff);
    while (r == 0xff);
    
    if(r != 0xfe) {
#ifdef __UART_DEBUG
        UARTprintf("Invalid data token read, recieved %02x\n",r);
#endif
        errorAndHang();
    }
    for(int i = 0; i < 512; i++)
       buf[i] = tradeByte(0xff);
    while(tradeByte(0xff) != 0xff);
    ROM_GPIOPinWrite(LED_BASE, BUSY_LED, 0);
}

void writeSingleBlock(unsigned char *buf, unsigned int addr) {
    ROM_GPIOPinWrite(LED_BASE, BUSY_LED, BUSY_LED);
    unsigned char r = sendCommand(CMD24, addr);
    if(r != 0x00){
#ifdef __UART_DEBUG
        UARTprintf("Recieved error 0x%02x for CMD24(0x%02x)!\n",r,addr);
#endif
        errorAndHang();
    }

    // block start token
    tradeByte(0xfe);

    // push out data packet
    for(int i = 0; i < 512; i++)
        tradeByte(buf[i]);

    // CRC dummy bytes
    r = tradeByte(0xff);
    r = tradeByte(0xff);
    
    // card is now busy, wait for it to stop
    while(tradeByte(0xff) != 0xff);
    ROM_GPIOPinWrite(LED_BASE, BUSY_LED, 0);
}
