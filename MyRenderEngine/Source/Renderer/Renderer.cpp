#include "Renderer.h"
#include "TextureLoader.h"
#include "ShaderCompiler.h"
#include "ShaderCache.h"
#include "GPUDrivenStats.h"
#include "GPUScene.h"
#include "RenderPasses/HierarchicalDepthBufferPass.h"
#include "RenderPasses/BasePassGPUDriven.h"
#include "RenderPasses/Lighting/LightingPasses.h"
#include "PipelineCache.h"
#include "Core/Engine.h"
#include "Utils/profiler.h"
#include "Utils/log.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "lodepng/lodepng.h"

Renderer::Renderer()
{
    m_pShaderCache = eastl::make_unique<ShaderCache>(this);
    m_pShaderCompiler = eastl::make_unique<ShaderCompiler>(this);
    m_pPipelineCache = eastl::make_unique<PipelineStateCache>(this);
    m_pCBAllocator = eastl::make_unique<LinearAllocator>(8 * 1024 * 1024); // 8 MB for constant buffer

    Engine::GetInstance()->WindowResizeSignal.connect(&Renderer::OnWindowResize, this);
}

Renderer::~Renderer()
{
    WaitGPUFinished();
    if (m_pRenderGraph)
    {
        m_pRenderGraph->Clear();
    }

    Engine::GetInstance()->WindowResizeSignal.disconnect(this);
}

bool Renderer::CreateDevice(void* windowHandle, uint32_t windowWidth, uint32_t windowHeight)
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
        return false;
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


    CreateCommonResources();
    m_pRenderGraph = eastl::make_unique<RenderGraph>(this);   
    m_pGPUScene = eastl::make_unique<GPUScene>(this);
    m_pHZBPass = eastl::make_unique<HZBPass>(this);
    m_pBasePassGPUDriven = eastl::make_unique<BasePassGPUDriven>(this);
    m_pLightingPasses = eastl::make_unique<LightingPasses>(this);
    m_pGPUStats = eastl::make_unique<GPUDrivenStats>(this);

    return true; 
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

