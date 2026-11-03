#ifndef SDL_INPUT_H
#define SDL_INPUT_H

/* Keyboard and mouse. The DOS build used IRQ 9 / int 33h and the BIOS keyboard.
   Returns 1 when the window is closed or Escape is pressed. */

int input_poll(void);

/* Run `steps` of the 0x102A0 key ramp, then getinput 0x30E60: the mouse joins
   the ramp and the keys pack into the sync bits. */
void input_sample(int steps, int *fvel, int *svel, int *avel, int *bits);

#endif
