#pragma once

#include "RHIResource.h"

class IRHIFence;
class IRHIBuffer;
class IRHITexture;
class IRHIHeap;
class IRHIDescriptor;
class IRHIPipelineState;
class IRHIRayTracingBLAS;
class IRHIRayTracingTLAS;
class IRHISwapChain;

class IRHICommandList : public IRHIResource
{
public:
    virtual ~IRHICommandList() {}

    virtual RHICommandQueue GetQueue() const = 0;

    virtual void ResetAllocator() = 0;
    virtual void Begin() = 0;
    virtual void End() = 0;
    virtual void Wait(IRHIFence* fence, uint64_t value) = 0;
    virtual void Signal(IRHIFence* fence, uint64_t value) = 0;
    virtual void Present(IRHISwapChain* pSwapChain) = 0;
    virtual void Submit() = 0;
    virtual void ClearState() = 0;

    virtual void BeginProfiling() = 0;
    virtual void EndProfiling() = 0;
    virtual void BeginEvent(const eastl::string& eventName) = 0;
    virtual void EndEvent() = 0;
    
    virtual void CopyBufferToTexture(IRHITexture* pDstTexture, uint32_t mipLevel, uint32_t arraySize, IRHIBuffer* pSrcBuffer, uint32_t offset) = 0;
    virtual void CopyTextureToBuffer(IRHIBuffer* pDstBuffer, IRHITexture* pSrcTexture, uint32_t mipLevel, uint32_t arraySize) = 0;
    virtual void CopyBuffer(IRHIBuffer* pDst, uint32_t dstOffset, IRHIBuffer* pSrc, uint32_t srcOffset, uint32_t size) = 0;
    virtual void CopyTexture(IRHITexture* pDst, uint32_t dstMip, uint32_t dstArray, IRHITexture* pSrc, uint32_t srcMip, uint32_t srcArray) = 0;
    virtual void ClearUAV(IRHIResource* pResource, IRHIDescriptor* pUAV, const float* pClearValue) = 0;
    virtual void ClearUAV(IRHIResource* pResource, IRHIDescriptor* pUAV, const uint32_t* pClearValue) = 0;
    virtual void WriteBuffer(IRHIBuffer* pBuffer, uint32_t offset, uint32_t data) = 0;
    virtual void UpdateTileMappings(IRHITexture* pTexture, IRHIHeap* pHeap, uint32_t mappingCount, const RHITileMapping* pMappings) = 0;

    virtual void TextureBarrier(IRHITexture* pTexture, uint32_t subResource, RHIAccessFlags accessBefore, RHIAccessFlags accessAfter) = 0;
    virtual void BufferBarrier(IRHIBuffer* pBuffer, RHIAccessFlags accessBefore, RHIAccessFlags accessAfter) = 0;
    virtual void GlobalBarrier(RHIAccessFlags accessBefore, RHIAccessFlags accessAfter) = 0;
    virtual void FlushBarriers() = 0;

    virtual void BeginRenderPass(const RHIRenderPassDesc& renderPass) = 0;
    virtual void EndRenderPass() = 0;
    virtual void SetPipelineState(IRHIPipelineState* state) = 0;
    virtual void SetStencilReference(uint8_t stencil) = 0;
    virtual void SetBlendFactor(const float* blendFactor) = 0;
    virtual void SetIndexBuffer(IRHIBuffer* pBuffer, uint32_t offset, RHIFormat format) = 0;
    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    virtual void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    virtual void SetGraphicsConstants(uint32_t slot, const void* data, size_t dataSize) = 0;
    virtual void SetComputeConstants(uint32_t slot, const void* data, size_t dataSize) = 0;

    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t indexOffset = 0) = 0;
    virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
    virtual void DispatchMesh(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
    
    virtual void DrawIndirect(IRHIBuffer* pBuffer, uint32_t offset) = 0;
    virtual void DrawIndexedIndirect(IRHIBuffer* pBuffer, uint32_t offset) = 0;
    virtual void DispatchIndirect(IRHIBuffer* pBuffer, uint32_t offset) = 0;
    virtual void DispatchMeshIndirect(IRHIBuffer* pBuffer, uint32_t offset) = 0;

    virtual void MultiDrawIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset) = 0;
    virtual void MultiDrawIndexedIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset) = 0;
    virtual void MultiDispatchIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset) = 0;
    virtual void MultiDispatchMeshIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset) = 0;

    virtual void BuildRayTracingBLAS(IRHIRayTracingBLAS* pBLAS) = 0;
    virtual void UpdateRayTracingBLAS(IRHIRayTracingBLAS* pBLAS, IRHIBuffer* pVertexBuffer, uint32_t vertexBufferOffset) = 0;
    virtual void BuildRayTracingTLAS(IRHIRayTracingTLAS* pTLAS, const RHIRayTracingInstance* pInstances, uint32_t instanceCount) = 0;

#if MICROPROFILE_GPU_TIMERS
    virtual struct MicroProfileThreadLogGpu* GetProfileLog() const = 0;
#endif
    
};