IRHIShader* Renderer::GetShader(const eastl::string& file, const eastl::string& entryPoint, RHIShaderType type, const eastl::vector<eastl::string>& defines, RHIShaderCompilerFlags flags /*= 0*/)
{
    return m_pShaderCache->GetShader(file, entryPoint, type, defines, flags);
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

RenderBatch& Renderer::AddGPUDrivenBasePassBatch()
{
    return m_pBasePassGPUDriven->AddBatch();
}

void Renderer::SetupGlobalConstants(IRHICommandList* pCommandList)
{
    World* pWorld = Engine::GetInstance()->GetWorld();
    Camera* pCamera = pWorld->GetCamera();

    bool enableJitter = false;
    pCamera->EnableJitter(enableJitter);

    RGHandle firstPhaseHABHandle = m_pHZBPass->Get1stPhaseCullingHZBMip(0);
    RGHandle secondPhaseHZBHandle = m_pHZBPass->Get2ndPhaseCullingHZBMip(0);
    RGHandle sceneHZBHandle = m_pHZBPass->GetSceneHZBMip(0);
    RGTexture* pFirstPhaseHZBTexture = m_pRenderGraph->GetTexture(firstPhaseHABHandle);
    RGTexture* pSecondPhaseHZBTexture = m_pRenderGraph->GetTexture(secondPhaseHZBHandle);
    RGTexture* pSceneHZBTexture = m_pRenderGraph->GetTexture(sceneHZBHandle);

    RGHandle occlusionCulledMeshletBufferHandle = m_pBasePassGPUDriven->GetSecondPhaseMeshletListBuffer();
    RGHandle occlusionCUlledMeshletCounterBufferHandle = m_pBasePassGPUDriven->GetSecondPhaseMeshletListCounterBuffer();
    RGBuffer* pOcclusionCulledMeshletBuffer = m_pRenderGraph->GetBuffer(occlusionCulledMeshletBufferHandle);
    RGBuffer* pOcclusionCulledMeshletCounterBuffer = m_pRenderGraph->GetBuffer(occlusionCUlledMeshletCounterBufferHandle);
    

    SceneConstant sceneCB;
    pCamera->SetupCameraCB(sceneCB.m_cameraCB);

    sceneCB.m_sceneConstantBufferSRV = m_pGPUScene->GetSceneConstantSRV()->GetHeapIndex();
    sceneCB.m_sceneStaticBufferSRV = m_pGPUScene->GetSceneStaticBufferSRV()->GetHeapIndex();
    sceneCB.m_sceneAnimationBufferSRV = m_pGPUScene->GetSceneAnimationBufferSRV()->GetHeapIndex();
    sceneCB.m_sceneAnimationBufferUAV = m_pGPUScene->GetSceneAnimationBufferUAV()->GetHeapIndex();
    sceneCB.m_instanceDataAddress = m_pGPUScene->GetInstanceDataAddress();
    sceneCB.m_sceneRayTracingTLAS = m_pGPUScene->GetRayTracingTLASSRV()->GetHeapIndex();
    sceneCB.m_showMeshlets = m_showMeshlets;
    sceneCB.m_secondPhaseMeshletsListUAV = RHI_INVALID_RESOURCE;//pOcclusionCulledMeshletBuffer->GetUAV()->GetHeapIndex();
    sceneCB.m_secondPhaseMeshletsCounterUAV = RHI_INVALID_RESOURCE;//pOcclusionCulledMeshletCounterBuffer->GetUAV()->GetHeapIndex();
    sceneCB.m_lightDir = float3(0.0f, -1.0f, 0.0f);
    sceneCB.m_lightColor = float3(1.0f, 1.0f, 1.0f);
    sceneCB.m_lightRadius = 1.0f;
    sceneCB.m_renderSize = uint2(m_renderWidth, m_renderHeight);
    sceneCB.m_rcpRenderSize = float2(1.0f/ m_renderWidth, 1.0f / m_renderHeight);
    sceneCB.m_displaySize = uint2(m_displayWidth, m_displayHeight);
    sceneCB.m_rcpDisplaySize = float2(1.0f / m_displayWidth, 1.0f / m_displayHeight);
    sceneCB.m_prevSceneColorSRV = m_pPrevSceneColorTexture->GetSRV()->GetHeapIndex();
    sceneCB.m_prevSceneDepthSRV = m_pPrevSceneDepthTexture->GetSRV()->GetHeapIndex();
    sceneCB.m_prevNormalSRV = m_pPrevSceneNormalTexture->GetSRV()->GetHeapIndex();
    sceneCB.m_hzbWidth = m_pHZBPass->GetHZBWidth();
    sceneCB.m_hzbHeight = m_pHZBPass->GetHZBHeight();
    sceneCB.m_firstPhaseCullingHZBSRV = pFirstPhaseHZBTexture->GetSRV()->GetHeapIndex();
    sceneCB.m_secondPhaseCullingHZBSRV = RHI_INVALID_RESOURCE;//pSecondPhaseHZBTexture->GetSRV()->GetHeapIndex();
    sceneCB.m_sceneHZBSRV = RHI_INVALID_RESOURCE;//pSceneHZBTexture->GetSRV()->GetHeapIndex();
    sceneCB.m_debugLineDrawCommandUAV = RHI_INVALID_RESOURCE;
    sceneCB.m_debugLineVertexBufferUAV = RHI_INVALID_RESOURCE;
    sceneCB.m_debugTextCounterBufferUAV = RHI_INVALID_RESOURCE;
    sceneCB.m_debugTextBufferUAV = RHI_INVALID_RESOURCE;
    sceneCB.m_debugFontCharBufferSRV = RHI_INVALID_RESOURCE;
    sceneCB.m_enableStats = m_gpuDrivenStatsEnabled;
    sceneCB.m_statsBufferUAV = m_pGPUStats->GetStatsBufferUAV()->GetHeapIndex();
    sceneCB.m_minReductionSampler = m_pMinReductionSampler->GetHeapIndex();
    sceneCB.m_maxReductionSampler = m_pMaxReductionSampler->GetHeapIndex();
    sceneCB.m_pointRepeatSampler = m_pPointRepeatSampler->GetHeapIndex();
    sceneCB.m_pointClampSampler = m_pPointClampSampler->GetHeapIndex();
    sceneCB.m_bilinearRepeatSampler = m_pBilinearRepeatSampler->GetHeapIndex();
    sceneCB.m_bilinearClampSampler = m_pBilinearClampSampler->GetHeapIndex();
    sceneCB.m_bilinearBlackBorderSampler = m_pBilinearBlackBoarderSampler->GetHeapIndex();
    sceneCB.m_bilinearWhiteBorderSampler = m_pBilinearWhiteBoarderSampler->GetHeapIndex();
    sceneCB.m_trilinearRepeatSampler = m_pTrilinearRepeatSampler->GetHeapIndex();
    sceneCB.m_trilinearClampSampler = m_pTrilinearClampSampler->GetHeapIndex();
    sceneCB.m_aniso2xSampler = m_pAniso2xSampler->GetHeapIndex();
    sceneCB.m_aniso4xSampler = m_pAniso4xSampler->GetHeapIndex();
    sceneCB.m_aniso8xSampler = m_pAniso8xSampler->GetHeapIndex();
    sceneCB.m_aniso16xSampler = m_pAniso16xSampler->GetHeapIndex();
    sceneCB.m_skyCubeTexture = RHI_INVALID_RESOURCE;
    sceneCB.m_skyDiffuseIBLTexture = RHI_INVALID_RESOURCE;
    sceneCB.m_skySpecularIBLTexture = RHI_INVALID_RESOURCE;
    sceneCB.m_preIntegratedGFTexture = RHI_INVALID_RESOURCE;
    sceneCB.m_blueNoiseTexture = RHI_INVALID_RESOURCE;
    sceneCB.m_sheenETexture = RHI_INVALID_RESOURCE;
    sceneCB.m_tonyMcMapfaceTexture = RHI_INVALID_RESOURCE;
    sceneCB.m_scalarSTBN = RHI_INVALID_RESOURCE;
    sceneCB.m_vec2STBN = RHI_INVALID_RESOURCE;
    sceneCB.m_vec3STBN = RHI_INVALID_RESOURCE;
    sceneCB.m_frameTime = Engine::GetInstance()->GetFrameDeltaTime();
    sceneCB.m_frameIndex = (uint32_t) GetFrameID();
    sceneCB.m_mipBias = m_mipBias;
    sceneCB.m_marschnerTextureM = RHI_INVALID_RESOURCE;
    sceneCB.m_marschnerTextureN = RHI_INVALID_RESOURCE;

    if (pCommandList->GetQueue() == RHICommandQueue::Graphics)
    {
        pCommandList->SetGraphicsConstants(2, &sceneCB, sizeof(sceneCB));
    }

    pCommandList->SetComputeConstants(2, &sceneCB, sizeof(sceneCB));
}

void Renderer::CreateCommonResources()
{
    RHISamplerDesc desc;
    m_pPointRepeatSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pPointRepeatSampler"));

    desc.m_minFilter = RHIFilter::Linear;
    desc.m_magFilter = RHIFilter::Linear;
    m_pBilinearRepeatSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pBilinearRepeatSampler"));

    desc.m_mipFilter = RHIFilter::Linear;
    m_pTrilinearRepeatSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pTrilinearRepeatSampler"));

    desc.m_minFilter = RHIFilter::Point;
    desc.m_magFilter = RHIFilter::Point;
    desc.m_mipFilter = RHIFilter::Point;
    desc.m_addressU = RHISamplerAddressMode::ClampToEdge;
    desc.m_addressV = RHISamplerAddressMode::ClampToEdge;
    desc.m_addressW = RHISamplerAddressMode::ClampToEdge;
    m_pPointClampSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pPointClampSampler"));

    desc.m_minFilter = RHIFilter::Linear;
    desc.m_magFilter = RHIFilter::Linear;
    m_pBilinearClampSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pBilinearClampSampler"));

    desc.m_mipFilter = RHIFilter::Linear;
    m_pTrilinearClampSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pTrilinearClampSampler"));

    desc.m_mipFilter = RHIFilter::Point;
    desc.m_addressU = RHISamplerAddressMode::ClampToBorder;
    desc.m_addressV = RHISamplerAddressMode::ClampToBorder;
    desc.m_addressW = RHISamplerAddressMode::ClampToBorder;
    m_pBilinearBlackBoarderSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pBilinearBlackBoarderSampler"));

    desc.m_borderColor[0] = desc.m_borderColor[1] = desc.m_borderColor[2] = desc.m_borderColor[3] = 1.0f;
    m_pBilinearWhiteBoarderSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pBilinearWhiteBoarderSampler"));

    desc.m_minFilter = RHIFilter::Linear;
    desc.m_magFilter = RHIFilter::Linear;
    desc.m_mipFilter = RHIFilter::Linear;
    desc.m_addressU = RHISamplerAddressMode::Repeat;
    desc.m_addressV = RHISamplerAddressMode::Repeat;
    desc.m_addressW = RHISamplerAddressMode::Repeat;
    desc.m_enableAnisotropy = true;
    desc.m_maxAnisotropy = 2.0f;
    m_pAniso2xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso2xSampler"));

    desc.m_maxAnisotropy = 4.0f;
    m_pAniso4xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso4xSampler"));

    desc.m_maxAnisotropy = 8.0f;
    m_pAniso8xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso8xSampler"));

    desc.m_maxAnisotropy = 16.0f;
    m_pAniso16xSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pAniso16xSampler"));

    desc.m_enableAnisotropy = false;
    desc.m_maxAnisotropy = 1.0f;

    desc.m_minFilter = RHIFilter::Linear;
    desc.m_magFilter = RHIFilter::Linear;
    desc.m_mipFilter = RHIFilter::Point;
    desc.m_addressU = RHISamplerAddressMode::ClampToEdge;
    desc.m_addressV = RHISamplerAddressMode::ClampToEdge;
    desc.m_addressW = RHISamplerAddressMode::ClampToEdge;
    desc.m_reductionMode = RHISamplerReductionMode::Min;
    m_pMinReductionSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pMinReductionSampler"));

    desc.m_reductionMode = RHISamplerReductionMode::Max;
    m_pMaxReductionSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pMaxReductionSampler"));

    desc.m_reductionMode = RHISamplerReductionMode::Compare;
    desc.m_compareFunc = RHICompareFunc::LessEqual;
    m_pShadowSampler.reset(m_pDevice->CreateSampler(desc, "Renderer::m_pShadowSampler"));  

    m_pSPDCounterBuffer.reset(CreateTypedBuffer(nullptr, RHIFormat::R32UI, 1, "Renderer::m_pSPDCounterBuffer", RHIMemoryType::GPUOnly, true));

    // Create global shader
    RHIGraphicsPipelineDesc psoDesc;
    psoDesc.m_pVS = GetShader("Copy.hlsl", "vs_main", RHIShaderType::VS);
    psoDesc.m_pPS = GetShader("Copy.hlsl", "ps_main", RHIShaderType::PS);
    psoDesc.m_depthStencilState.m_depthWrite = false;
    psoDesc.m_rtFormat[0] = m_pSwapChain->GetDesc().m_format;
    psoDesc.m_depthStencilFromat = RHIFormat::D32F;
    m_pCopyColorPSO = GetPipelineState(psoDesc, "Copy PSO");
    
    psoDesc.m_pPS = GetShader("Copy.hlsl", "ps_main_graphics_test", RHIShaderType::PS);
    psoDesc.m_rtFormat[0] = m_pSwapChain->GetDesc().m_format;
    m_pGraphicsTestPSO = GetPipelineState(psoDesc, "Graphics Test");

    psoDesc.m_pVS = GetShader("FinalTestPass.hlsl", "vs_main", RHIShaderType::VS);
    psoDesc.m_pPS = GetShader("FinalTestPass.hlsl", "ps_main", RHIShaderType::PS);
    psoDesc.m_rtFormat[0] = m_pSwapChain->GetDesc().m_format;
    m_pFinalTestPassPSO = GetPipelineState(psoDesc, "Pass Test Final");

    RHIComputePipelineDesc computePSODesc;
    computePSODesc.m_pCS = GetShader("ComputeTest.hlsl", "cs_main", RHIShaderType::CS);
    m_pComputeTestPSO = GetPipelineState(computePSODesc, "Compute Test");

    computePSODesc.m_pCS = GetShader("Copy.hlsl", "cs_copy_depth_main", RHIShaderType::CS);
    m_pCopyDepthPSO = GetPipelineState(computePSODesc, "Copy Depth PSO");
}

void Renderer::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    WaitGPUFinished();
    if (m_pSwapChain->GetDesc().m_windowHandle == window)
    {
        m_pSwapChain->Resize(width, height);
        
        m_displayWidth = width;
        m_displayHeight = height;
        m_renderWidth = (uint32_t)((float)m_displayWidth / m_upscaleRatio);
        m_renderHeight = (uint32_t)((float)m_displayHeight / m_upscaleRatio);
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


    if (m_pDevice->GetFrameID() == 0)
    {
        pCommandList->BufferBarrier(m_pSPDCounterBuffer->GetBuffer(), RHIAccessBit::RHIAccessComputeShaderUAV, RHIAccessBit::RHIAccessCopyDst);
        pCommandList->WriteBuffer(m_pSPDCounterBuffer->GetBuffer(), 0, 0);
        pCommandList->BufferBarrier(m_pSPDCounterBuffer->GetBuffer(), RHIAccessBit::RHIAccessCopyDst, RHIAccessComputeShaderUAV);
    }

    m_pRenderGraph->Clear();
    m_pGPUScene->Update();

    // ImportPrevFrameTextures();

    RGHandle outputColorHandle, outputDepthHandle;
    BuildRenderGraph(outputColorHandle, outputDepthHandle);


    // m_pGPUDebugLine->Clear(pCommandList);
    // m_pGPUDebugPring->Clear(pCommandList);
    m_pGPUStats->Clear(pCommandList);

    SetupGlobalConstants(pCommandList);
    //FlushComputePass(pCommandList);
    BuildRayTracingAS(pCommandList, pComputeCommandList);

    // m_pSkyyCubeMap->Update();

    World* pWorld = Engine::GetInstance()->GetWorld();
    Camera* pCamera = pWorld->GetCamera();
    pCamera->DrawViewFrustum(pCommandList);

    m_pRenderGraph->Execute(this, pCommandList, pComputeCommandList);

    RenderBackBufferPass(pCommandList, outputColorHandle, outputDepthHandle);
    //Engine::GetInstance()->GetGUI()->Render(pCommandList);

}

void Renderer::BuildRenderGraph(RGHandle& outColor, RGHandle& outDepth)
{
    m_pRenderGraph->Clear();
    ImportPrevFrameTextures();
    
    //RGHandle outSceneColor, outSceneDepth;
    //RGHandle output0, output1, output2;
    //BasePass(outSceneColor, outSceneDepth);
    //GraphicsTestPass(outSceneColor, output0);
    //ComputeTestPass(outSceneColor, output1);
    //FinalTestPass(output0, output1, output2);

    m_pHZBPass->Generate1stPhaseCullingHZB(m_pRenderGraph.get());
    m_pBasePassGPUDriven->Render1stPhase(m_pRenderGraph.get());

    m_pHZBPass->Generate2ndPhaseCullingHZB(m_pRenderGraph.get(), m_pBasePassGPUDriven->GetDepthRT());
    m_pBasePassGPUDriven->Render2ndPhase(m_pRenderGraph.get());

    RGHandle sceneDepthRT = m_pBasePassGPUDriven->GetDepthRT();
    RGHandle linearRT;
    RGHandle velocityRT;
    RGHandle gtao = m_pLightingPasses->AddPass(m_pRenderGraph.get(), sceneDepthRT, linearRT, velocityRT, m_renderWidth, m_renderHeight);
    
    CopyHistoryPass(m_pBasePassGPUDriven->GetDepthRT(), m_pBasePassGPUDriven->GetDiffuseRT(), m_pBasePassGPUDriven->GetNormalRT());



    RGHandle sceneDiffuseRT = gtao;//m_pBasePassGPUDriven->GetNormalRT();
    RGHandle showCulledDiffuseRT = m_pBasePassGPUDriven->GetCulledObjectsDiffuseRT();
    
    
    //m_pRenderGraph->Present(outSceneColor, RHIAccessBit::RHIAccessPixelShaderSRV);
    //m_pRenderGraph->Present(output1, RHIAccessBit::RHIAccessPixelShaderSRV);
    //m_pRenderGraph->Present(output2, RHIAccessBit::RHIAccessPixelShaderSRV);
    outColor = sceneDiffuseRT;
    outDepth = sceneDepthRT;
    //m_pRenderGraph->Present(outDepth, RHIAccessBit::RHIAccessDSVReadOnly);
    m_pRenderGraph->Present(sceneDiffuseRT, RHIAccessBit::RHIAccessPixelShaderSRV);
    m_pRenderGraph->Present(outDepth, RHIAccessBit::RHIAccessPixelShaderSRV);
    //m_pRenderGraph->Present(showCulledDiffuseRT, RHIAccessBit::RHIAccessPixelShaderSRV);

    m_pRenderGraph->Compile();
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
    pCommandList->Present(m_pSwapChain.get());
    pCommandList->Signal(m_pFrameFence.get(), m_currentFrameFenceValue);
    pCommandList->Submit();
    pCommandList->EndProfiling(); 
    /* {
        CPU_EVENT("Render", "IRHISwapchain::Present");
        m_pSwapChain->Present();
    }*/

    m_pStagingBufferAllocator[frameIndex]->Reset();
    m_pCBAllocator->Reset();
    m_pGPUScene->ResetFrameData();

    m_BaseBatchs.clear();
    m_animationBatchs.clear();
    m_velocityPassBatchs.clear();
    m_idPassBatchs.clear();

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

void Renderer::BasePass(RGHandle& outColor, RGHandle& outDepth)
{
    struct BasePassData
    {
        RGHandle outSceneColorRT;
        RGHandle outSceneDepthRT;
    };

    auto basePass = m_pRenderGraph->AddPass<BasePassData>("Base Pass", RenderPassType::Graphics,
        [&](BasePassData& data, RGBuilder& builder)
        {
            RGTexture::Desc desc;
            desc.m_width = m_renderWidth;
            desc.m_height = m_renderHeight;
            desc.m_format = RHIFormat::RGBA8SRGB;
            desc.m_frameID = GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;

            data.outSceneColorRT = builder.Create<RGTexture>(desc, "SceneColor RT");
            data.outSceneColorRT = builder.WriteColor(0, data.outSceneColorRT, 0, RHIRenderPassLoadOp::Clear, float4(1.0f, 1.0f, 0.0f, 1.0f));

            desc.m_format = RHIFormat::D32F;
            data.outSceneDepthRT = builder.Create<RGTexture>(desc, "SceneDepth RT");
            data.outSceneDepthRT = builder.WriteDepth(data.outSceneDepthRT, 0, RHIRenderPassLoadOp::Clear, RHIRenderPassLoadOp::Clear);
        },
        [&](const BasePassData& data, IRHICommandList* pCommandList)
        {
            for (size_t i = 0; i < m_BaseBatchs.size(); ++i)
            {
                DrawBatch(pCommandList, m_BaseBatchs[i]);
            }
        });

    outColor = basePass->outSceneColorRT;
    outDepth = basePass->outSceneDepthRT;
}

void Renderer::ComputeTestPass(RGHandle input, RGHandle& output)
{
    struct ComputeTestPassData
    {
        RGHandle input;
        RGHandle output;
    };

    auto computeTestPass = m_pRenderGraph->AddPass<ComputeTestPassData>("Compute Test Pass", RenderPassType::AsyncCompute,
        [&](ComputeTestPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);

            RGTexture::Desc desc;
            desc.m_width = m_renderWidth;
            desc.m_height = m_renderHeight;
            desc.m_format = RHIFormat::RGBA8UNORM;
            desc.m_frameID = GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;

            data.output = builder.Create<RGTexture>(desc, "Compute Test Pass output texture");
            data.output = builder.Write(data.output);
        },

        [=](const ComputeTestPassData& data, IRHICommandList* pCommandList)
        {
            RGTexture* pInput = m_pRenderGraph->GetTexture(data.input);
            RGTexture* pOutput = m_pRenderGraph->GetTexture(data.output);

            
            struct CB
            {
                uint input;
                uint output;
                uint samplerState;
                uint padding;
                float2 pixelSize;
            };

            CB constants;
            constants.input = pInput->GetSRV()->GetHeapIndex();
            constants.output = pOutput->GetSRV()->GetHeapIndex();
            constants.samplerState = m_pPointClampSampler.get()->GetHeapIndex();
            constants.pixelSize = float2(1.0f / m_renderWidth, 1.0f / m_renderHeight);
            pCommandList->SetComputeConstants(0, &constants, sizeof(constants));

            pCommandList->SetPipelineState(m_pComputeTestPSO);

            pCommandList->Dispatch(DivideRoundingUp(m_renderWidth, 8), DivideRoundingUp(m_renderHeight, 8), 1);
        });

    output = computeTestPass->output;
}

void Renderer::GraphicsTestPass(RGHandle input, RGHandle& output)
{
    struct GraphicsPassTestData
    {
        RGHandle input;
        RGHandle output;
    };

    auto graphicsTestPass = m_pRenderGraph->AddPass<GraphicsPassTestData>("Graphics Test Pass", RenderPassType::Graphics,
        [&](GraphicsPassTestData& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);
    

            RGTexture::Desc desc;
            desc.m_width = m_renderWidth;
            desc.m_height = m_renderHeight;
            desc.m_format = RHIFormat::RGBA8SRGB;
            desc.m_frameID = GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;

            data.output = builder.Create<RGTexture>(desc, "Graphics Pass Test output texture");
            data.output = builder.WriteColor(0, data.output, 0, RHIRenderPassLoadOp::Clear, float4(0.0f, 0.0f, 0.0f, 1.0f));

        },
        [=](const GraphicsPassTestData& data, IRHICommandList* pCommandList)
        {
            RGTexture* pInput = m_pRenderGraph->GetTexture(data.input);
            RGTexture* pOutput = m_pRenderGraph->GetTexture(data.output);

            uint32_t cb[3] = {pInput->GetSRV()->GetHeapIndex(), m_pPointClampSampler->GetHeapIndex(), 0};
            pCommandList->SetGraphicsConstants(0, cb, sizeof(cb));
            pCommandList->SetPipelineState(m_pGraphicsTestPSO);
            pCommandList->Draw(3);
        });

    output = graphicsTestPass->output;
}

