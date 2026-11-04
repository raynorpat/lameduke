/* Map checks for the editor work: load and write a board, compare two
   files, validate a board's links. Exit 0 on success. */

#include "board.h"
#include "display.h"
#include "draw.h"
#include "engine.h"
#include "sdl_main.h"
#include "tiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int same(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    int ca, cb, r = 1;

    if (fa && fb) {
        do {
            ca = fgetc(fa);
            cb = fgetc(fb);
        } while (ca == cb && ca != EOF);
        r = ca != cb;
    }
    if (fa)
        fclose(fa);
    if (fb)
        fclose(fb);
    return r;
}

static void report(const char *why)
{
    puts(why);
}

int main(int argc, char **argv)
{

    if (argc == 4 && !strcmp(argv[1], "roundtrip")) {
        if (loadboard(argv[2]) != 0) {
            printf("%s: does not load\n", argv[2]);
            return 1;
        }
        return writeboard(argv[3], posx, posy, posz, ang, cursectnum) != 0;
    }
    if (argc == 4 && !strcmp(argv[1], "same"))
        return same(argv[2], argv[3]);
    if (argc == 3 && !strcmp(argv[1], "validate")) {
        if (loadboard(argv[2]) != 0) {
            printf("%s: does not load\n", argv[2]);
            return 1;
        }
        return checkboard(report) != 0;
    }
    /* one 3D frame at the map start with the editor's search pick on */
    if (argc == 5 && !strcmp(argv[1], "pick")) {
        platform_init();
        initengine();
        loadpics("tiles000.art");
        loadlookup();
        setgamemode();
        if (loadboard(argv[2]) != 0) {
            printf("%s: does not load\n", argv[2]);
            return 1;
        }
        searchx = atoi(argv[3]);
        searchy = atoi(argv[4]);
        searchit = 2;
        drawrooms(posx, posy, posz, ang, 100, cursectnum);
        drawmasks();
        if (getenv("MAPCHECK_FRAME")) {
            FILE *fp = fopen(getenv("MAPCHECK_FRAME"), "wb");
            const unsigned char *f = framebuffer();
            int i;

            fprintf(fp, "P6 %d %d 255\n", xdim, ydim);
            for (i = 0; i < xdim * ydim; i++)
                fwrite(&palette[3 * f[i]], 3, 1, fp);
            fclose(fp);
        }
        printf("searchstat %d searchsector %d searchwall %d\n", searchstat, searchsector, searchwall);
        return searchit != 1;
    }
    fprintf(stderr, "usage: mapcheck roundtrip IN OUT | same A B | validate MAP | pick MAP X Y\n");
    return 2;
}
