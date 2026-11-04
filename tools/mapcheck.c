/* Map checks for the editor work: load and write a board, compare two
   files, validate a board's links. Exit 0 on success. */

#include "board.h"

#include <stdio.h>
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
    fprintf(stderr, "usage: mapcheck roundtrip IN OUT | same A B | validate MAP\n");
    return 2;
}
