#include "mmc.h"

#ifdef __UART_DEBUG
void
initConsole(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioInit(0);
}
#endif

int main(void) {
    unsigned char sectorBuffer[511];

    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_16MHZ);

#ifdef __UART_DEBUG
    initConsole();
#endif
    initializeCard();
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
