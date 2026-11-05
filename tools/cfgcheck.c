#include "videocfg.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    videocfg_t c;
    if (argc < 3 || (strcmp(argv[1], "read") && strcmp(argv[1], "roundtrip")))
        return 1;
    videocfg_read(argv[2], &c);
    if (!strcmp(argv[1], "roundtrip"))
        return argc == 4 ? videocfg_write(argv[3], &c) != 0 : 1;
    printf("renderer = %d\nwidth = %d\nheight = %d\ndisplay = %s\nvsync = %d\nfilter = %s\nmiplevel = %d\ncontrast = %.17g\nsaturation = %.17g\nsupersample = %s\n",
        c.renderer, c.width, c.height,
        c.display == 0 ? "windowed" : c.display == 1 ? "fullscreen" : "borderless",
        c.vsync, c.filter == 0 ? "nearest" : c.filter == 1 ? "bilinear" : "trilinear",
        c.miplevel, c.contrast, c.saturation,
        c.supersample == 0 ? "off" : c.supersample == 1 ? "2k" : c.supersample == 2 ? "4k" : "8k");
    return 0;
}
