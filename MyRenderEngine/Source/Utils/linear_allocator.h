#pragma once
#include "memory.h"
#include "assert.h"
#include "math.h"

class LinearAllocator
{
public:
    LinearAllocator(uint32_t size)
    {
        m_pMemory = MY_ALLOC(size);
        m_memorySize = size;       
    }

    ~LinearAllocator()
    {
        MY_FREE(m_pMemory);
    }

    void* Alloc(uint32_t size, uint32_t alignment = 1)
    {
        uint32_t address = RoundUpPow2(m_pointerOffset, alignment);
        MY_ASSERT(address + size <= m_memorySize);
        m_pointerOffset = address + size;
        return (char*)m_pMemory + address;
    }

private:
    void* m_pMemory = nullptr;
    uint32_t m_memorySize = 0;
    uint32_t m_pointerOffset = 0;
};


