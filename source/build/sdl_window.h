#ifndef SDL_WINDOW_H
#define SDL_WINDOW_H

int platform_video(int width, int height);
const char *platform_video_error(void);
void platform_present(const void *pixels, int width, int height);
void platform_shutdown(void);
void platform_title(const char *text);
const char *platform_title_text(void);

#endif
