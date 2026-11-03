#include "network.h"

short connecthead, connectpoint2[17], numplayers;

static int net_comm;
static int net_rate;

int initmultiplayers(int comm, int rate)
{
    static const int baud[] = { 2400, 4800, 9600, 14400, 19200, 28800 };

    int i;

    numplayers = 0;
    connecthead = 0;
    for (i = 0; i < 16; i++)
        connectpoint2[i] = -1;
    net_comm = comm;
    net_rate = 0;
    if (comm > 0 && comm <= 4 && rate >= 0 && rate < 6)
        net_rate = baud[rate];
    (void)net_rate;
    numplayers = 1;
    return 0;
}
