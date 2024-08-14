#pragma once

#include "D3D12Headers.h"
#include "../RHISwapChain.h"

class D3D12Device;

class D3D12SwapChain : public IRHISwapChain
{
public:
    D3D12SwapChain(D3D12Device* pDevice, const RHISwapChainDesc& desc, const eastl::string& name);
    ~D3D12SwapChain();

    virtual void* GetHandle() const override { return m_pSwapChain; }
    virtual void AcquireNextBackBuffer() override;
    virtual bool Present() override;
    virtual bool Resize(uint32_t width, uint32_t height) override;
    virtual void SetVsyncEnabled(bool value) override { m_enableVsync = value; }
    virtual IRHITexture* GetBackBuffer() const override;

    bool Create();
    
private:
    bool CreateTextures();

protected:
    IDXGISwapChain3* m_pSwapChain = nullptr;
    bool m_enableVsync = true;
    bool m_supportTearing = false;
    bool m_windowMode = true;
    uint32_t m_currentBackBuffer = 0;
    eastl::vector<IRHITexture*> m_backBuffer;
};