void Renderer::FinalTestPass(RGHandle input1, RGHandle input2, RGHandle& output)
{
    struct FinalTestPassData
    {
        RGHandle input1;
        RGHandle input2;
        RGHandle output;
    };

    auto finalTestPass = m_pRenderGraph->AddPass<FinalTestPassData>("Final Test Pass", RenderPassType::Graphics, 
        [&](FinalTestPassData& data, RGBuilder& builder)
        {
            data.input1 = builder.Read(input1);
            data.input2 = builder.Read(input2);

            RGTexture::Desc desc;
            desc.m_width = m_renderWidth;
            desc.m_height = m_renderHeight;
            desc.m_format = RHIFormat::RGBA8SRGB;
            desc.m_frameID = GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;

            data.output = builder.Create<RGTexture>(desc, "Final Test Pass output");
            data.output = builder.WriteColor(0, data.output, 0, RHIRenderPassLoadOp::Load);
        },
        [=](const FinalTestPassData& data, IRHICommandList* pCommandList)
        {
            RGTexture* pInputTexture1 = m_pRenderGraph->GetTexture(data.input1);
            RGTexture* pInputTexture2 = m_pRenderGraph->GetTexture(data.input2);

            uint cb[3] = {pInputTexture1->GetSRV()->GetHeapIndex(), pInputTexture2->GetSRV()->GetHeapIndex(), m_pPointClampSampler->GetHeapIndex()};
            pCommandList->SetGraphicsConstants(0, cb, sizeof(cb));
            pCommandList->SetPipelineState(m_pFinalTestPassPSO);
            pCommandList->Draw(3);
         });

    output = finalTestPass->output;
}

