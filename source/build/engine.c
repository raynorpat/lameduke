#include "engine.h"
#include "display.h"
#include "draw.h"
#include "math.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char setup_cfg[6];
static unsigned char setup_keys[17];

static void loadsetup(void)
{
    FILE *fp = fopen("setup.dat", "rb");

    memset(setup_cfg, 0, sizeof setup_cfg);
    memset(setup_keys, 0, sizeof setup_keys);
    if (!fp)
        return;
    if (fread(setup_cfg, 1, sizeof setup_cfg, fp) != sizeof setup_cfg)
        memset(setup_cfg, 0, sizeof setup_cfg);
    if (fread(setup_keys, 1, sizeof setup_keys, fp) != sizeof setup_keys)
        memset(setup_keys, 0, sizeof setup_keys);
    fclose(fp);
}

int setup_byte(int index)
{
    if (index < 0 || index >= (int)sizeof setup_cfg)
        return 0;
    return setup_cfg[index];
}

void errorexit(const char *msg)
{
    if (msg && msg[0])
        puts(msg);
    /* 0x10C60 also tears down the timer and video before leaving. */
    exit(1);
}

void initengine(void)
{
    int wide;
    int tall;

    /* 0x4A0C0. Loads tables.dat and setup.dat, then setview over the full screen. */
    loadtables();
    loadsetup();
    if (setup_byte(0) == 7) {
        wide = 639;
        tall = 479;
    } else {
        wide = 319;
        tall = 199;
    }
    setview(0, 0, wide, tall);
    draw_init();
}
