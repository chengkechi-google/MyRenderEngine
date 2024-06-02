#include "Renderer.h"
#include "ShaderCompiler.h"
#include "ShaderCache.h"
#include "PipelineCache.h"
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
    m_pRenderGraph->Clear();
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

    m_pFrameFence.reset(m_pDevice->CreateFence("Renderer::m_pFrameFence"));
    for (int i = 0; i < RHI_MAX_INFLIGHT_FRAMES; ++i)
    {
        eastl::string name = fmt::format("Renderer::m_pCommandList[{}]", i).c_str();
        m_pCommandLists[i].reset(m_pDevice->CreateCommandList(RHICommandQueue::Graphics, name));
    }

    m_pAsyncComputeFence.reset(m_pDevice->CreateFence("Renderer::m_pAsyncComputeFence"));
    for (int i = 0; i < RHI_MAX_INFLIGHT_FRAMES; ++i)
    {
        eastl::string name = fmt::format("Renderer::m_pComputeCommandLists[{}]", i).c_str();
        m_pComputeCommandLists[i].reset(m_pDevice->CreateCommandList(RHICommandQueue::Compute, name));
    }

    m_pUploadFence.reset(m_pDevice->CreateFence("Renderer::m_pUploadFence"));
    for (int i = 0; i < RHI_MAX_INFLIGHT_FRAMES; ++i)
    {
        eastl::string name = fmt::format("Renderer::m_pUploadCommandLists[{}]", i).c_str();
        m_pUploadCommandLists[i].reset(m_pDevice->CreateCommandList(RHICommandQueue::Copy, name));
        m_pStagingBufferAllocator[i] = eastl::make_unique<StagingBufferAllocator>(this);
    }


    // CreateCommonResources();
    m_pRenderGraph = eastl::make_unique<RenderGraph>(this);    
}

void Renderer::RenderFrame()
{
    CPU_EVENT("Render", "Renderer::RenderFrame");
    BeginFrame();
    UploadResources();
    Render();
    EndFrame();

    // MouseHitTest();
}

void Renderer::WaitGPUFinished()
{
    m_pFrameFence->Wait(m_currentFrameFenceValue);
}

IRHIShader* Renderer::GetShader(const eastl::string& file, const eastl::string& entryPoint, const eastl::string& profile, const eastl::vector<eastl::string>& defines, RHIShaderCompilerFlags flags /*= 0*/)
{
    return nullptr;
}