void Renderer::RenderBackBufferPass(IRHICommandList* pCommandList, RGHandle color, RGHandle depth)
{
    GPU_EVENT(pCommandList, "BackBuffer Pass");

    m_pSwapChain->AcquireNextBackBuffer();
    pCommandList->TextureBarrier(m_pSwapChain->GetBackBuffer(), 0, RHIAccessBit::RHIAccessPresent, RHIAccessBit::RHIAccessRTV);
    //m_pRenderGraph->GetTexture(m_prevSceneDepthHandle)->Barrier(pCommandList, 0, RHIAccessBit::RHIAccessMaskUAV, RHIAccessBit::RHIAccessMaskSRV);
    pCommandList->TextureBarrier(m_pPrevSceneDepthTexture->GetTexture(), 0, RHIAccessBit::RHIAccessMaskUAV, RHIAccessMaskSRV);
    //RGTexture* pDepth = m_pRenderGraph->GetTexture(depth);

    RHIRenderPassDesc renderPass;
    renderPass.m_color[0].m_pTexture = m_pSwapChain->GetBackBuffer();
    renderPass.m_color[0].m_loadOp = RHIRenderPassLoadOp::DontCare;
    renderPass.m_depth.m_pTexture = nullptr;//pDepth->GetTexture();
    renderPass.m_depth.m_loadOp = RHIRenderPassLoadOp::DontCare;
    renderPass.m_depth.m_stencilLoadOp = RHIRenderPassLoadOp::DontCare;
    renderPass.m_depth.m_storeOp = RHIRenderPassStoreOp::DontCare;
    renderPass.m_depth.m_stencilStoreOp = RHIRenderPassStoreOp::DontCare;
    pCommandList->BeginRenderPass(renderPass);
  
    CopyToBackBuffer(pCommandList, color, depth, false);
    Engine::GetInstance()->GetGUI()->Render(pCommandList);
    pCommandList->EndRenderPass();
    pCommandList->TextureBarrier(m_pSwapChain->GetBackBuffer(), 0, RHIAccessBit::RHIAccessRTV, RHIAccessBit::RHIAccessPresent);
    pCommandList->SetPipelineState(m_pComputeTestPSO);

}

