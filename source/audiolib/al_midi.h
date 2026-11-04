#ifndef AL_MIDI_H
#define AL_MIDI_H

int AL_Init(int card);
void AL_NoteOff(int channel, int key, int velocity);
void AL_NoteOn(int channel, int key, int velocity);
void AL_ControlChange(int channel, int type, int data);
void AL_ProgramChange(int channel, int patch);
void AL_SetPitchBend(int channel, int lsb, int msb);

#endif
