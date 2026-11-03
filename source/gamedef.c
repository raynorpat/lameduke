#include "gamedef.h"

#include "platform.h"

#include <stdio.h>

int compilecon(void)
{
    FILE *fp = plat_fopen("game.con", "rb");
    if (!fp)
        return -1;
    fclose(fp);
    puts("Compiling...");
    return 0;
}