void Renderer::CopyToBackBuffer(IRHICommandList* pCommandList, RGHandle color, RGHandle depth, bool needUpscaleDepth)
{
    GPU_EVENT(pCommandList, "CopyToBackBuffer");
    
    RGTexture* pColorRT = m_pRenderGraph->GetTexture(color);
    RGTexture* pDepthRT = m_pRenderGraph->GetTexture(depth);
    
    uint32_t constants[3] = { pColorRT->GetSRV()->GetHeapIndex(), /*m_pPrevSceneDepthTexture->GetSRV()->GetHeapIndex(),*/ /*pDepthRT->GetSRV()->GetHeapIndex(),*/ m_pPointClampSampler->GetHeapIndex(), 0};
    pCommandList->SetGraphicsConstants(0, constants, sizeof(constants));
    pCommandList->SetPipelineState(m_pCopyColorPSO);
    pCommandList->Draw(3);
}

void Renderer::CopyHistoryPass(RGHandle sceneDepth, RGHandle sceneColor, RGHandle sceneNormal)
{
    struct CopyDepthPassData
    {
        RGHandle m_srcSceneDepthTexture;
        RGHandle m_dstSceneDepthTexture;
    };

    m_pRenderGraph->AddPass<CopyDepthPassData>("Copy History Depth Texture", RenderPassType::Compute,
        [&](CopyDepthPassData& data, RGBuilder& builder)
        {
            data.m_srcSceneDepthTexture = builder.Read(sceneDepth);
            data.m_dstSceneDepthTexture = builder.Write(m_prevSceneDepthHandle);
            builder.SkipCulling();
        },
        [&](const CopyDepthPassData& data, IRHICommandList* pCommandList)
        {
            RGTexture* pSrcSceneDepthTexture = m_pRenderGraph->GetTexture(data.m_srcSceneDepthTexture);
            uint32_t cb[2] = {pSrcSceneDepthTexture->GetSRV()->GetHeapIndex(), m_pPrevSceneDepthTexture->GetUAV()->GetHeapIndex()};

            pCommandList->SetPipelineState(m_pCopyDepthPSO);
            pCommandList->SetComputeConstants(0, cb, sizeof(cb));
            pCommandList->Dispatch(DivideRoundingUp(m_renderWidth, 8), DivideRoundingUp(m_renderHeight, 8), 1);
        });

    struct CopyPassData
    {
        RGHandle m_srcNormalTexture;
        RGHandle m_dstNormalTexture;

        RGHandle m_srcColorTexture;
        RGHandle m_dstColorTexture;
    };

    /*m_pRenderGraph->AddPass<CopyPassData>("Copy History Textures", RenderPassType::Copy,
        [&](CopyPassData& data, RGBuilder& builder)
        {
            data.m_srcNormalTexture = builder.Read(sceneNormal);
            data.m_dstNormalTexture = builder.Write(m_prevSceneNormalHandle);
    
            data.m_srcColorTexture = builder.Read(sceneColor);
            data.m_dstColorTexture = builder.Write(m_prevSceneColorHandle);

            builder.SkipCulling();
        },
        [&](const CopyPassData& data, IRHICommandList* pCommandList)
        {
            RGTexture* pSrcNormalTexture = m_pRenderGraph->GetTexture(data.m_srcNormalTexture);
            RGTexture* pSrcColorTexture = m_pRenderGraph->GetTexture(data.m_srcColorTexture);

            pCommandList->CopyTexture(m_pPrevSceneNormalTexture->GetTexture(), 0, 0, pSrcNormalTexture->GetTexture(), 0, 0);
            //pCommandList->CopyTexture(m_pPrevSceneColorTexture->GetTexture(), 0, 0, pSrcColorTexture->GetTexture(), 0, 0);
        });*/
}


