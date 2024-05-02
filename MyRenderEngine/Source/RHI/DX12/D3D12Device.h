#pragma once
#include "D3D12Headers.h"
#include "../RHIDevice.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/queue.h"

// {D414FCFE-F480-497E-8BC8-86C209F9194E}
static GUID ComponentID =
{ 0xd414fcfe, 0xf480, 0x497e, { 0x8b, 0xc8, 0x86, 0xc2, 0x9, 0xf9, 0x19, 0x4e } };

namespace D3D12MA
{
    class Allocator;
    class Allocation;
}

class D3D12DescriptorAllocator
{
public:
    D3D12DescriptorAllocator(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE heapType, bool shaderVisible, uint32_t descriptorCount, const eastl::string& name);
    ~D3D12DescriptorAllocator();

    D3D12Descriptor Allocate();
    void Free(const D3D12Descriptor& descriptor);

    ID3D12DescriptorHeap* GetHeap() const { return m_pHeap; }
    D3D12Descriptor GetDescriptor(uint32_t index) const;

private:
    ID3D12DescriptorHeap* m_pHeap = nullptr;
    uint32_t m_descriptorSize = 0;
    uint32_t m_descriptorCount = 0;
    uint32_t m_allocatedCount = 0;
    bool m_shaderVisible = false;
    eastl::vector<D3D12Descriptor> m_freeDescriptors;

};

class D3D12Device;

class D3D12ConstantBufferAllocator
{
public:
    D3D12ConstantBufferAllocator(D3D12Device* pDevice, uint32_t size, const eastl::string& name);
    
    void Allocate(uint32_t size, void** ppCPUAddress, uint64_t* pGPUAddress);
    void Reset();

private:
    eastl::unique_ptr<IRHIBuffer> m_pBuffer = nullptr;
    uint32_t m_allocatedSize = 0;
};

class D3D12Device : public IRHIDevice
{
public:
    D3D12Device(const RHIDeviceDesc& desc);
    virtual ~D3D12Device();

    virtual void* GetHandle() const override { return m_pD3D12Device; }

    virtual uint64_t GetFrameID() const override { return m_frameID; }

    virtual IRHISwapChain* CreateSwapChain(const RHISwapChainDesc& desc, const eastl::string& name) override;
    virtual IRHIHeap* CreatHeap(const RHIHeapDesc& desc, const eastl::string& name) override;
    virtual IRHIBuffer* CreateBuffer(const RHIBufferDesc& desc, const eastl::string& name) override;
    virtual IRHITexture* CreateTexture(const RHITextureDesc& desc, const eastl::string& name) override;
    virtual IRHIDescriptor* CreateShaderResourceView(IRHIResource* pResource, const RHIShaderResourceViewDesc& desc, const eastl::string& name) override;
    virtual IRHIDescriptor* CreateUnorderedAccessView(IRHIResource* pResource, const RHIUnorderedAccessViewDesc& desc, const eastl::string& name) override;
    virtual IRHIDescriptor* CreateConstBufferView(IRHIBuffer* pBuffer, const RHIConstantBufferViewDesc& desc, const eastl::string& name) override;
    virtual IRHIDescriptor* CreateSampler(const RHISamplerDesc& desc, const eastl::string& name) override;
    virtual IRHIRayTracingBLAS* CreateRayTracingBLAS(const RHIRayTracingBLASDesc& desc, const eastl::string& name) override;
    virtual IRHIRayTracingTLAS* CreateRayTracongTLAS(const RHIRayTracingTLASDesc& desc, const eastl::string& name) override;

    virtual uint32_t GetAllocationSize(const RHITextureDesc& desc) override;

    bool Init();
    IDXGIFactory7* GetDXGIFactory() const { return m_pDXGIFactory; }
    ID3D12CommandQueue* GetGraphicsQueue() const { return m_pGraphicsQueue; }
    ID3D12CommandQueue* GetComputeQueue() const { return m_pComputeQueue; }
    ID3D12CommandQueue* GetCopyQueue() const { return m_pCopyQueue; }
    ID3D12RootSignature* GetRootSignature() const { return m_pRootSignature; }
    D3D12MA::Allocator* GetResourceAllocator() const { return m_pResourceAllocator; }
    ID3D12DescriptorHeap* GetResourceDescriptorHeap() const { return m_pResourceDescriptorAllocator->GetHeap(); }
    ID3D12DescriptorHeap* GetSamplerDescriptorHeap() const { return m_pSamplerAllocator->GetHeap(); }

