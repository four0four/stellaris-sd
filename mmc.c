#include "mmc.h"

unsigned char tradeByte(unsigned char b) {
    unsigned int buf = 0;
    SSIDataPut(SSI0_BASE, b);
    SSIDataGet(SSI0_BASE, &buf);
    return buf; 
}

unsigned char sendCommand(unsigned char cmd, unsigned int arg) {
    unsigned int buf;
    // clear out any buffers
    while(SSIDataGetNonBlocking(SSI0_BASE, &buf));

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
    ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);
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
    ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0);
}

void writeSingleBlock(unsigned char *buf, unsigned int addr) {
    ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);
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
    ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0);
}

void errorAndHang() {
    ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0);
    ROM_GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_PIN_1);
    while(1);
}
