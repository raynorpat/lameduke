#include "cache1d.h"
#include "engine.h"

#include <stddef.h>

#define CACHE_BYTES 0x493E0
#define CACHE_SLOTS 128

static unsigned char cache_mem[CACHE_BYTES];
static unsigned char *cache_ptr;
static int cache_left;
static int cache_count;
static int cache_sizes[CACHE_SLOTS];

void initcache(void)
{
    cache_ptr = cache_mem;
    cache_count = 0;
    cache_left = CACHE_BYTES;
}

void *allocache(int size)
{
    unsigned char *block;

    if (cache_count > CACHE_SLOTS - 1)
        errorexit("Clog Error.");
    if (size < 0 || cache_left - size <= 0)
        errorexit("Out of memory!");
    cache_sizes[cache_count++] = size;
    block = cache_ptr;
    cache_ptr += size;
    cache_left -= size;
    return block;
}

int allocache_store(int dest, int size)
{
    (void)dest;
    allocache(size);
    return 0;
}
