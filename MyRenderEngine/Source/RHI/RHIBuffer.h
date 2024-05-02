#pragma once

#include "RHIResource.h"

class IRHIBuffer : public IRHIResource
{
public:
    const RHIBufferDesc& GetDesc() const { return m_desc; }
    
    virtual bool IsBuffer() const { return true; }
    virtual void* GetCPUAddress() const = 0;
    virtual uint64_t GetGPUAddress() const = 0;
    virtual uint32_t GetRequiredStagingBufferSize() const = 0;

protected:
    RHIBufferDesc m_desc = {};
};