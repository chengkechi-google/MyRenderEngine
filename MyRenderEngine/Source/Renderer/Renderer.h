#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderBatch.h"
#include "GPUScene.h"
#include "RHI/RHI.h"
#include "Resource/Texture2D.h"
#include "Resource/Texture3D.h"
#include "Resource/TextureCube.h"
#include "Resource/indexBuffer.h"
#include "Resource/StructedBuffer.h"
#include "Resource/RawBuffer.h"
#include "Resource/TypedBuffer.h"
#include "EASTL/unique_ptr.h"
#include "Utils/linear_allocator.h"
#include "StagingBufferAllocator.h"

class ShaderCompiler;
class ShaderCache;
class PipelineStateCache;
class GPUScene;

enum class RendererOutput
{
    Default,
    Diffuse,
    Specular,
    WorldNormal,
    Roughness,
    Emissive,
    ShadingModel,
    CustomData,
    AO,
    DirectLighting,
    IndirectSpecular,
    IndirectDiffuse,
    PathTracing,
    Physics,

    Max
};

enum class TemporalSuperResolution
{
    None,
    FSR2,
    DLSS,
    XeSS,
    
    Max
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    bool CreateDevice(void* windowHandle, uint32_t windowWidth, uint32_t windowHeight);
    void RenderFrame();
    void WaitGPUFinished();

    uint64_t GetFrameID() const { return m_pDevice->GetFrameID(); }
    ShaderCompiler* GetShaderCompiler() const { return m_pShaderCompiler.get(); }
    ShaderCache* GetShaderCache() const { return m_pShaderCache.get(); }
    PipelineStateCache* GetPipelineStateCache() const { return m_pPipelineCache.get(); }
    RenderGraph* GetRenderGraph() const { return m_pRenderGraph.get(); }

    RendererOutput GetOutputType() const { return m_outputType; }   
    void SetOutputType(RendererOutput output) { m_outputType = output; }

    TemporalSuperResolution GetTemporalUpscaleMode() const { return m_upscaleMode; }
    void SetTemporalUpscaleMode(TemporalSuperResolution mode) { m_upscaleMode = mode; }
    float GetTemporalUpscaleRatio() const { return m_upscaleRatio; }
    void SetTemporalUpscaleRatio(float ratio);

    uint32_t GetDisplayWidth() const { return m_displayWidth; }
    uint32_t GetDispaltHeight() const { return m_displayHeight; }
    uint32_t GetRenderWidth() const { return m_renderWidth; }
    uint32_t GetRenderHeight() const { return m_renderHeight; }

    IRHIDevice* GetDevice() const { return m_pDevice.get(); }
    IRHISwapChain* GetSwapChain() const { return m_pSwapChain.get(); }
    IRHIShader* GetShader(const eastl::string& file, const eastl::string& entryPoint, RHIShaderType type, const eastl::vector<eastl::string>& defines = {}, RHIShaderCompilerFlags flags = 0);
    IRHIPipelineState* GetPipelineState(const RHIGraphicsPipelineDesc& desc, const eastl::string& name);
    IRHIPipelineState* GetPipelineState(const RHIMeshShaderPipelineDesc& desc, const eastl::string& name);
    IRHIPipelineState* GetPipelineState(const RHIComputePipelineDesc& desc, const eastl::string& name);
    void ReloadShaders();
    IRHIDescriptor* GetPointSampler() const { return m_pPointRepeatSampler.get(); }
    IRHIDescriptor* GetLinearSampler() const { return m_pBilinearRepeatSampler.get(); }

