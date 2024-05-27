#include "StagingBufferAllocator.h"
#include "Renderer.h"
#include "Utils/math.h"

#define BUFFER_SIZE (64 * 1024 * 1024) //< 64 mb

StagingBufferAllocator::StagingBufferAllocator(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

StagingBuffer StagingBufferAllocator::Allocate(uint32_t size)
{
    MY_ASSERT(size <= BUFFER_SIZE);

    if (m_pBuffers.empty())
    {
        CreateNewBuffer();
    }

    if (m_allocatedSize + size > BUFFER_SIZE)
    {
        CreateNewBuffer();
        m_currentBuffer ++;
        m_allocatedSize = 0;
    }

    StagingBuffer buffer;
    buffer.m_pBuffer = m_pBuffers[m_currentBuffer].get();
    buffer.m_size = size;
    buffer.m_offset = m_allocatedSize;

    m_allocatedSize += RoundUpPow2(size, 512);  // 512 : D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
    m_lastAllocatedFrame = m_pRenderer->GetFrameID();

    return buffer;
}

void StagingBufferAllocator::Reset()
{
    m_currentBuffer = 0;
    m_allocatedSize = 0;

    if (!m_pBuffers.empty())
    {
        if (m_pRenderer->GetFrameID() - m_lastAllocatedFrame > 100)
        {
            m_pBuffers.clear();
        }
    }
}

void StagingBufferAllocator::CreateNewBuffer()
{
    RHIBufferDesc desc;
    desc.m_size = BUFFER_SIZE;
    desc.m_memoryType = RHIMemoryType::CPUOnly;

    IRHIBuffer* pBuffer = m_pRenderer->GetDevice()->CreateBuffer(desc, "StagingBufferAllocator::m_pBuffer");
    m_pBuffers.push_back(eastl::unique_ptr<IRHIBuffer>(pBuffer));
}