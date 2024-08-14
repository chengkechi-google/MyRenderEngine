#pragma once
#include "D3D12Headers.h"
#include "RHI/RHICommandList.h"

class D3D12CommandList : public IRHICommandList
{
public:
    D3D12CommandList(IRHIDevice* pDevice, RHICommandQueue queueType, const eastl::string& nmae);
    ~D3D12CommandList();

    bool Create();

    virtual void* GetHandle() const override { return m_pCommandList; }
    virtual RHICommandQueue GetQueue() const override { return m_queueType; }

    virtual void ResetAllocator() override;
    virtual void Begin() override;
    virtual void End() override;
    virtual void Wait(IRHIFence* fence, uint64_t value) override;
    virtual void Signal(IRHIFence* fence, uint64_t value) override;
    virtual void Present(IRHISwapChain* pSwapChain) override;
    virtual void Submit() override;
    virtual void ClearState() override;

    virtual void BeginProfiling() override;
    virtual void EndProfiling() override;
    virtual void BeginEvent(const eastl::string& eventName) override;
    virtual void EndEvent() override;
    
    virtual void CopyBufferToTexture(IRHITexture* pDstTexture, uint32_t mipLevel, uint32_t arraySize, IRHIBuffer* pSrcBuffer, uint32_t offset) override;
    virtual void CopyTextureToBuffer(IRHIBuffer* pDstBuffer, IRHITexture* pSrcTexture, uint32_t mipLevel, uint32_t arraySize) override;
    virtual void CopyBuffer(IRHIBuffer* pDst, uint32_t dstOffset, IRHIBuffer* pSrc, uint32_t srcOffset, uint32_t size) override;
    virtual void CopyTexture(IRHITexture* pDst, uint32_t dstMip, uint32_t dstArray, IRHITexture* pSrc, uint32_t srcMip, uint32_t srcArray) override;
    virtual void ClearUAV(IRHIResource* pResource, IRHIDescriptor* pUAV, const float* pClearValue) override;
    virtual void ClearUAV(IRHIResource* pResource, IRHIDescriptor* pUAV, const uint32_t* pClearValue) override;
    virtual void WriteBuffer(IRHIBuffer* pBuffer, uint32_t offset, uint32_t data) override;
    virtual void UpdateTileMappings(IRHITexture* pTexture, IRHIHeap* pHeap, uint32_t mappingCount, const RHITileMapping* pMappings) override;

    virtual void TextureBarrier(IRHITexture* pTexture, uint32_t subResource, RHIAccessFlags accessBefore, RHIAccessFlags accessAfter) override;
    virtual void BufferBarrier(IRHIBuffer* pBuffer, RHIAccessFlags accessBefore, RHIAccessFlags accessAfter) override;
    virtual void GlobalBarrier(RHIAccessFlags accessBefore, RHIAccessFlags accessAfter) override;
    virtual void FlushBarriers() override;

    virtual void BeginRenderPass(const RHIRenderPassDesc& renderPass) override;
    virtual void EndRenderPass() override;
    virtual void SetPipelineState(IRHIPipelineState* state) override;
    virtual void SetStencilReference(uint8_t stencil) override;
    virtual void SetBlendFactor(const float* blendFactor) override;
    virtual void SetIndexBuffer(IRHIBuffer* pBuffer, uint32_t offset, RHIFormat format) override;
    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    virtual void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    virtual void SetGraphicsConstants(uint32_t slot, const void* data, size_t dataSize) override;
    virtual void SetComputeConstants(uint32_t slot, const void* data, size_t dataSize) override;

    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) override;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t indexOffset = 0) override;
    virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    virtual void DispatchMesh(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
    
    virtual void DrawIndirect(IRHIBuffer* pBuffer, uint32_t offset) override;
    virtual void DrawIndexedIndirect(IRHIBuffer* pBuffer, uint32_t offset) override;
    virtual void DispatchIndirect(IRHIBuffer* pBuffer, uint32_t offset) override;
    virtual void DispatchMeshIndirect(IRHIBuffer* pBuffer, uint32_t offset) override;

    virtual void MultiDrawIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset) override;
    virtual void MultiDrawIndexedIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset) override;
    virtual void MultiDispatchIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset) override;
    virtual void MultiDispatchMeshIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset) override;

    virtual void BuildRayTracingBLAS(IRHIRayTracingBLAS* pBLAS) override;
    virtual void UpdateRayTracingBLAS(IRHIRayTracingBLAS* pBLAS, IRHIBuffer* pVertexBuffer, uint32_t vertexBufferOffset) override;
    virtual void BuildRayTracingTLAS(IRHIRayTracingTLAS* pTLAS, const RHIRayTracingInstance* pInstances, uint32_t instanceCount) override;

#if MICROPROFILE_GPU_TIMERS
    virtual struct MicroProfileThreadLogGpu* GetProfileLog() const override;
#endif

private:
    RHICommandQueue m_queueType;
    ID3D12CommandQueue* m_pCommandQueue = nullptr;
    ID3D12CommandAllocator* m_pCommandAllocator = nullptr;
    ID3D12GraphicsCommandList10* m_pCommandList = nullptr;

    uint32_t m_commandCount = 0;
    IRHIPipelineState* m_pCurrentPSO = nullptr;
    
    eastl::vector<D3D12_TEXTURE_BARRIER> m_textureBarriers;
    eastl::vector<D3D12_BUFFER_BARRIER> m_bufferBarriers;
    eastl::vector<D3D12_GLOBAL_BARRIER> m_globalBarriers;
    
    eastl::vector<eastl::pair<IRHIFence*, uint64_t>> m_pendingWaits;
    eastl::vector<eastl::pair<IRHIFence*, uint64_t>> m_pendingSignals;
    
    eastl::vector<IRHISwapChain*> m_pendingSwapChain;

#if MICROPROFILE_GPU_TIMERS_D3D12
    struct MicroProfileThreadLogGpu* m_pProfileLog = nullptr;
    int m_profileQueue = -1;
#endif
};