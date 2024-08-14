#pragma once
#include "RHIResource.h"

class IRHITexture;

class IRHISwapChain : public IRHIResource
{
public:
    virtual ~IRHISwapChain() {};
    
    virtual bool Present() = 0;
    virtual void AcquireNextBackBuffer() = 0;
    virtual bool Resize(uint32_t width, uint32_t height) = 0;
    virtual void SetVsyncEnabled(bool value) = 0;
    virtual IRHITexture* GetBackBuffer() const = 0;

    const RHISwapChainDesc& GetDesc() const { return m_desc; }

protected:
    RHISwapChainDesc m_desc = {};
};