stellaris-sd v0.1
============

A simple SPI SD card interface for the Stellaris Launchpad, written in C with TI's driverlib.

info
----

This provides a basic read/write interface to an SD card attached to (currently) the SSI0 peripheral.
Pins used are detailed in mmc.h, and \#defines can be changed from there - this is not pervasive, and 
there remain a few hardcoded constant. This will change.

This library uses TI's proprietarily licensed uartstdio software, and as of such I cannot include it in this 
"virally licensed" distribution. By default any code that uses it is \#define'd out, but this can be changed.
Simply uncomment __UART_DEBUG, and place uartstdio.c and uartstdio.h in the source directory.

A Makefile will be added as soon as I clean it up a bit.

Eventually this library should provide all the necessary functionality to run Chan's FAT-fs filesystem.

Otherwise, the code is commented decently, and everything should work with non-SDHC cards.
As always, ymmv.

license
-------

This software is made available under the Creative Commons BY-SA license. Any copies of this software may be 
freely distributed as long as proper attribution is made, this license retained, and any copies made available
under this license.