void Renderer::SetupGlobalConstants(IRHICommandList* pCommandList)
{
    
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

void Renderer::BeginFrame()
{
    CPU_EVENT("Render", "Renderer::BeginFrame");

    uint32_t frameIndex = m_pDevice->GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;
    {
        CPU_EVENT("Render", "IRHIFence::Wait");
        m_pFrameFence->Wait(m_frameFenceValue[frameIndex]);
    }
    m_pDevice->BeginFrame();

    IRHICommandList* pCommandList = m_pCommandLists[frameIndex].get();
    pCommandList->ResetAllocator();
    pCommandList->Begin();
    pCommandList->BeginProfiling();

    IRHICommandList* pComputeCommandList = m_pComputeCommandLists[frameIndex].get();
    pComputeCommandList->ResetAllocator();
    pComputeCommandList->Begin();
    pComputeCommandList->BeginProfiling();

    if (m_pDevice->GetFrameID() == 0)
    {
        //pCommandList->BufferBarrier()
    }
}

void Renderer::UploadResources()
{
    CPU_EVENT("Render", "Renderer::UploadResources");

    if (m_pendingTextureUploads.empty() && m_pendingBufferUploads.empty())
    {
        return;
    }

    uint32_t frameIndex = m_pDevice->GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;
    IRHICommandList* pUploadCommandList = m_pUploadCommandLists[frameIndex].get();
    pUploadCommandList->ResetAllocator();
    pUploadCommandList->Begin();

    {
        GPU_EVENT_DEBUG(pUploadCommandList, "Renderer::UplaodResources");
        for (size_t i = 0; i < m_pendingTextureUploads.size(); ++i)
        {
            const TextureUpload& upload = m_pendingTextureUploads[i];
            pUploadCommandList->CopyBufferToTexture(upload.m_pTexture, upload.m_mipLevel, upload.m_arraySlice, 
                upload.m_stagingBuffer.m_pBuffer, upload.m_stagingBuffer.m_offset + upload.m_offset);
        }
        m_pendingTextureUploads.clear();

        for (size_t i = 0; i < m_pendingBufferUploads.size(); ++i)
        {
            const BufferUpload& upload = m_pendingBufferUploads[i];
            pUploadCommandList->CopyBuffer(upload.m_pBuffer, upload.m_offset, upload.m_stagingBuffer.m_pBuffer, 
                upload.m_stagingBuffer.m_offset, upload.m_stagingBuffer.m_size);
        }
        m_pendingBufferUploads.clear();
    }

    pUploadCommandList->End();
    pUploadCommandList->Signal(m_pUploadFence.get(), ++ m_currentUploadFenceValue);
    pUploadCommandList->Submit();

    IRHICommandList* pCommandList = m_pCommandLists[frameIndex].get();
    pCommandList->Wait(m_pUploadFence.get(), m_currentUploadFenceValue);
}

void Renderer::Render()
{
    CPU_EVENT("Render", "Renderer::Render");

    uint32_t frameIndex = m_pDevice->GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;
    IRHICommandList* pCommandList = m_pCommandLists[frameIndex].get();
    IRHICommandList* pComputeCommandList = m_pComputeCommandLists[frameIndex].get();

    GPU_EVENT_DEBUG(pCommandList, fmt::format("Render Frame {}", m_pDevice->GetFrameID()).c_str());
    GPU_EVENT_PROFILER(pCommandList, "Render Frame");

    m_pRenderGraph->Clear();
    // m_pGPUScene->Update();

    // ImportPrevFrameTextures();

    RGHandle outputColorHandle, outputDepthHandle;
    BuildRenderGraph(outputColorHandle, outputDepthHandle);

    m_pRenderGraph->Compile();

    // m_pGPUDebugLine->Clear(pCommandList);
    // m_pGPUDebugPring->Clear(pCommandList);
    // m_pGPUStates->Clear(pCommandList);

    // SetupGlobalConstants(pCommandList);
    // FlushComputePass(pCommandList);
    // BuildRayTracingAS(pCommandList, pComputeCommandList);

    // m_pSkyyCubeMap->Update();

    // World* pWorld = Engine::GetInstance()->GetWorld();
    // Camera* pCamera = pWorld->GetCamera();
    // pCamera->DrawViewFrustum(pCommandList);

    m_pRenderGraph->Execute(this, pCommandList, pComputeCommandList);

    // RenderBackBufferPass(pCommandList, outputColorHandle, outputDepthHandle);
}

void Renderer::BuildRenderGraph(RGHandle& outColor, RGHandle& outDepth)
{
    
}

void Renderer::EndFrame()
{
    CPU_EVENT("Render", "Renderer::EndFrame");

    uint32_t frameIndex = m_pDevice->GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;

    IRHICommandList* pComputeCommandList = m_pComputeCommandLists[frameIndex].get();
    pComputeCommandList->End();
    pComputeCommandList->EndProfiling();

    IRHICommandList* pCommandList = m_pCommandLists[frameIndex].get();
    pCommandList->End();

    m_frameFenceValue[frameIndex] = ++ m_currentFrameFenceValue;

    pCommandList->Signal(m_pFrameFence.get(), m_currentFrameFenceValue);
    pCommandList->Submit();
    pCommandList->EndProfiling(); 
    {
        CPU_EVENT("Render", "IRHISwapchain::Present");
        m_pSwapChain->Present();
    }

    m_pStagingBufferAllocator[frameIndex]->Reset();
    m_pCBAllocator->Reset();

    m_pDevice->EndFrame();
}