    IndexBuffer* CreateIndexBuffer(const void* pData, uint32_t stride, uint32_t indexCount, const eastl::string& name, RHIMemoryType memoryType = RHIMemoryType::GPUOnly);
    StructedBuffer* CreateStructedBuffer(const void* pData, uint32_t stride, uint32_t elementCount, const eastl::string& name, RHIMemoryType memoryType = RHIMemoryType::GPUOnly, bool uav = false);
    TypedBuffer* CreateTypedBuffer(const void* pData, RHIFormat format, uint32_t elementCount, const eastl::string& name, RHIMemoryType memoryType = RHIMemoryType::GPUOnly, bool uav = false);
    RawBuffer* CreateRawBuffer(const void* pData, uint32_t size, const eastl::string& name, RHIMemoryType memoryType = RHIMemoryType::GPUOnly, bool uav = false);

    Texture2D* CreateTexture2D(const eastl::string& file, bool srgb = true);
    Texture2D* CreateTexture2D(uint32_t width, uint32_t height, uint32_t levels, RHIFormat format, RHITextureUsageFlags flags, const eastl::string& name);
    Texture3D* CreateTexture3D(const eastl::string& file, bool srgb = true);
    Texture3D* CreateTexture3D(uint32_t width, uint32_t height, uint32_t depths, uint32_t levels, RHIFormat format, RHITextureUsageFlags flags, const eastl::string& name);
    TextureCube* CreateTextureCube(const eastl::string& file, bool srgb = true);
    TextureCube* CreateTextureCube(uint32_t width, uint32_t height, uint32_t levels, RHIFormat format, RHITextureUsageFlags flags, const eastl::string& name);

    void SaveTexture(const eastl::string& file, const void* pData, uint32_t width, uint32_t height, RHIFormat format);
    void SaveTexture(const eastl::string& file, IRHITexture* pTexture);

    IRHIBuffer* GetSceneStaticBuffer() const;
    OffsetAllocator::Allocation AllocateSceneStaticBuffer(const void* pData, uint32_t size);
    void FreeSceneStaticBuffer(OffsetAllocator::Allocation allocation);

    IRHIBuffer* GetAScenenimationBuffer() const;
    OffsetAllocator::Allocation AllocateSceneAnimationBuffer(uint32_t size);
    void FreeSceneAnimationBuffer(OffsetAllocator::Allocation allocation);

    uint32_t AllocateSceneConstant(const void* pData, uint32_t size);

    uint32_t AddInstance(const InstanceData& data, IRHIRayTracingBLAS* pBLAS, RHIRayTracingInstanceFlags flags);
    uint32_t GetInstanceCount() const { return m_pGPUScene->GetInstanceCount(); }

    uint32_t AddLocalLight(const LocalLightData& data);

    void RequestMouseHitTest(uint32_t x, uint32_t y);
    bool IsEnableMouseHitTest() const { return m_enableObjectIDRendering; }
    uint32_t GetMouseHitObjectID() const { return m_mouseHitObjectID; }

    void SetGPUDrivenStatsEnabled(bool value) { m_gpuDrivenStatsEnabled = value; }
    void SetShowMeshletsEnabled(bool value) { m_showMeshlets = value; }
    
    bool IsAsyncComputeEnabled() const { return m_enableAsyncCompute; }
    void SetAsyncComputeEnabled(bool value) { m_enableAsyncCompute = value; }
  
    void UploadTexture(IRHITexture* pTexture, const void* pData);
    void UploadBuffer(IRHIBuffer* pBuffer, uint32_t offset, const void* pData, uint32_t detaSize);
    void BuildRayTracingBLAS(IRHIRayTracingBLAS* pBLAS);
    void UpdateRayTracingBLAS(IRHIRayTracingBLAS* pBLAS, IRHIBuffer* vertexBuffer, uint32_t vertexBufferOffset);

    LinearAllocator* GetConstantAllocator() const { return m_pCBAllocator.get(); }
    RenderBatch& AddGPUDrivenBasePassBatch();
    RenderBatch& AddBasePassBatch() { return m_BaseBatchs.emplace_back(*m_pCBAllocator); }
    RenderBatch& AddForwardPassBatch() { return m_forwardPassBatchs.emplace_back(*m_pCBAllocator); }
    RenderBatch& AddVelocityPassBatch() { return m_velocityPassBatchs.emplace_back(*m_pCBAllocator); }
    RenderBatch& AddObjectIDPassBatch() { return m_idPassBatchs.emplace_back(*m_pCBAllocator); }
    RenderBatch& AddGUIPassBatch() { return m_guiBatch.emplace_back(*m_pCBAllocator); }
    ComputeBatch& AddAnimationBatch() { return m_animationBatchs.emplace_back(*m_pCBAllocator); }

