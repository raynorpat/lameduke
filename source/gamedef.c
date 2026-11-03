#include "gamedef.h"

#include <stdio.h>

int compilecon(void)
{
    FILE *fp = fopen("game.con", "rb");
    if (!fp)
        return -1;
    fclose(fp);
    puts("Compiling...");
    return 0;
}
