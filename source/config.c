#include "game.h"

#include "platform.h"

#include <stdio.h>

/* 0x1B520. The file name in the binary is userdefs.tmp. */
void load_user_stats(void)
{
    FILE *fp = plat_fopen("userdefs.tmp", "rb");
    if (!fp)
        return;
    fclose(fp);
}