    void SetupGlobalConstants(IRHICommandList* pCommandList);

    class HZBPass* GetHZBPass() const { return m_pHZBPass.get(); }
    class BasePassGPUDriven* GetBasePassGPUDriven() const { return m_pBasePassGPUDriven.get(); }

    bool IsHistoryTextureValid() const { return m_bHistoryValid; };
    RGHandle GetPrevSceneDepthHandle() const { return m_prevSceneDepthHandle; }
    RGHandle GetPrevSceneColorHandle() const { return m_prevSceneColorHandle; }
    RGHandle GetPrevNormalHandle() const { return m_prevSceneNormalHandle; }

    TypedBuffer* GetSPDCounterBuffer() const { return m_pSPDCounterBuffer.get(); };

private:
    void CreateCommonResources();
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

    void BeginFrame();
    void UploadResources();
    void Render();
    void BuildRenderGraph(RGHandle& outColor, RGHandle& outDepth);
    void EndFrame();

    void UpdateMipBias();

    // Render passes
    void BasePass(RGHandle& outColor, RGHandle& outDepth);
    void ComputeTestPass(RGHandle input, RGHandle& output);
    void GraphicsTestPass(RGHandle input, RGHandle& output);
    void FinalTestPass(RGHandle input1, RGHandle input2, RGHandle& output);
    void RenderBackBufferPass(IRHICommandList* pCommandList, RGHandle color, RGHandle depth);
    void CopyToBackBuffer(IRHICommandList* pCommandList, RGHandle color, RGHandle depth, bool needUpscaleDepth);    
    void CopyHistoryPass(RGHandle sceneDepth, RGHandle sceneColor, RGHandle sceneNormal);
    void BuildRayTracingAS(IRHICommandList* pGraphicsCommandList, IRHICommandList* pComputeCommandList);
    void ImportPrevFrameTextures();
   
private:
    // Render resource
    eastl::unique_ptr<IRHIDevice> m_pDevice;
    eastl::unique_ptr<IRHISwapChain> m_pSwapChain;
    eastl::unique_ptr<RenderGraph> m_pRenderGraph;
    eastl::unique_ptr<ShaderCompiler> m_pShaderCompiler;
    eastl::unique_ptr<ShaderCache> m_pShaderCache;
    eastl::unique_ptr<PipelineStateCache> m_pPipelineCache;
    eastl::unique_ptr<GPUScene> m_pGPUScene;

    RendererOutput m_outputType = RendererOutput::Default;
    TemporalSuperResolution m_upscaleMode = TemporalSuperResolution::None;

    uint32_t m_displayWidth;
    uint32_t m_displayHeight;
    uint32_t m_renderWidth;
    uint32_t m_renderHeight;
    float m_upscaleRatio = 1.0f;
    float m_mipBias = 0.0f;

    eastl::unique_ptr<LinearAllocator> m_pCBAllocator;

    uint64_t m_currentFrameFenceValue = 0;
    eastl::unique_ptr<IRHIFence> m_pFrameFence;
    uint64_t m_frameFenceValue[RHI_MAX_INFLIGHT_FRAMES] = {};
    eastl::unique_ptr<IRHICommandList> m_pCommandLists[RHI_MAX_INFLIGHT_FRAMES];

    eastl::unique_ptr<IRHIFence> m_pAsyncComputeFence;
    uint64_t m_currentAsyncComputeFenceValue = 0;
    eastl::unique_ptr<IRHICommandList> m_pComputeCommandLists[RHI_MAX_INFLIGHT_FRAMES];

