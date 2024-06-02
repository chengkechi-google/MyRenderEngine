#pragma once
#include "RHI/RHI.h"
#include "EASTL/unique_ptr.h"

class IndexBuffer
{
public:
    IndexBuffer(const eastl::string& name);

    bool Create(uint32_t stride, uint32_t indexCount, RHIMemoryType memoryType);
    
    IRHIBuffer* GetBuffer() const { return m_pBuffer.get(); }
    uint32_t GetIndexCount() const { return m_indexCount; }
    RHIFormat GetFormat() const { return m_pBuffer->GetDesc().m_format; }
private:
    eastl::string m_name;

    eastl::unique_ptr<IRHIBuffer> m_pBuffer;
    uint32_t m_indexCount = 0;
};