#pragma once
#include "RHIDefines.h"

class IRHIResource;
class IRHIHeap;
class IRHIBuffer;
class IRHITexture;
class IRHIFence;
class IRHISwapChain;
class IRHICommandList;
class IRHIShader;
class IRHIPipelineState;
class IRHIDescriptor;
class IRHIRayTracingBLAS;
class IRHIRayTracingTLAS;

class IRHIDevice
{
public:
    virtual ~IRHIDevice() {}

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual uint64_t GetFrameID() const = 0;
    virtual void* GetHandle() const = 0;
    virtual RHIVender GetVender() const = 0;
 
    virtual IRHISwapChain* CreateSwapChain(const RHISwapChainDesc& desc, const eastl::string& name) = 0;
    virtual IRHICommandList* CreateCommandList(RHICommandQueue queueType, const eastl::string& name) = 0;
    virtual IRHIFence* CreateFence(const eastl::string& name) = 0;
    virtual IRHIHeap* CreatHeap(const RHIHeapDesc& desc, const eastl::string& name) = 0;
    virtual IRHIBuffer* CreateBuffer(const RHIBufferDesc& desc, const eastl::string& name) = 0;
    virtual IRHITexture* CreateTexture(const RHITextureDesc& desc, const eastl::string& name) = 0;
    virtual IRHIShader* CreateShader(const RHIShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name) = 0;
    virtual IRHIPipelineState* CreateGraphicsPipelineState(const RHIGraphicsPipelineDesc& desc, const eastl::string& name) = 0;
    virtual IRHIPipelineState* CreateMeshShaderPipelineState(const RHIMeshShaderPipelineDesc& desc, const eastl::string& name) = 0;
    virtual IRHIPipelineState* CreateComputePipelineState(const RHIComputePipelineDesc& desc, const eastl::string& name) = 0;
    virtual IRHIDescriptor* CreateShaderResourceView(IRHIResource* pResource, const RHIShaderResourceViewDesc& desc, const eastl::string& name) = 0;
    virtual IRHIDescriptor* CreateUnorderedAccessView(IRHIResource* pResource, const RHIUnorderedAccessViewDesc& desc, const eastl::string& name) = 0;
    virtual IRHIDescriptor* CreateConstBufferView(IRHIBuffer* pBuffer, const RHIConstantBufferViewDesc& desc, const eastl::string& name) = 0;
    virtual IRHIDescriptor* CreateSampler(const RHISamplerDesc& desc, const eastl::string& name) = 0;
    virtual IRHIRayTracingBLAS* CreateRayTracingBLAS(const RHIRayTracingBLASDesc& desc, const eastl::string& name) = 0;
    virtual IRHIRayTracingTLAS* CreateRayTracongTLAS(const RHIRayTracingTLASDesc& desc, const eastl::string& name) = 0;

    virtual uint32_t GetAllocationSize(const RHITextureDesc& desc) = 0;
};