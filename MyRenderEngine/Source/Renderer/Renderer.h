#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RHI/RHI.h"
#include "EASTL/unique_ptr.h"
#include "Utils/linear_allocator.h"
#include "StagingBufferAllocator.h"

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void CreateDevice(void* windowHandle, uint32_t windowWidth, uint32_t windowHeight);
    void RenderFrame();
    void WaitGPUFinished();

    uint64_t GetFrameID() const { return m_pDevice->GetFrameID(); }

    uint32_t GetDisplayWidth() const { return m_displayWidth; }
    uint32_t GetDispaltHeight() const { return m_displayHeight; }
    uint32_t GetRenderWidth() const { return m_renderWidth; }
    uint32_t GetRenderHeight() const { return m_renderHeight; }

    IRHIDevice* GetDevice() const { return m_pDevice.get(); }
    IRHISwapChain* GetSwapChain() const { return m_pSwapChain.get(); }
    IRHIShader* GetShader(const eastl::string& file, const eastl::string& entryPoint, const eastl::string& profile, const eastl::vector<eastl::string>& defines, RHIShaderCompilerFlags flags = 0);


    bool IsAsyncComputeEnabled() const {return m_enableAsyncCompute; }
    void SetAsyncComputeEnabled(bool value) { m_enableAsyncCompute = value; }

    void SetupGlobalConstants(IRHICommandList* pCommandList);

private:
    void OnWindowResize(void* window, uint32_t width, uint32_t height);

    void BeginFrame();
    void UploadResources();
    void Render();
    void BuildRenderGraph(RGHandle& outColor, RGHandle& outDepth);
    void EndFrame();

    

private:
    bool m_enableAsyncCompute = false;

    // Render resource
    eastl::unique_ptr<IRHIDevice> m_pDevice;
    eastl::unique_ptr<IRHISwapChain> m_pSwapChain;
    eastl::unique_ptr<RenderGraph> m_pRenderGraph;
   // eastl::unique_ptr<class ShaderCompiler> m_pShaderCompiler;
   // eastl::unique_ptr<class ShaderCache> m_pShaderCache;
   // eastl::unique_ptr<class PipelineStateCache> m_pPipelineCache;
   // eastl::unique_ptr<class GPUScene> m_pGPUScene;

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
    eastl::unique_ptr<IRHIDescriptor> m_pMinReductionSampler;
    eastl::unique_ptr<IRHIDescriptor> m_pMaxReductionSampler;
};