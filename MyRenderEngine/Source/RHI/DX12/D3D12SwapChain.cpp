#include "D3D12SwapChain.h"
#include "D3D12Device.h"
#include "D3D12Texture.h"
#include "Utils/assert.h"
#include "Utils/log.h"
#include <wrl/client.h>

D3D12SwapChain::D3D12SwapChain(D3D12Device* pDevice, const RHISwapChainDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

D3D12SwapChain::~D3D12SwapChain()
{
    for (size_t i = 0; i < m_backBuffer.size(); ++i)
    {
        delete m_backBuffer[i];
    }
    m_backBuffer.clear();

    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    pDevice->Delete(m_pSwapChain);
}

void D3D12SwapChain::AcquireNextBackBuffer()
{
    m_currentBackBuffer = m_pSwapChain->GetCurrentBackBufferIndex();
}

bool D3D12SwapChain::Present()
{
    UINT interval, flags;
    if (m_enableVsync)
    {
        interval = 1;
        flags = 0;
    }
    else
    {
        interval = 0;
        flags = m_supportTearing && m_windowMode ? DXGI_PRESENT_ALLOW_TEARING : 0;
    }

    HRESULT hResult = m_pSwapChain->Present(interval, flags);
    return SUCCEEDED(hResult);
}

bool D3D12SwapChain::Resize(uint32_t width, uint32_t height)
{
    if (m_desc.m_width == width && m_desc.m_height == height)
    {
        return false;
    }

    m_desc.m_width = width;
    m_desc.m_height = height;
    m_currentBackBuffer = 0;

    for (size_t i = 0; i < m_backBuffer.size(); ++i)
    {
        delete m_backBuffer[i];
    }
    m_backBuffer.clear();

    ((D3D12Device *) m_pDevice)->FlushDeferredDeletions();

    DXGI_SWAP_CHAIN_DESC desc = {};
    m_pSwapChain->GetDesc(&desc);
    HRESULT hResult = m_pSwapChain->ResizeBuffers(m_desc.m_backBufferCount, width, height, desc.BufferDesc.Format, desc.Flags);
    if (!SUCCEEDED(hResult))
    {
        MY_ERROR("[D3D12Swapchain] failed to resize {}", m_name);
        return false;
    }

    BOOL fullScreenState;
    m_pSwapChain->GetFullscreenState(&fullScreenState, nullptr);
    m_windowMode = !fullScreenState;

    return CreateTextures();
}

IRHITexture* D3D12SwapChain::GetBackBuffer() const
{
    return m_backBuffer[m_currentBackBuffer];
}

bool D3D12SwapChain::Create()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    IDXGIFactory5* pFactory = pDevice->GetDXGIFactory();

    BOOL allowTearing = FALSE;
    pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    m_supportTearing = allowTearing;

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.BufferCount = m_desc.m_backBufferCount;
    desc.Width = m_desc.m_width;
    desc.Height = m_desc.m_height;
    desc.Format = m_desc.m_format == RHIFormat::RGBA8SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGIFormat(m_desc.m_format);
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.SampleDesc.Count = 1;
    desc.Flags = allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    
    IDXGISwapChain1* pSwapChain1 = NULL;
    HRESULT hResult = pFactory->CreateSwapChainForHwnd(pDevice->GetGraphicsQueue(), (HWND) m_desc.m_windowHandle, &desc, nullptr, nullptr, &pSwapChain1);
    if (FAILED(hResult))
    {
        MY_ERROR("[D3D12SwapChain] failed to create {}", m_name);
        return false;
    }

    pSwapChain1->QueryInterface(&m_pSwapChain);
    SAFE_RELEASE(pSwapChain1);
    return CreateTextures();
}

bool D3D12SwapChain::CreateTextures()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    RHITextureDesc desc = {};
    desc.m_width = m_desc.m_width;
    desc.m_height = m_desc.m_height;
    desc.m_format = m_desc.m_format;
    desc.m_usage = RHITextureUsageBit::RHITextureUsageRenderTarget;

    for (uint32_t i = 0; i < m_desc.m_backBufferCount; ++i)
    {
        ID3D12Resource* pBackBuffer = nullptr;
        HRESULT hResult = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        if (FAILED(hResult))
        {
            return false;
        }

        eastl::string name = fmt::format("{} texture {}", m_name, i).c_str();
        pBackBuffer->SetName(string_to_wstring(name).c_str());
        
        D3D12Texture* pTexture = new D3D12Texture(pDevice, desc, name);
        pTexture->m_pTexture = pBackBuffer;
        m_backBuffer.push_back(pTexture);
    }

    return true;
}