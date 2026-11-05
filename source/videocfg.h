#ifndef VIDEOCFG_H
#define VIDEOCFG_H

typedef struct {
    int renderer;      /* 0, 2, 3 (1 only from /p) */
    int width, height;
    int display;       /* 0 windowed, 1 fullscreen, 2 borderless */
    int vsync;
    int filter;        /* 0 nearest, 1 bilinear, 2 trilinear */
    int miplevel;
    double contrast, saturation;
    int supersample;   /* 0 off, 1 2K, 2 4K, 3 8K */
} videocfg_t;

void videocfg_defaults(videocfg_t *c);
int videocfg_read(const char *path, videocfg_t *c);
int videocfg_write(const char *path, const videocfg_t *c);
void videocfg_apply(const videocfg_t *c);
extern videocfg_t videocfg;        /* saved settings */
extern videocfg_t videocfg_active; /* effective settings, including /p */
/* Last apply's renderer/resolution failure; GL failure stays until a retry. */
extern int videocfg_apply_failed, videocfg_gl_failed;

#endif
