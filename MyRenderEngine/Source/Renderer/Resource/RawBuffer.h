#pragma once
#include "RHI/RHI.h"
#include "EASTL/unique_ptr.h"

class RawBuffer
{
public:
    RawBuffer(const eastl::string& name);

    bool Create(uint32_t size, RHIMemoryType memoryType, bool uav);
    
    IRHIBuffer* GetBuffer() const { return m_pBuffer.get(); }
    IRHIDescriptor* GetSRV() const { return m_pSRV.get(); }
    IRHIDescriptor* GetUAV() const { return m_pUAV.get(); }

protected:
    eastl::string m_name;
    eastl::unique_ptr<IRHIBuffer> m_pBuffer;
    eastl::unique_ptr<IRHIDescriptor> m_pSRV;
    eastl::unique_ptr<IRHIDescriptor> m_pUAV;
};