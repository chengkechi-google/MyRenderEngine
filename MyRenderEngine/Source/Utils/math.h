#pragma once
#include "assert.h"
#include "linalg/linalg.h"

using namespace linalg;
using namespace linalg::aliases;

inline bool IsPow2(uint32_t x)
{
    return (x & (x - 1)) == 0;
}

// It is round up to b, and b is power of 2
inline uint32_t RoundUpPow2(uint32_t a, uint32_t b)
{
    MY_ASSERT(IsPow2(b));
    return (a + b - 1) & ~(b - 1);
}