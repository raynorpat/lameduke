#include "premap.h"

#include "board.h"
#include "engine.h"

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int game_volume;
int game_level;
int game_skill;
int game_level_set;
int game_demorec;
int game_quotes;
int game_debug;

int check_file_id(void)
{
    FILE *fp = plat_fopen("FILE_ID.DIZ", "rb");
    if (!fp)
        return 0;
    fclose(fp);
    return 0;
}

/* 0x154F0 */
static void show_help(const char *argv0)
{
    puts("\n *  Here's a little command line help...");
    printf("    %s [flags...]\n", argv0 ? argv0 : "d3d");
    puts("\t/?        This help message.\n");
    puts("\t/l#       Go to level #.");
    puts("\t/v#       Go to volume #.");
    puts("\t/s#       Skill # (0-3)");
    puts("\t/q        Turn on help quotes.");
    puts("\t/d        Activate DEBUG MODEs.");
    puts("\t/r        Record a demo.");
    puts("\t/oDIR     Reroute game output to DIR.");
    puts("\t/e        Encode maps...");
}

void check_args(int argc, char **argv)
{
    int i;

    game_volume = 0;
    game_level = 0;
    game_skill = 0;
    game_level_set = 0;
    game_demorec = 0;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        char flag;

        if (!arg || arg[0] != '/' || arg[1] == 0) {
            printf("Unknown command line paramater '%s'\n", arg ? arg : "");
            show_help(argv[0]);
            exit(0);
        }
        flag = arg[1];
        if (flag >= 'A' && flag <= 'Z')
            flag = (char)(flag - 'A' + 'a');
        switch (flag) {
        case '?':
            show_help(argv[0]);
            exit(0);
        case 'd':
            game_debug = 1;
            break;
        case 'e': {
            /* 0x15748 only prints; no map is written. It leaves the index at 10. */
            char name[] = "L?.MAP";
            int v;

            for (v = 0; v < 3; v++) {
                for (i = 0; i < 9; i++) {
                    name[0] = (char)('L' + v);
                    name[1] = (char)('1' + i);
                    printf("Encoding %s\n", name);
                }
            }
            break;
        }
        case 'l':
            game_level_set = 1;
            game_level = atoi(arg + 2) - 1;
            break;
        case 'o':
            /* The path goes to byte_8EE78, which nothing reads. */
            printf("I/O path rerouted to %s.\n", arg + 2);
            break;
        case 'q':
            game_quotes = 1;
            break;
        case 'r':
            game_demorec = 1;
            puts("Demorec mode on...");
            break;
        case 's':
            game_skill = atoi(arg + 2) - 1;
            break;
        case 'v':
            game_volume = atoi(arg + 2) - 1;
            break;
        default:
            printf("Unknown command line paramater '%s'\n", arg);
            show_help(argv[0]);
            exit(0);
        }
    }
}

int enterlevel(int mode)
{
    char name[16];

    (void)mode;
    if (game_volume < 0 || game_level < 0) {
        errorexit("Board not found.");
    }
    snprintf(name, sizeof name, "%c%d.MAP", 'L' + game_volume, game_level + 1);
    puts("Entering...");
    printf("Episode %d\n", game_volume + 1);
    printf("Level %d\n", game_level + 1);
    if (loadboard(name) != 0)
        errorexit("Board not found.");
    return 0;
}
