#ifndef SDL_INPUT_H
#define SDL_INPUT_H

/* Keyboard and mouse. The DOS build used IRQ 9 / int 33h and the BIOS keyboard.
   Returns 1 when the window is closed or Escape is pressed. */

int input_poll(void);

/* Run `steps` of the sub_102A0 ramp, then return the velocities a demo stores.
   mouse_turn is an extra angle in Build units and is not part of the ramp. */
void input_sample(int steps, int *fvel, int *svel, int *avel, int *look, int *mouse_turn);

#endif
