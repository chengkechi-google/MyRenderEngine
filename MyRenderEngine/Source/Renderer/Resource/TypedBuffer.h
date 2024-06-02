#pragma once
#include "RHI/RHI.h"
#include "EASTL/unique_ptr.h"

class TypedBuffer
{
public:
    TypedBuffer(const eastl::string& name);

    bool Create(RHIFormat format, uint32_t elementCount, RHIMemoryType memoryType, bool uav);
    
    IRHIBuffer* GetBuffer() { return m_pBuffer.get(); }
    IRHIDescriptor* GetSRV() { return m_pSRV.get(); }
    IRHIDescriptor* GetUAV() { return m_pUAV.get(); }

private:
    eastl::string m_name;
    eastl::unique_ptr<IRHIBuffer> m_pBuffer;
    eastl::unique_ptr<IRHIDescriptor> m_pSRV;
    eastl::unique_ptr<IRHIDescriptor> m_pUAV;
};