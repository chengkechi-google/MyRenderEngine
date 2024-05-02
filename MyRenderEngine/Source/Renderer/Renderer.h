#pragma once
#include "RHI/RHI.h"
#include "EASTL/unique_ptr.h"
#include "Utils/linear_allocator.h"

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void CreateDevice(void* windowHandle, uint32_t windowWidth, uint32_t windowHeight);
    void WaitGPUFinished();

    bool IsAsyncComputeEnabled() const {return m_enableAsyncCompute; }
    void SetAsyncComputeEnabled(bool value) { m_enableAsyncCompute = value; }

private:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

    

private:
    bool m_enableAsyncCompute = false;

    // Render resource
    eastl::unique_ptr<IRHIDevice> m_pDevice;
    eastl::unique_ptr<IRHISwapChain> m_pSwapChain;

    uint32_t m_displayWidth;
    uint32_t m_displayHeight;
    uint32_t m_renderWidth;
    uint32_t m_renderHeight;
    float m_upscaleRatio = 1.0f;
    uint64_t m_currentFrameValue = 0;
    eastl::unique_ptr<IRHIFence> m_pFrameFence;

    eastl::unique_ptr<LinearAllocator> m_pCBAllocator;
};