#include "Renderer.h"
#include "TextureLoader.h"
#include "ShaderCompiler.h"
#include "ShaderCache.h"
#include "GPUScene.h"
#include "PipelineCache.h"
#include "Core/Engine.h"
#include "Utils/profiler.h"
#include "Utils/log.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "lodepng/lodepng.h"

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

void Renderer::SetTemporalUpscaleRatio(float ratio)
{
    if (!nearly_equal(m_upscaleRatio, ratio))
    {
        m_upscaleRatio = ratio;
    
        m_renderWidth = (uint32_t)((float) m_displayWidth / ratio);
        m_renderHeight = (uint32_t)((float) m_displayHeight / ratio);

        // If we use lower render size, maybe need to offset mipmap to get sharper result
        UpdateMipBias();
    }
}

IRHIShader* Renderer::GetShader(const eastl::string& file, const eastl::string& entryPoint, const eastl::string& profile, const eastl::vector<eastl::string>& defines, RHIShaderCompilerFlags flags /*= 0*/)
{
    return nullptr;
}

IRHIPipelineState* Renderer::GetPipelineState(const RHIGraphicsPipelineDesc& desc, const eastl::string& name)
{
    return m_pPipelineCache->GetPipelineState(desc, name);
}

IRHIPipelineState* Renderer::GetPipelineState(const RHIMeshShaderPipelineDesc& desc, const eastl::string& name)
{
    return m_pPipelineCache->GetPipelineState(desc, name);
}

IRHIPipelineState* Renderer::GetPipelineState(const RHIComputePipelineDesc& desc, const eastl::string& name)
{
    return m_pPipelineCache->GetPipelineState(desc, name);
}

void Renderer::ReloadShaders()
{
    m_pShaderCache->ReloadShaders();
}

IndexBuffer* Renderer::CreateIndexBuffer(const void* pData, uint32_t stride, uint32_t indexCount, const eastl::string& name, RHIMemoryType memoryType)
{
    eastl::vector<uint16_t> u16IB;
    if (stride == 1)
    {
        u16IB.resize(indexCount);
        for (uint32_t i = 0; i < indexCount; ++i)
        {
            u16IB[i] = ((const char*) pData)[i];
        }

        stride = 2;
        pData = u16IB.data();
    }

    IndexBuffer* pBuffer = new IndexBuffer(name);
    if (!pBuffer->Create(stride, indexCount, memoryType))
    {
        delete pBuffer;
        return nullptr;
    }

    if (pData)
    {
        UploadBuffer(pBuffer->GetBuffer(), 0, pData, stride * indexCount);
    }

    return pBuffer;
}

StructedBuffer* Renderer::CreateStructedBuffer(const void* pData, uint32_t stride, uint32_t elementCount, const eastl::string& name, RHIMemoryType memoryType, bool uav)
{
    StructedBuffer* pBuffer = new StructedBuffer(name);
    if (!pBuffer->Create(stride, elementCount, memoryType, uav))
    {
        delete pBuffer;
        return nullptr;
    }

    if (pData)
    {
        UploadBuffer(pBuffer->GetBuffer(), 0, pData, stride * elementCount);
    }

    return pBuffer;
}

TypedBuffer* Renderer::CreateTypedBuffer(const void* pData, RHIFormat format, uint32_t elementCount, const eastl::string& name, RHIMemoryType memoryType, bool uav)
{
    TypedBuffer* pBuffer = new TypedBuffer(name);
    if (!pBuffer->Create(format, elementCount, memoryType, uav))
    {
        delete pBuffer;
        return nullptr;
    }

    if (pData)
    {
        UploadBuffer(pBuffer->GetBuffer(), 0, pData, GetFormatRowPitch(format, 1) * elementCount);
    }

    return pBuffer;
}

RawBuffer* Renderer::CreateRawBuffer(const void* pData, uint32_t size, const eastl::string& name, RHIMemoryType memoryType, bool uav)
{
    RawBuffer* pBuffer = new RawBuffer(name);
    if (!pBuffer->Create(size, memoryType, uav))
    {
        delete pBuffer;
        return nullptr;
    }

    if (pData)
    {
        UploadBuffer(pBuffer->GetBuffer(), 0, pData, size);
    }

    return pBuffer;
}

