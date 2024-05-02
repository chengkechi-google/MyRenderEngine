#pragma once
#include "RHIResource.h"

class IRHITexture : public IRHIResource
{
public:
    const RHITextureDesc& GetDesc() const { return m_desc; }

    virtual bool IsTexture() const { return true; }
    virtual uint32_t GetRequiredStagingBufferSize() const = 0;
    virtual uint32_t GetRowPitch(uint32_t mipLevel) const = 0;
    virtual RHITilingDesc GetTilingDesc() const = 0;
    virtual RHISubresourceTilingDesc GetSubresourceTilingDesc(uint32_t subresource) const = 0;
    
protected:
    RHITextureDesc m_desc;
};