    void FlushDeferredDeletions();
    void Delete(IUnknown* pObject); 
    void Delete(D3D12MA::Allocation* pAllocation);
    void DeleteRTV(const D3D12Descriptor& descriptor);
    void DeleteDSV(const D3D12Descriptor& descriptor);
    void DeleteResoruce(const D3D12Descriptor& descriptor);
    void DeleteSampler(const D3D12Descriptor& descriptor);
    void DeleteNonShaderVisiableUAV(const D3D12Descriptor& descriptor);

    D3D12Descriptor AllocateRTV();
    D3D12Descriptor AllocateDSV();
    D3D12Descriptor AllocateResourceDescriptor();
    D3D12Descriptor AllocateSampler();
    D3D12Descriptor AllocateNonShaderVisibleUAV();

#if MICROPROFILE_GPU_TIMERS_D3D12
    int GetProfileGraphicsQueue() const { return m_profileGraphicsQueue; }
    int GetProfileComputeQueue() const { return m_profileComputeQueue; }
    int GetProfileCopyQueue() const { return m_profileCopyQueue; }
#endif

private:
    void DoDeferredDeletion(bool forceDelete = false);
    void CreateRootSignature();
    void CreateIndirectCommandSignatures();

protected:
    RHIDeviceDesc m_desc;
    RHIVender m_vender = RHIVender::Uknown;
    CD3DX12FeatureSupport m_featureSupport;
    uint64_t m_frameID = 0;

    // D3D12 members
    IDXGIFactory7* m_pDXGIFactory = nullptr;
    IDXGIAdapter4* m_pDXGIAdapter = nullptr;

    ID3D12Device10* m_pD3D12Device = nullptr;
    ID3D12CommandQueue* m_pGraphicsQueue = nullptr;
    ID3D12CommandQueue* m_pComputeQueue = nullptr;
    ID3D12CommandQueue* m_pCopyQueue = nullptr;

    ID3D12RootSignature* m_pRootSignature = nullptr;

    ID3D12CommandSignature* m_pDrawSignature = nullptr;
    ID3D12CommandSignature* m_pDrawIndexedSignature = nullptr;
    ID3D12CommandSignature* m_pDispatchSignature = nullptr;
    ID3D12CommandSignature* m_pDispatchMeshSignature = nullptr;
    ID3D12CommandSignature* m_pMultiDrawSignature = nullptr;
    ID3D12CommandSignature* m_pMultiDrawIndexedSignature = nullptr;
    ID3D12CommandSignature* m_pMultiDispatchSignature = nullptr;
    ID3D12CommandSignature* m_pMultiDispatchMeshSignature = nullptr;

    D3D12MA::Allocator* m_pResourceAllocator = nullptr;
    eastl::unique_ptr<D3D12ConstantBufferAllocator> m_pConstantBufferAllocator[RHI_MAX_INFLIGHT_FRAMES];
    eastl::unique_ptr<D3D12DescriptorAllocator> m_pRTVAllocator;
    eastl::unique_ptr<D3D12DescriptorAllocator> m_pDSVAllocator;
    eastl::unique_ptr<D3D12DescriptorAllocator> m_pResourceDescriptorAllocator;
    eastl::unique_ptr<D3D12DescriptorAllocator> m_pSamplerAllocator;
    eastl::unique_ptr<D3D12DescriptorAllocator> m_pNonShaderVisibleUAVAllocator;
  
    struct ObjectDeletion
    {
        IUnknown* m_pObject;
        uint64_t m_frame;
    };
    eastl::queue<ObjectDeletion> m_objectDeletionQueue;

    struct AllocationDeletion
    {
        D3D12MA::Allocation* m_pAllocation;
        uint64_t m_frame;
    };
    eastl::queue<AllocationDeletion> m_allocationDeletionQueue;

    struct DescriptorDeletion
    {
        D3D12Descriptor m_descriptor;
        uint64_t m_frame;
    };
    eastl::queue<DescriptorDeletion> m_rtvDeletionQueue;
    eastl::queue<DescriptorDeletion> m_dsvDeletionQueue;
    eastl::queue<DescriptorDeletion> m_resourceDeletionQueue;
    eastl::queue<DescriptorDeletion> m_samplerDeletionQueue;
    eastl::queue<DescriptorDeletion> m_nonShaderVisibleUAVDeletionQueue;
    
#if MICROPROFILE_GPU_TIMERS_D3D12
    int m_profileGraphicsQueue = -1;
    int m_profileComputeQueue = -1;
    int m_profileCopyQueue = -1;
#endif
};

