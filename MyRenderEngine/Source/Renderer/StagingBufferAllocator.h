#pragma once
#include "RHI/RHI.h"
#include "EASTL/unique_ptr.h"

class Renderer;

struct StagingBuffer
{
    IRHIBuffer* m_pBuffer;
    uint32_t m_size;
    uint32_t m_offset;    
};

class StagingBufferAllocator
{
public:
    StagingBufferAllocator(Renderer* pRenderer);
    
    StagingBuffer Allocate(uint32_t size);
    void Reset();

private:
    void CreateNewBuffer();

private:
    Renderer* m_pRenderer = nullptr;
    eastl::vector<eastl::unique_ptr<IRHIBuffer>> m_pBuffers;
    uint32_t m_currentBuffer = 0;
    uint32_t m_allocatedSize = 0;
    uint64_t m_lastAllocatedFrame = 0;
};