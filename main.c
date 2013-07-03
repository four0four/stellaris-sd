#include "mmc.h"
#include "inc/hw_ints.h"
#include "inc/hw_timer.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"

/* -- Todo --
** -> Shift to direct register calls (speed!). Perhaps allow for ROM calls in inits?
** -> Support more variety of SD cards. implement SDHC, etc. 
** -> Support sequential (>512B) reads
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

static int timerFlag = 0;

void timer(void) {
    ROM_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    timerFlag = 1;
}

int main(void) {
    unsigned int startAddr = 0;
    unsigned char sectorBuffer[511];
    
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_16MHZ);

#ifdef __UART_DEBUG
    initConsole();
#endif
    initializeCard();
    //temptimer
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    ROM_IntMasterEnable();
    ROM_TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    ROM_TimerLoadSet(TIMER0_BASE, TIMER_A, ROM_SysCtlClockGet());
    ROM_IntEnable(INT_TIMER0A);
    TimerIntRegister(TIMER0_BASE, TIMER_A, timer);
    ROM_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    ROM_TimerEnable(TIMER0_BASE, TIMER_A);
    //
    readSingleBlock(sectorBuffer, startAddr);

    // print out xxd-style
    UARTprintf("Before writing:\n");
    hexDump(sectorBuffer, startAddr, 512);

    sectorBuffer[44] = 0x00;
    writeSingleBlock(sectorBuffer, startAddr);
    readSingleBlock(sectorBuffer, startAddr);

    UARTprintf("After writing:\n");
    hexDump(sectorBuffer, startAddr, 512);

    timerFlag = 0;
    unsigned int cnt = 0;
    ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, GPIO_PIN_3);
    while(!timerFlag) {
      readSingleBlock(sectorBuffer,cnt*512);
      ++cnt;
    }
    //UARTprintf("Read %u bits in 3 seconds\nRead in %u bits at %f Kbps\n",cnt*512,cnt*512,(float)(cnt*512/3)/1024);
    UARTprintf("Read %u bits in 1 second\n",cnt*512);               
    UARTprintf("Read for 1 second at %u kbps\n",cnt/2);               

    ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0);

    return 0;
}