Texture2D* Renderer::CreateTexture2D(const eastl::string& file, bool srgb)
{
    TextureLoader loader;
    if (!loader.Load(file, srgb))
    {
        return nullptr;
    }

    Texture2D* pTexture = new Texture2D(file);
    if (!pTexture->Create(loader.GetWidth(), loader.GetHeight(), loader.GetMipLevels(), loader.GetFormat(), 0))
    {
        delete pTexture;
        return nullptr;
    }

    UploadTexture(pTexture->GetTexture(), loader.GetData());
    return pTexture;
}

Texture2D* Renderer::CreateTexture2D(uint32_t width, uint32_t height, uint32_t levels, RHIFormat format, RHITextureUsageFlags flags, const eastl::string& name)
{
    Texture2D* pTexture = new Texture2D(name);
    if (!pTexture->Create(width, height, levels, format, flags))
    {
        delete pTexture;
        return nullptr;
    }

    return pTexture;
}

Texture3D* Renderer::CreateTexture3D(const eastl::string& file, bool srgb)
{
    TextureLoader loader;
    if (!loader.Load(file, srgb))
    {
        return nullptr;
    }

    Texture3D *pTexture = new Texture3D(file);
    if (!pTexture->Create(loader.GetWidth(), loader.GetHeight(), loader.GetDepth(), loader.GetMipLevels(), loader.GetFormat(), 0))
    {
        delete pTexture;
        return nullptr;
    }

    UploadTexture(pTexture->GetTexture(), loader.GetData());
    return pTexture;
}

Texture3D* Renderer::CreateTexture3D(uint32_t width, uint32_t height, uint32_t depth, uint32_t levels, RHIFormat format, RHITextureUsageFlags flags, const eastl::string& name)
{
    Texture3D* pTexture = new Texture3D(name);
    if (!pTexture->Create(width, height, depth, levels, format, flags))
    {
        delete pTexture;
        return nullptr;
    }

    return pTexture;
}

TextureCube* Renderer::CreateTextureCube(const eastl::string& file, bool srgb)
{
    TextureLoader loader;
    if (!loader.Load(file, srgb))
    {
        return nullptr;
    }

    TextureCube* pTexture = new TextureCube(file);
    if (!pTexture->Create(loader.GetWidth(), loader.GetHeight(), loader.GetMipLevels(), loader.GetFormat(), 0))
    {
        delete pTexture;
        return nullptr;
    }

    UploadTexture(pTexture->GetTexture(), loader.GetData());
    return pTexture;
}

TextureCube* Renderer::CreateTextureCube(uint32_t width, uint32_t height, uint32_t levels, RHIFormat format, RHITextureUsageFlags flags, const eastl::string& name)
{
    TextureCube* pTexture = new TextureCube(name);
    if (!pTexture->Create(width, height, levels, format, flags))
    {
        delete pTexture;
        return nullptr;
    }

    return pTexture;
}

