#ifndef CACHE1D_H
#define CACHE1D_H

/* "BootLeg CacheMaster" bump allocator at D3D.EXE 0x3BB30 / 0x3BA30.
   128 slots, 0x493E0 bytes. Not the later hash cache. */

void initcache(void);
void *allocache(int size);

/* 0x3BB60. Stores the block through a pointer the decompiler lost. */
int allocache_store(int dest, int size);

#endif
