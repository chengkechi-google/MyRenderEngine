#pragma once
#include "rpmalloc/rpmalloc.h"

static inline void* MY_ALLOC(size_t size)
{
    return rpmalloc(size);
}

static inline void* MY_ALLOC(size_t size, size_t alignment)
{
    return rpaligned_alloc(alignment, size);
}

static inline void* MY_REALLOC(void* ptr, size_t size)
{
    return rprealloc(ptr, size);
}

static inline void MY_FREE(void* ptr)
{
    return rpfree(ptr);
}