void Renderer::SaveTexture(const eastl::string& file, const void* pData, uint32_t width, uint32_t height, RHIFormat format)
{
    if (strstr(file.c_str(), ".png"))
    {
        if (format == RHIFormat::R16UNORM)
        {
            lodepng::State state;
            state.info_raw.bitdepth = 16;
            state.info_raw.colortype = LCT_GREY;
            state.info_png.color.bitdepth = 16;
            state.info_png.color.colortype = LCT_GREY;
            state.encoder.auto_convert = 0;

            eastl::vector<uint16_t> dataBigEndian;
            dataBigEndian.reserve(width * height);
            for (int i = 0; i < width * height; ++i)
            {
                uint16_t x = ((const uint16_t*) pData)[i];
                dataBigEndian.push_back(((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8));
            }

            std::vector<unsigned char> png;
            lodepng::encode(png, (const unsigned char*) dataBigEndian.data(), width, height, state);
            lodepng::save_file(png, file.c_str());
        }
        else
        {
            stbi_write_png(file.c_str(), width, height, GetFormatComponentNum(format), pData, GetFormatRowPitch(format, width));
        }
    }
    else if (strstr(file.c_str(), ".bmp"))
    {
        stbi_write_bmp(file.c_str(), width, height, GetFormatComponentNum(format), pData);
    }
    else if (strstr(file.c_str(), ".tga"))
    {
        stbi_write_tga(file.c_str(), width, height, GetFormatComponentNum(format), pData);
    }
    else if (strstr(file.c_str(), ".hdr"))
    {
        stbi_write_hdr(file.c_str(), width, height, GetFormatComponentNum(format), (const float*) pData);
    }
    else if (strstr(file.c_str(), ".jpg"))
    {
        stbi_write_jpg(file.c_str(), width, height, GetFormatComponentNum(format), pData, 0);
    }
}

void Renderer::SaveTexture(const eastl::string& file, IRHITexture* pTexture)
{
    // todo: should copy texture object to staging buffer, and write data to image file
}

IRHIBuffer* Renderer::GetSceneStaticBuffer() const
{
    return m_pGPUScene->GetSceneStaticBuffer();
}

OffsetAllocator::Allocation Renderer::AllocateSceneStaticBuffer(const void* pData, uint32_t size)
{
    OffsetAllocator::Allocation allocation = m_pGPUScene->AllocateStaticBuffer(size);
    if (pData)
    {
        UploadBuffer(m_pGPUScene->GetSceneStaticBuffer(), allocation.offset, pData, size);
    }

    return allocation;
}

void Renderer::FreeSceneStaticBuffer(OffsetAllocator::Allocation allocation)
{
    m_pGPUScene->FreeStaticBuffer(allocation);
}

IRHIBuffer* Renderer::GetAScenenimationBuffer() const
{
    return m_pGPUScene->GetSceneAnimationBuffer();
}

OffsetAllocator::Allocation Renderer::AllocateSceneAnimationBuffer(uint32_t size)
{
    return m_pGPUScene->AllocateAnimationBuffer(size);
}

void Renderer::FreeSceneAnimationBuffer(OffsetAllocator::Allocation allocation)
{
    m_pGPUScene->FreeAnimationBuffer(allocation);
}

uint32_t Renderer::AllocateSceneConstant(const void* pData, uint32_t size)
{
    uint32_t address = m_pGPUScene->AllocateConstantBuffer(size);
    
    if(pData)
    {
        void* dst = (char*) m_pGPUScene->GetSceneConstantBuffer()->GetCPUAddress() + address;
        memcpy(dst, pData, size);
    }

    return address;
}

uint32_t Renderer::AddInstance(const InstanceData& data, IRHIRayTracingBLAS* pBLAS, RHIRayTracingInstanceFlags flags)
{
    return m_pGPUScene->AddInstance(data, pBLAS, flags);
}

void Renderer::RequestMouseHitTest(uint32_t x, uint32_t y)
{
    m_mouseX = x;
    m_mouseY = y;
    m_enableObjectIDRendering = true;
}

inline void ImageCopy(char* pDstData, uint32_t dstRowPitch, char* pSrcData, uint32_t srcRowPitch, uint32_t rowNum, uint32_t d)
{
    uint32_t srcSliceSize = srcRowPitch * rowNum;
    uint32_t dstSliceSize = dstRowPitch * rowNum;

    for (uint32_t z = 0; z < d; ++z)
    {
        char* pDstSlice = pDstData + dstSliceSize * z;
        char* pSrcSlice = pSrcData + srcSliceSize * z;

        for (uint32_t row = 0; row < rowNum; ++row)
        {
            memcpy(pDstSlice + dstRowPitch * row, pSrcSlice + srcRowPitch * row, srcRowPitch);
        }
    }
}

void Renderer::UploadTexture(IRHITexture* pTexture, const void* pData)
{
    uint32_t frameIndex = m_pDevice->GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;
    StagingBufferAllocator* pAllocator = m_pStagingBufferAllocator[frameIndex].get();

    uint32_t requiredSize = pTexture->GetRequiredStagingBufferSize();
    StagingBuffer buffer = pAllocator->Allocate(requiredSize);

    const RHITextureDesc& desc = pTexture->GetDesc();
    char* pDstData = (char*)buffer.m_pBuffer->GetCPUAddress() + buffer.m_offset;
    uint32_t dstOffset = 0;
    uint32_t srcOffset = 0;

    for (uint32_t slice = 0; slice < desc.m_arraySize; ++slice)
    {
        for (uint32_t mip = 0; mip < desc.m_mipLevels; ++mip)
        {
            uint32_t minWidth = GetFormatBlockWidth(desc.m_format);
            uint32_t minHeight = GetFormatBlockHeight(desc.m_format);

            uint32_t w = max(desc.m_width >> mip, minWidth);
            uint32_t h = max(desc.m_height >> mip, minHeight);
            uint32_t d = max(desc.m_depth >> mip, 1u);

            uint32_t srcRowPitch = GetFormatRowPitch(desc.m_format, w) * GetFormatBlockHeight(desc.m_format);
            uint32_t dstRowPitch = pTexture->GetRowPitch(mip);
            uint32_t rowNum = h / GetFormatBlockHeight(desc.m_format);

            ImageCopy(pDstData + dstOffset, dstRowPitch, (char*)pData + srcOffset, srcRowPitch, rowNum, d);

            TextureUpload upload;
            upload.m_pTexture = pTexture;
            upload.m_mipLevel = mip;
            upload.m_arraySlice = slice;
            upload.m_stagingBuffer = buffer;
            upload.m_offset = dstOffset;
            m_pendingTextureUploads.push_back(upload);

            dstOffset += RoundUpPow2(dstRowPitch * rowNum, 512);    //< 512 : D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT
            srcOffset += srcRowPitch * rowNum;
        }
    }   
}

void Renderer::UploadBuffer(IRHIBuffer* pBuffer, uint32_t offset, const void* pData, uint32_t dataSize)
{
    uint32_t frameIndex = m_pDevice->GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;
    StagingBufferAllocator* pAllocator = m_pStagingBufferAllocator[frameIndex].get();

    StagingBuffer stagingBuffer = pAllocator->Allocate(dataSize);

    char* pDstData = (char*) stagingBuffer.m_pBuffer->GetCPUAddress() + stagingBuffer.m_offset;
    memcpy(pDstData, pData, dataSize);

    BufferUpload upload;
    upload.m_pBuffer = pBuffer;
    upload.m_offset = offset;
    upload.m_stagingBuffer = stagingBuffer;
    m_pendingBufferUploads.push_back(upload);
}

void Renderer::BuildRayTracingBLAS(IRHIRayTracingBLAS* pBLAS)
{
    m_pendingBLASBuilds.push_back(pBLAS);
}

void Renderer::UpdateRayTracingBLAS(IRHIRayTracingBLAS* pBLAS, IRHIBuffer* vertexBuffer, uint32_t vertexBufferOffset)
{
    m_pendingBLASUpdates.push_back({pBLAS, vertexBuffer, vertexBufferOffset});
}

RenderBatch& Renderer::AddBasePassBatch()
{
    // Using forward pass for render tesing
    return m_forwardPassBatchs.emplace_back(*m_pCBAllocator);
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
        //pCommandList->BufferBarrier(
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

void Renderer::UpdateMipBias()
{
    if (m_renderWidth == m_displayWidth)
    {
        m_mipBias = 0.0f;
    }
    else
    {
        m_mipBias = log2f((float) m_renderWidth / (float) m_displayWidth);// - 1.0f; //< back to original resolution
    }

    RHISamplerDesc desc;
    desc.m_minFilter = RHIFilter::Linear;
    desc.m_magFilter = RHIFilter::Linear;
    desc.m_magFilter = RHIFilter::Linear;
    desc.m_addressU = RHISamplerAddressMode::Repeat;
    desc.m_addressV = RHISamplerAddressMode::Repeat;
    desc.m_addressW = RHISamplerAddressMode::Repeat;
    desc.m_mipBias = m_mipBias;
    desc.m_enableAnisotropy = true;
    desc.m_maxAnisotropy = 2.0f;
    m_pAniso2xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso2xSampler"));

    desc.m_maxAnisotropy = 4.0f;
    m_pAniso4xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso4xSampler"));

    desc.m_maxAnisotropy = 8.0f;
    m_pAniso8xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso8xSampler"));

    desc.m_maxAnisotropy = 16.0f;
    m_pAniso16xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso16xSampler"));
}