void Renderer::BuildRayTracingAS(IRHICommandList* pGraphicsCommandList, IRHICommandList* pComputeCommandList)
{
    if (m_enableAsyncCompute)
    {
        pGraphicsCommandList->End();
        pGraphicsCommandList->Signal(m_pAsyncComputeFence.get(), ++ m_currentAsyncComputeFenceValue);
        pGraphicsCommandList->Submit();

        pGraphicsCommandList->Begin();
        SetupGlobalConstants(pGraphicsCommandList);
        pComputeCommandList->Wait(m_pAsyncComputeFence.get(), m_currentAsyncComputeFenceValue);
    }

    {
        IRHICommandList* pCommandList = m_enableAsyncCompute ? pComputeCommandList : pGraphicsCommandList;
        GPU_EVENT(pCommandList, "BuildRayTracingAS");

        if (!m_pendingBLASBuilds.empty())
        {
            GPU_EVENT(pCommandList, "BuildAS");
            
            for (size_t i = 0; i < m_pendingBLASBuilds.size(); ++i)
            {
                pCommandList->BuildRayTracingBLAS(m_pendingBLASBuilds[i]);
            }
            m_pendingBLASBuilds.clear();
            pCommandList->GlobalBarrier(RHIAccessBit::RHIAccessMaskAS, RHIAccessBit::RHIAccessMaskAS);
        }

        if (!m_pendingBLASUpdates.empty())
        {
            GPU_EVENT(pCommandList, "UpdateBLAS");
            
            for (size_t i = 0; i < m_pendingBLASUpdates.size(); ++i)
            {
                pCommandList->UpdateRayTracingBLAS(m_pendingBLASUpdates[i].m_pBLAS, m_pendingBLASUpdates[i].m_pVertexBuffer, m_pendingBLASUpdates[i].m_vettexBufferOffset);
            }
            m_pendingBLASUpdates.clear();
            pCommandList->GlobalBarrier(RHIAccessBit::RHIAccessMaskAS, RHIAccessBit::RHIAccessMaskAS);
            
        }

        m_pGPUScene->BuildRayTracingAS(pCommandList);
    }
}

