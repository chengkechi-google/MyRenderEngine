#include "Renderer.h"
#include "Core/Engine.h"
#include "Utils/profiler.h"
#include "Utils/log.h"

Renderer::Renderer()
{
    m_pCBAllocator = eastl::make_unique<LinearAllocator>(8 * 1024 * 1024); // 8 MB for constant buffer

    Engine::GetInstance()->WindowResizeSignal.connect(&Renderer::OnWindowResize, this);
}

Renderer::~Renderer()
{
    WaitGPUFinished();
    Engine::GetInstance()->WindowResizeSignal.disconnect(this);
}

void Renderer::CreateDevice(void* windowHandle, uint32_t windowWidth, uint32_t windowHeight)
{
    m_displayWidth = windowWidth;
    m_displayHeight = windowHeight;
    m_renderWidth = windowWidth;
    m_renderHeight = windowHeight;

    RHIDeviceDesc desc;
    desc.m_maxFrameDelay  = RHI_MAX_INFLIGHT_FRAMES;
    m_pDevice.reset(CreateRHIDevice(desc));
    if (m_pDevice == nullptr)
    {
        MY_ERROR("[Renderer::CreateDevice] failed to create the RHI device");
        return;
    }

    RHISwapChainDesc swapChainDesc = {};
    swapChainDesc.m_windowHandle = windowHandle;
    swapChainDesc.m_width = windowWidth;
    swapChainDesc.m_height = windowHeight;
    m_pSwapChain.reset(m_pDevice->CreateSwapChain(swapChainDesc, "Renderer::m_pSwapChain")); 
}

void Renderer::WaitGPUFinished()
{
    m_pFrameFence->Wait(m_currentFrameValue);
}

void Renderer::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    WaitGPUFinished();
    if (m_pSwapChain->GetDesc().m_windowHandle == window)
    {
        m_pSwapChain->Resize(width, height);
        
        m_displayWidth = width;
        m_displayHeight = height;
        m_renderWidth = (uint32_t)((float)m_renderWidth / m_upscaleRatio);
        m_renderHeight = (uint32_t)((float)m_renderHeight / m_upscaleRatio);
    }
}