#pragma once
#include "RHI/RHI.h"
#include "EASTL/unique_ptr.h"

class ConstantBuffer
{
public:
    ConstantBuffer(const eastl::string& name);
    
    bool Create(uint32_t size, uint32_t stride, RHIMemoryType memoryType);

    IRHIBuffer* GetBuffer() const { return m_pBuffer.get(); }
    IRHIDescriptor* GetCBV() const { return m_pCBV.get(); }
private:
    eastl::string m_name;
    eastl::unique_ptr<IRHIBuffer> m_pBuffer;
    eastl::unique_ptr<IRHIDescriptor> m_pCBV;
};