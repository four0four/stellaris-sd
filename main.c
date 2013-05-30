#include "mmc.h"

/* -- Todo --
** -> Shift to direct register calls (speed!). Perhaps allow for ROM calls in inits?
** -> Support more variety of SD cards. implement SDHC, etc. 
** -> FAT driver.
*/

#ifdef __UART_DEBUG
void
initConsole(void) {
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioInit(0);
}

void hexDump(unsigned char *d, unsigned int start, unsigned int sz) {
    for(int i=0;i<sz;i+=16){ // rows
        UARTprintf("%08x: ",i+start);
        for(int j=0;j<16;j+=2) // cols
            UARTprintf("%02x%02x ",d[i+j],d[i+j+1]);
        UARTprintf("\n");
    }
}

#else
void hexDump(unsigned char *d, unsigned int start, unsigned int sz) {
  return;
}
#endif


int main(void) {
    unsigned int startAddr = 0;
    unsigned char sectorBuffer[511];

    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_16MHZ);

#ifdef __UART_DEBUG
    initConsole();
#endif
    initializeCard();
    readSingleBlock(&sectorBuffer, startAddr);

    // print out xxd-style
    UARTprintf("Before writing:\n");
    hexDump(sectorBuffer, startAddr, 512);

    sectorBuffer[44] = 0x00;
    writeSingleBlock(&sectorBuffer, startAddr);
    readSingleBlock(&sectorBuffer, startAddr);

    UARTprintf("After writing:\n");
    hexDump(sectorBuffer, startAddr, 512);

    ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);
    return 0;
}
