#include "mmc.h"

#ifdef __UART_DEBUG
void
InitConsole(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioInit(0);
}
#endif

int
main(void)
{
    unsigned long dataBuf;
    unsigned char sectorBuffer[511];

    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_16MHZ);

#ifdef __UART_DEBUG
    InitConsole();
#endif
    // Enable both the SSI peripheral
    // and the port that it resides on
    // - May be able to just OR these
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PORT);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    // Set up GPIO pins to be used for SPI 
    // Uncomment below if you are using mux'd pins - SSI0 is not.
    
    GPIOPinConfigure(GPIO_PA2_SSI0CLK);
    GPIOPinConfigure(GPIO_PA3_SSI0FSS);
    GPIOPinConfigure(GPIO_PA4_SSI0RX);
    GPIOPinConfigure(GPIO_PA5_SSI0TX);
    
    GPIOPinTypeSSI(SSI_BASE, SSI_TX | SSI_RX | SSI_FSS | SSI_CLK);
    // CS too...
    GPIOPinTypeGPIOOutput(CS_BASE, CS_PIN);
    // red = error, blue = busy/trigger (PF2), green = probable success
    ROM_GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3); 
    ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 0);
    // Configure SPI hardware
    SSIConfigSetExpClk(SSI0_BASE, SysCtlClockGet(), SSI_FRF_MOTO_MODE_0,
                       SSI_MODE_MASTER, SPI_SPEED, 8);

    // Pull CS high (deselect) and enable SSI->SPI
    ROM_GPIOPinWrite(CS_BASE, CS_PIN, CS_PIN);
    SSIEnable(SSI0_BASE);

    // SPI is primed and ready
    // Hold CS low for reset
#ifdef __UART_DEBUG
    UARTprintf("Resetting SD card...\n");
#endif

    for(int i = 0; i < 10; i++){
        SSIDataPut(SSI0_BASE, 0xff);
        SSIDataGet(SSI0_BASE, &dataBuf);
    }

    while(SSIDataGetNonBlocking(SSI0_BASE, &dataBuf));

    ROM_GPIOPinWrite(CS_BASE, CS_PIN, 0);

    // send CMD1
    dataBuf = sendCommand(CMD0,0x00);
    if(dataBuf != 0x01) {
#ifdef __UART_DEBUG
        UARTprintf("\nFailed, read 0x%02x expected 0x01\n",dataBuf);
#endif
        errorAndHang();
    }
    // wait for the card to complete internal initialization
    // should add a timeout!
    while(sendCommand(CMD1,0x00) != 0x00);


    dataBuf = sendCommand(CMD16,512);
    if(dataBuf != 0x00){
#ifdef __UART_DEBUG
        UARTprintf("CMD16 failed with 0x%02x\n",dataBuf);
#endif
        errorAndHang();
    }
    readSingleBlock(&sectorBuffer, 0x00);

    // print out xxd-style
#ifdef __UART_DEBUG
    UARTprintf("Before writing:\n");
    for(int i=0;i<512;i+=16){ // rows
        UARTprintf("%08x: ",i);
        for(int j=0;j<16;j+=2) // cols
            UARTprintf("%02x%02x ",sectorBuffer[i+j],sectorBuffer[i+j+1]);
        UARTprintf("\n");
    }
#endif
    sectorBuffer[44] = 0x00;

    writeSingleBlock(&sectorBuffer, 0x00);

    readSingleBlock(&sectorBuffer, 0x00);

#ifdef __UART_DEBUG
    UARTprintf("After writing:\n");
    for(int i=0;i<512;i+=16){ // rows
        UARTprintf("%08x: ",i);
        for(int j=0;j<16;j+=2) // cols
            UARTprintf("%02x%02x ",sectorBuffer[i+j],sectorBuffer[i+j+1]);
        UARTprintf("\n");
    }
#endif
    ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);
    return 0;
}