void Renderer::ImportPrevFrameTextures()
{
    if (m_pPrevSceneDepthTexture == nullptr ||
        m_pPrevSceneDepthTexture->GetTexture()->GetDesc().m_width != m_renderWidth ||
        m_pPrevSceneDepthTexture->GetTexture()->GetDesc().m_height != m_renderHeight)
    {
        m_pPrevSceneDepthTexture.reset(CreateTexture2D(m_renderWidth, m_renderHeight, 1, RHIFormat::R32F, RHITextureUsageBit::RHITextureUsageUnorderedAccess, "Pre SceneDepth"));
        m_pPrevSceneNormalTexture.reset(CreateTexture2D(m_renderWidth, m_renderHeight, 1, RHIFormat::RGBA8UNORM, RHITextureUsageBit::RHITextureUsageUnorderedAccess, "Prev SceneNormal"));
        m_pPrevSceneColorTexture.reset(CreateTexture2D(m_renderWidth, m_renderHeight, 1, RHIFormat::RGBA16F, RHITextureUsageBit::RHITextureUsageUnorderedAccess, "Prev SceneColor"));
        m_bHistoryValid = false;
    }
    else
    {
        m_bHistoryValid = true;
    }

    m_prevSceneDepthHandle = m_pRenderGraph->Import(m_pPrevSceneDepthTexture->GetTexture(), m_bHistoryValid?  RHIAccessBit::RHIAccessPixelShaderSRV : RHIAccessBit::RHIAccessComputeShaderUAV);
    m_prevSceneNormalHandle = m_pRenderGraph->Import(m_pPrevSceneNormalTexture->GetTexture(), m_bHistoryValid ? RHIAccessBit::RHIAccessCopyDst : RHIAccessBit::RHIAccessComputeShaderUAV);
    m_prevSceneColorHandle = m_pRenderGraph->Import(m_pPrevSceneColorTexture->GetTexture(), m_bHistoryValid ? RHIAccessBit::RHIAccessCopyDst : RHIAccessBit::RHIAccessComputeShaderUAV);

    if (!m_bHistoryValid)
    {
        struct ClearHistoryPassData
        {
            RGHandle m_linearDepth;
            RGHandle m_normal;
            RGHandle m_color;
        };

        auto clearPass = m_pRenderGraph->AddPass<ClearHistoryPassData>("Clear History Textures", RenderPassType::Compute,
            [&](ClearHistoryPassData& data, RGBuilder& builder)
            {
                data.m_linearDepth = builder.Write(m_prevSceneDepthHandle);
                data.m_normal = builder.Write(m_prevSceneNormalHandle);
                data.m_color = builder.Write(m_prevSceneColorHandle);
            },
            [=](const ClearHistoryPassData& data, IRHICommandList* pCommandList)
            {
                float clearValue[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                pCommandList->ClearUAV(m_pPrevSceneDepthTexture->GetTexture(), m_pPrevSceneDepthTexture->GetUAV(), clearValue);
                pCommandList->ClearUAV(m_pPrevSceneNormalTexture->GetTexture(), m_pPrevSceneNormalTexture->GetUAV(), clearValue);
                pCommandList->ClearUAV(m_pPrevSceneColorTexture->GetTexture(), m_pPrevSceneColorTexture->GetUAV(), clearValue);
            });

        m_prevSceneDepthHandle = clearPass->m_linearDepth;
        m_prevSceneNormalHandle = clearPass->m_normal;
        m_prevSceneColorHandle = clearPass->m_color;
    }
}