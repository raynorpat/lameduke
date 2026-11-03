#ifndef NETWORK_H
#define NETWORK_H

/* 0x57AD7. comm == 0 is single player. 1..4 select a serial baud from
   rate (2400, 4800, 9600, 14400, 19200, 28800). 5 and above open IPX. */

int initmultiplayers(int comm, int rate);

#endif
