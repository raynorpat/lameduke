#ifndef SDL_AUDIO_H
#define SDL_AUDIO_H

/* The output device: 16-bit stereo at rate, filled by fill(out, frames) on
   the audio thread. lock/unlock keep fill out while state changes. */
int platform_audio_open(int rate, void (*fill)(short *out, int frames));
void platform_audio_close(void);
void platform_audio_lock(void);
void platform_audio_unlock(void);

#endif
