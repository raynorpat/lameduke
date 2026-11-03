#include "sdl_window.h"
#include "display.h"

#include <SDL.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static unsigned char *rgb;
static int rgb_bytes;
static int tex_w;
static int tex_h;
static const char *video_error;

static void destroy_texture(void)
{
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }
    free(rgb);
    rgb = NULL;
    rgb_bytes = 0;
    tex_w = 0;
    tex_h = 0;
}

int platform_video(int width, int height)
{
    int window_w;
    int window_h;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        video_error = SDL_GetError();
        return -1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    window_w = width * 3;
    window_h = height * 3;
    /* 320x200 was stretched to 4:3 on a VGA monitor. */
    if (width == 320 && height == 200)
        window_h = 720;
    if (window)
        SDL_SetWindowSize(window, window_w, window_h);
    else {
        window = SDL_CreateWindow("Duke Nukem 3D",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            window_w, window_h, SDL_WINDOW_SHOWN);
        if (!window) {
            video_error = SDL_GetError();
            return -1;
        }
    }
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer)
            renderer = SDL_CreateRenderer(window, -1, 0);
        if (!renderer) {
            video_error = SDL_GetError();
            return -1;
        }
    }
    if (tex_w != width || tex_h != height) {
        destroy_texture();
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!texture) {
            video_error = SDL_GetError();
            return -1;
        }
        rgb_bytes = width * height * 3;
        rgb = (unsigned char *)malloc((size_t)rgb_bytes);
        if (!rgb) {
            video_error = "out of memory";
            return -1;
        }
        tex_w = width;
        tex_h = height;
    }
    video_error = NULL;
    return 0;
}

const char *platform_video_error(void)
{
    return video_error ? video_error : "video init failed";
}

void platform_present(const void *pixels, int width, int height)
{
    const unsigned char *src;
    int i;
    int n;

    if (!renderer || !texture || !rgb || !pixels)
        return;
    if (width != tex_w || height != tex_h)
        return;
    src = (const unsigned char *)pixels;
    n = width * height;
    for (i = 0; i < n; i++) {
        int c = src[i] * 3;
        rgb[i * 3] = palette[c];
        rgb[i * 3 + 1] = palette[c + 1];
        rgb[i * 3 + 2] = palette[c + 2];
    }
    SDL_UpdateTexture(texture, NULL, rgb, width * 3);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void platform_shutdown(void)
{
    destroy_texture();
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
