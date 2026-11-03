#include "user.h"

#include <stdlib.h>

void *USRHOOKS_GetMem(int size)
{
    return malloc(size > 0 ? (size_t)size : 1);
}

void USRHOOKS_FreeMem(void *ptr)
{
    free(ptr);
}
