#include "math.h"

#include "platform.h"

#include <stdio.h>
#include <string.h>

static unsigned char tables[TABLES_DAT_SIZE];
static int tables_ok;

int loadtables(void)
{
    FILE *fp;
    size_t n;

    if (tables_ok)
        return 0;
    fp = plat_fopen("tables.dat", "rb");
    if (!fp)
        return -1;
    n = fread(tables, 1, sizeof tables, fp);
    fclose(fp);
    if (n != sizeof tables) {
        memset(tables, 0, sizeof tables);
        return -1;
    }
    tables_ok = 1;
    return 0;
}

const unsigned char *tables_dat(void)
{
    return tables;
}

const short *sintable(void)
{
    if (!tables_ok)
        return NULL;
    return (const short *)tables;
}
