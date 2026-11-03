#include "network.h"

static int net_comm;
static int net_rate;

int initmultiplayers(int comm, int rate)
{
    static const int baud[] = { 2400, 4800, 9600, 14400, 19200, 28800 };

    net_comm = comm;
    net_rate = 0;
    if (comm > 0 && comm <= 4 && rate >= 0 && rate < 6)
        net_rate = baud[rate];
    (void)net_rate;
    return 0;
}
