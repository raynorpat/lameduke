#ifndef SDL_INPUT_H
#define SDL_INPUT_H

/* Keyboard and mouse. The DOS build used IRQ 9 / int 33h and the BIOS keyboard.
   Returns 1 when the window is closed or Escape is pressed. */

int input_poll(void);

/* Held movement, in the same units a demo sample stores (signed). */
void input_stick(int *fvel, int *svel, int *avel, int *look);

#endif
