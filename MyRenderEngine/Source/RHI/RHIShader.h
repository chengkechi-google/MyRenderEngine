#pragma once
#include "RHIResource.h"

class IRHIShader : public IRHIResource
{
public:
    const RHIShaderDesc& GetDesc() const { return m_desc; }
    uint64_t GetHash() const { return m_hash; }
    virtual bool SetShaderData(const uint8_t* data, uint32_t dataSize) = 0;

protected:
    RHIShaderDesc m_desc = {};
    uint64_t m_hash= 0;
};