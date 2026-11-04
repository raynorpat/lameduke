#ifndef SDL_INPUT_H
#define SDL_INPUT_H

/* Keyboard and mouse. The DOS build used IRQ 9 / int 33h and the BIOS keyboard. */

/* byte_1BCA10: set while a key is down; the game clears the keys it takes. */
extern unsigned char keystatus[256];
extern int mousesens; /* word_1257B7 */

/* Returns 1 when the window is closed. */
int input_poll(void);
void getmousevalues(int *dx, int *dy, int *buttons);
void input_clear(void);

/* Run `steps` of the 0x102A0 key ramp, then getinput 0x30E60: the mouse joins
   the ramp and the keys pack into the sync bits. */
void input_sample(int steps, int *fvel, int *svel, int *avel, int *bits);

#endif
