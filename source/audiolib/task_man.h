#ifndef TASK_MAN_H
#define TASK_MAN_H

/* The one task this game schedules is the MIDI player (0x66114, 0x662CC,
   0x66244). dsl.c's audio callback stands in for the PIT and advances it
   once per output frame. */

void TS_ScheduleTask(void (*function)(void), int rate);
void TS_SetTaskRate(int rate);
void TS_Terminate(void);
void TS_Advance(unsigned outrate);

#endif