    eastl::unique_ptr<IRHIFence> m_pUploadFence;
    uint64_t m_currentUploadFenceValue = 0;
    eastl::unique_ptr<IRHICommandList> m_pUploadCommandLists[RHI_MAX_INFLIGHT_FRAMES];

    eastl::unique_ptr<StagingBufferAllocator> m_pStagingBufferAllocator[RHI_MAX_INFLIGHT_FRAMES];

    struct TextureUpload
    {
        IRHITexture* m_pTexture;
        uint32_t m_mipLevel;
        uint32_t m_arraySlice;
        StagingBuffer m_stagingBuffer;
        uint32_t m_offset;
    };
    eastl::vector<TextureUpload> m_pendingTextureUploads;

    struct BufferUpload
    {
        IRHIBuffer* m_pBuffer;
        uint32_t m_offset;
        StagingBuffer m_stagingBuffer;
    };
    eastl::vector<BufferUpload> m_pendingBufferUploads;

    struct BLASUpdate
    {
        IRHIRayTracingBLAS *m_pBLAS;
        IRHIBuffer* m_pVertexBuffer;
        uint32_t m_vettexBufferOffset;
    };
    eastl::vector<BLASUpdate> m_pendingBLASUpdates;
    eastl::vector<IRHIRayTracingBLAS*> m_pendingBLASBuilds;

    eastl::unique_ptr<IRHIDescriptor> m_pAniso2xSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pAniso4xSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pAniso8xSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pAniso16xSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pPointRepeatSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pPointClampSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pBilinearRepeatSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pBilinearClampSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pBilinearBlackBoarderSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pBilinearWhiteBoarderSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pTrilinearRepeatSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pTrilinearClampSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pShadowSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pMinReductionSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pMaxReductionSampler;

    eastl::unique_ptr<Texture2D> m_pPrevSceneDepthTexture;
    eastl::unique_ptr<Texture2D> m_pPrevSceneNormalTexture;
    eastl::unique_ptr<Texture2D> m_pPrevSceneColorTexture;
    RGHandle m_prevSceneDepthHandle;
    RGHandle m_prevSceneNormalHandle;
    RGHandle m_prevSceneColorHandle;
    bool m_bHistoryValid = false;

    eastl::unique_ptr<TypedBuffer> m_pSPDCounterBuffer;

    bool m_gpuDrivenStatsEnabled = false;
    bool m_showMeshlets = false;
    bool m_enableAsyncCompute = false;

    bool m_enableObjectIDRendering = false;
    uint32_t m_mouseX = 0;
    uint32_t m_mouseY = 0;
    uint32_t m_mouseHitObjectID = UINT32_MAX;
    eastl::unique_ptr<IRHIBuffer> m_pObjectIDBuffer;
    uint32_t m_objectIDRowPitch = 0;

    eastl::vector<RenderBatch> m_BaseBatchs;             //< not mesh let
    eastl::vector<ComputeBatch> m_animationBatchs;
    eastl::vector<RenderBatch> m_forwardPassBatchs;
    eastl::vector<RenderBatch> m_velocityPassBatchs;
    eastl::vector<RenderBatch> m_idPassBatchs;
    eastl::vector<RenderBatch> m_guiBatch;

    IRHIPipelineState* m_pCopyColorPSO = nullptr;
    IRHIPipelineState* m_pCopyDepthPSO = nullptr;
    IRHIPipelineState* m_pComputeTestPSO = nullptr;
    IRHIPipelineState* m_pGraphicsTestPSO = nullptr;
    IRHIPipelineState* m_pFinalTestPassPSO = nullptr;

    eastl::unique_ptr<class HZBPass> m_pHZBPass;
    eastl::unique_ptr<class BasePassGPUDriven> m_pBasePassGPUDriven;
    eastl::unique_ptr<class LightingPasses> m_pLightingPasses;

    eastl::unique_ptr<class GPUDrivenStats> m_pGPUStats;
};