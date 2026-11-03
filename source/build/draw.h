#ifndef DRAW_H
#define DRAW_H

/* The DOS column drawers sit just before the audio library. This view
   uses the same sector, wall and sprite records they walk. */

extern int parallaxtype; /* byte_23DE9D */
extern int visibility;  /* byte_23DE9E */

void draw_init(void);
void draw_view(void);
const unsigned char *framebuffer(void);
int framebuffer_pitch(void);

#endif
