#include <stdlib.h>
#include <string.h>
#include "editor.h"

#ifdef USE_OPENGL
extern int32_t polymosttexfullbright;
#endif

static int message_until;
static const char *names[] = { "Classic", "Polymost debug", "Polymost software", "Polymost OpenGL" };

static void saveconfig(void)
{
    videocfg_write("build.cfg", &videocfg);
}

int editor_is_override(const char *arg)
{
    return (arg[0] == '/' || arg[0] == '-') && arg[1] == 'p'
        && arg[2] >= '0' && arg[2] <= '9' && arg[3] == 0;
}

void editor_config_init(int argc, char **argv)
{
    int i;
    videocfg_read("build.cfg", &videocfg);
    videocfg_active = videocfg;
    for (i = 1; i < argc; i++)
        if (editor_is_override(argv[i]))
            videocfg_active.renderer = atoi(argv[i] + 2);
#ifdef USE_OPENGL
    polymosttexfullbright = 240;
#endif
    atexit(saveconfig);
}

void editor_set3d(void)
{
    videocfg_t active = videocfg_active;
    videocfg_apply(&active);
    setgamemode();
    setview(0, 0, xdim - 1, ydim - 1);
}

void editor_cycle_renderer(void)
{
    videocfg_t active = videocfg_active;
    int oldwidth = xdim, oldheight = ydim;
    int next = (active.renderer + 1) % 4;
    if (next == 3 && videocfg_gl_failed) next = 0;
    active.renderer = next;
    videocfg_apply(&active);
    if (!videocfg_apply_failed && videocfg_active.renderer != 1)
        videocfg.renderer = videocfg_active.renderer;
    setgamemode();
    setview(0, 0, xdim - 1, ydim - 1);
    searchx = scale(searchx, xdim, oldwidth);
    searchy = scale(searchy, ydim, oldheight);
    message_until = totalclock + 240;
}

void editor_renderer_message(int color)
{
    if (message_until && totalclock < message_until)
        printext256(0, 0, (short)color, 0, names[getrendermode()], 0);
}
