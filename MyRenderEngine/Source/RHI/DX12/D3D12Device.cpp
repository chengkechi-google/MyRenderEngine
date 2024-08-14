#include "D3D12Device.h"
#include "D3D12Buffer.h"
#include "D3D12Texture.h"
#include "D3D12SwapChain.h"
#include "D3D12Fence.h"
#include "D3D12CommandList.h"
#include "D3D12Shader.h"
#include "D3D12PipelineState.h"
#include "D3D12Heap.h"
#include "D3D12Descriptor.h"
#include "D3D12RayTracingBLAS.h"
#include "D3D12RayTracingTLAS.h"
#include "Utils/log.h"
#include "Utils/string.h"
#include "Utils/math.h"
#include "magic_enum/magic_enum.hpp"
#include "ags.h"
#include "d3d12ma/D3D12MemAlloc.h"
#include "PixRuntime.h"
#include "microprofile/microprofile.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

extern "C" { _declspec(dllexport) extern const UINT D3D12SDKVersion = 614; }
extern "C" { _declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }

static void __stdcall D3D12MessageCallback(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, LPCSTR pDescription, void* pContext)
{
    MY_DEBUG(pDescription);
}

static IDXGIAdapter1* FindAdapter(IDXGIFactory6* pDXGIFactory, D3D_FEATURE_LEVEL minimumFeatureLevel)
{
    eastl::vector<IDXGIAdapter1*> adapters;

    // Get how many adapters
    MY_DEBUG("Available GPUs : ");

    IDXGIAdapter1* pDXGIAdapter = nullptr;
    for (UINT adapterIndex = 0;
        DXGI_ERROR_NOT_FOUND != pDXGIFactory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pDXGIAdapter));
        ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        pDXGIAdapter->GetDesc1(&desc);
        MY_DEBUG(" - {}", wstring_to_string(desc.Description));
        adapters.push_back(pDXGIAdapter);
    }
    
    auto selectAdapter = eastl::find_if(adapters.begin(), adapters.end(), [=](IDXGIAdapter1* adapter)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        
        bool isSoftwareGPU = desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE;
        bool capableFeatureLevel = SUCCEEDED(D3D12CreateDevice(adapter, minimumFeatureLevel, _uuidof(ID3D12Device), nullptr));

        return !isSoftwareGPU && capableFeatureLevel;
    });

    for (auto adapter : adapters)
    {
        if (selectAdapter == nullptr || adapter != *selectAdapter)
        {
            SAFE_RELEASE(adapter);
        }
    }

    return selectAdapter == nullptr ? nullptr : *selectAdapter;
}

D3D12DescriptorAllocator::D3D12DescriptorAllocator(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE heapType, bool shaderVisible, uint32_t descriptorCount, const eastl::string& name)
{
    m_descriptorSize = pDevice->GetDescriptorHandleIncrementSize(heapType);
    m_descriptorCount = descriptorCount;
    m_allocatedCount = 0;
    m_shaderVisible = shaderVisible;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = heapType;
    desc.NumDescriptors = m_descriptorCount;
    if (m_shaderVisible)
    {
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    }
    pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pHeap));

    m_pHeap->SetName(string_to_wstring(name).c_str());
}

D3D12DescriptorAllocator::~D3D12DescriptorAllocator()
{
    SAFE_RELEASE(m_pHeap);
}

D3D12Descriptor D3D12DescriptorAllocator::Allocate()
{
    if (!m_freeDescriptors.empty())
    {
        D3D12Descriptor descriptor = m_freeDescriptors.back();
        m_freeDescriptors.pop_back();
        return descriptor;
    }

    MY_ASSERT(m_allocatedCount <= m_descriptorCount);
    D3D12Descriptor descriptor = GetDescriptor(m_allocatedCount);
    ++m_allocatedCount;
    return descriptor;
}

void D3D12DescriptorAllocator::Free(const D3D12Descriptor& descriptor)
{
    m_freeDescriptors.push_back(descriptor);
}

D3D12Descriptor D3D12DescriptorAllocator::GetDescriptor(uint32_t index) const
{
    D3D12Descriptor descriptor;
    descriptor.cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pHeap->GetCPUDescriptorHandleForHeapStart(), index, m_descriptorSize);

    if (m_shaderVisible)
    {
        descriptor.gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pHeap->GetGPUDescriptorHandleForHeapStart(), index, m_descriptorSize);
    }
    descriptor.index = index;
    return descriptor;
}

D3D12ConstantBufferAllocator::D3D12ConstantBufferAllocator(D3D12Device* pDevice, uint32_t size, const eastl::string& name)
{
    RHIBufferDesc desc = {};
    desc.m_size = size;
    desc.m_memoryType = RHIMemoryType::CPUToGPU;
    desc.m_usage = RHIBufferUsageBit::RHIBufferUsageConstantBuffer;

    m_pBuffer.reset(pDevice->CreateBuffer(desc, name));
}

void D3D12ConstantBufferAllocator::Allocate(uint32_t size, void** ppCPUAddress, uint64_t* pGPUAddress)
{
    MY_ASSERT(m_allocatedSize + size <= m_pBuffer->GetDesc().m_size);
    *ppCPUAddress = static_cast<char*>(m_pBuffer->GetCPUAddress()) + m_allocatedSize;
    *pGPUAddress = m_pBuffer->GetGPUAddress() + m_allocatedSize;
    
    m_allocatedSize += RoundUpPow2(size, 256); //< Aligment a mutiple of 256    
}

void D3D12ConstantBufferAllocator::Reset()
{
    m_allocatedSize = 0;
}

D3D12Device::D3D12Device(const RHIDeviceDesc& desc)
{
    m_desc = desc;
}

D3D12Device::~D3D12Device()
{
    for (uint32_t i = 0; i < RHI_MAX_INFLIGHT_FRAMES; ++i)
    {
        m_pConstantBufferAllocator[i].reset();
    }

    FlushDeferredDeletions();

    m_pRTVAllocator.reset();
    m_pDSVAllocator.reset();
    m_pResourceDescriptorAllocator.reset();
    m_pSamplerAllocator.reset();
    m_pNonShaderVisibleUAVAllocator.reset();

    
    SAFE_RELEASE(m_pDrawSignature);
    SAFE_RELEASE(m_pDrawIndexedSignature);
    SAFE_RELEASE(m_pDispatchSignature);
    SAFE_RELEASE(m_pDispatchMeshSignature);
    SAFE_RELEASE(m_pMultiDrawSignature);
    SAFE_RELEASE(m_pMultiDrawIndexedSignature);
    SAFE_RELEASE(m_pMultiDispatchSignature);
    SAFE_RELEASE(m_pMultiDispatchMeshSignature);
    SAFE_RELEASE(m_pRootSignature);
    SAFE_RELEASE(m_pResourceAllocator);
    SAFE_RELEASE(m_pGraphicsQueue);
    SAFE_RELEASE(m_pComputeQueue);
    SAFE_RELEASE(m_pCopyQueue);
    SAFE_RELEASE(m_pDXGIAdapter);
    SAFE_RELEASE(m_pDXGIFactory);

#if defined(_DEBUG)
    ID3D12DebugDevice* pDebugDevice = nullptr;
    m_pD3D12Device->QueryInterface(IID_PPV_ARGS(&pDebugDevice));
#endif

    if (m_vender == RHIVender::AMD)
    {
        ags::ReleaseDevice(m_pD3D12Device);
    }
    else
    {
        SAFE_RELEASE(m_pD3D12Device);
    }

#if defined(_DEBUG)
    if (pDebugDevice)
    {
        pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
        pDebugDevice->Release();
    }
#endif

}

void D3D12Device::BeginFrame()
{
    DoDeferredDeletion();
    
    uint32_t index = m_frameID % RHI_MAX_INFLIGHT_FRAMES;
    m_pConstantBufferAllocator[index]->Reset();
}

void D3D12Device::EndFrame()
{
    ++ m_frameID;
    m_pResourceAllocator->SetCurrentFrameIndex((UINT) m_frameID);
}

IRHISwapChain* D3D12Device::CreateSwapChain(const RHISwapChainDesc& desc, const eastl::string& name)
{
    D3D12SwapChain* pSwapChain = new D3D12SwapChain(this, desc, name);
    if (!pSwapChain->Create())
    {
        delete pSwapChain;
        return nullptr;
    }

    return pSwapChain; 
}

IRHICommandList* D3D12Device::CreateCommandList(RHICommandQueue queueType, const eastl::string& name)
{
    D3D12CommandList* pCommandList = new D3D12CommandList(this, queueType, name);
    if (!pCommandList->Create())
    {
        delete pCommandList;
        return nullptr;
    }

    return pCommandList;
}

IRHIFence* D3D12Device::CreateFence(const eastl::string& name)
{
    D3D12Fence* pFence = new D3D12Fence(this, name);
    if (!pFence->Create())
    {
        delete pFence;
        return nullptr;
    }

    return pFence;
}

IRHIHeap* D3D12Device::CreatHeap(const RHIHeapDesc& desc, const eastl::string& name)
{
    D3D12Heap* pHeap = new D3D12Heap(this, desc, name);
    if (!pHeap->Create())
    {
        delete pHeap;
        return nullptr;
    }

    return pHeap;
}

IRHIBuffer* D3D12Device::CreateBuffer(const RHIBufferDesc& desc, const eastl::string& name)
{
    D3D12Buffer* pBuffer = new D3D12Buffer(this, desc, name);
    if (!pBuffer->Create())
    {
        delete pBuffer;
        return nullptr;
    }

    return pBuffer;
}

IRHITexture* D3D12Device::CreateTexture(const RHITextureDesc& desc, const eastl::string& name)
{
    D3D12Texture* pTexture = new D3D12Texture(this, desc, name);
    if (!pTexture->Create())
    {
        delete pTexture;
        return nullptr;
    }
    
    return pTexture;    
}

IRHIShader* D3D12Device::CreateShader(const RHIShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name)
{
    return new D3D12Shader(this, desc, data, name);
}

IRHIPipelineState* D3D12Device::CreateGraphicsPipelineState(const RHIGraphicsPipelineDesc& desc, const eastl::string& name)
{
    D3D12GraphicsPipelineState* pPipelineState = new D3D12GraphicsPipelineState(this, desc, name);
    if (!pPipelineState->Create())
    {
        delete pPipelineState;
        return nullptr;
    }

    return pPipelineState;
}

IRHIPipelineState* D3D12Device::CreateMeshShaderPipelineState(const RHIMeshShaderPipelineDesc& desc, const eastl::string& name)
{
    D3D12MeshShaderPipelineState* pPipelineState = new D3D12MeshShaderPipelineState(this, desc, name);
    if (!pPipelineState->Create())
    {
        delete pPipelineState;
        return nullptr;
    }

    return pPipelineState;
}

IRHIPipelineState* D3D12Device::CreateComputePipelineState(const RHIComputePipelineDesc& desc, const eastl::string& name)
{
    D3D12ComputePipelineState* pPipelineState = new D3D12ComputePipelineState(this, desc, name);
    if (!pPipelineState->Create())
    {
        delete pPipelineState;
        return nullptr;
    }
    
    return pPipelineState;
}

IRHIDescriptor* D3D12Device::CreateShaderResourceView(IRHIResource* pResource, const RHIShaderResourceViewDesc& desc, const eastl::string& name)
{
    D3D12ShaderResourceView* pSRV = new D3D12ShaderResourceView(this, pResource, desc, name);
    if (!pSRV->Create())
    {
        delete pSRV;
        return nullptr;
    }

    return pSRV;
}

IRHIDescriptor* D3D12Device::CreateUnorderedAccessView(IRHIResource* pResource, const RHIUnorderedAccessViewDesc& desc, const eastl::string& name)
{
    D3D12UnorderedAccessView* pUAV = new D3D12UnorderedAccessView(this, pResource, desc, name);
    if (!pUAV->Create())
    {
        delete pUAV;
        return nullptr;
    }

    return pUAV;
}

IRHIDescriptor* D3D12Device::CreateConstBufferView(IRHIBuffer* pBuffer, const RHIConstantBufferViewDesc& desc, const eastl::string& name)
{
    D3D12ConstantBufferView* pCBV = new D3D12ConstantBufferView(this, pBuffer, desc, name);
    if (!pCBV->Create())
    {
        delete pCBV;
        return nullptr;
    }
    
    return pCBV;
}

IRHIDescriptor* D3D12Device::CreateSampler(const RHISamplerDesc& desc, const eastl::string& name)
{
    D3D12Sampler *pSampler = new D3D12Sampler(this, desc, name);
    if (!pSampler->Create())
    {
        delete pSampler;
        return nullptr;
    }

    return pSampler;
}

IRHIRayTracingBLAS* D3D12Device::CreateRayTracingBLAS(const RHIRayTracingBLASDesc& desc, const eastl::string& name)
{
    D3D12RayTracingBLAS* pBLAS = new D3D12RayTracingBLAS(this, desc, name);
    if (!pBLAS->Create())
    {
        delete pBLAS;
        return nullptr;
    }
    
    return pBLAS;
}

IRHIRayTracingTLAS* D3D12Device::CreateRayTracongTLAS(const RHIRayTracingTLASDesc& desc, const eastl::string& name)
{
    D3D12RayTracingTLAS* pTLAS = new D3D12RayTracingTLAS(this, desc, name);
    if (!pTLAS->Create())
    {
        delete pTLAS;
        return nullptr;
    }

    return pTLAS;
}

uint32_t D3D12Device::GetAllocationSize(const RHITextureDesc& desc)
{
    D3D12_RESOURCE_DESC resourceDesc = RHIToD3D12ResourceDesc(desc);
    D3D12_RESOURCE_ALLOCATION_INFO info = m_pD3D12Device->GetResourceAllocationInfo(0, 1, &resourceDesc);
    return (uint32_t) info.SizeInBytes;
}

bool D3D12Device::Init()
{
    UINT dxgiFactoryFlag = 0;

//  Receve debug layer
#if defined(_DEBUG)
    ID3D12Debug6* pDebugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController))))
    {
        pDebugController->EnableDebugLayer();
        //pDebugController->SetEnableGPUBasedValidation(TRUE);
    
        dxgiFactoryFlag |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    if (FAILED(CreateDXGIFactory2(dxgiFactoryFlag, IID_PPV_ARGS(&m_pDXGIFactory))))
    {
        return false;
    }

    D3D_FEATURE_LEVEL minimunFeatureLevel = D3D_FEATURE_LEVEL_12_0;
    m_pDXGIAdapter = FindAdapter(m_pDXGIFactory, minimunFeatureLevel);

    if (m_pDXGIAdapter == nullptr)
    {
        MY_ERROR("Failed to find a capable DXGI adapter.");
        return false; 
    }

    DXGI_ADAPTER_DESC desc;
    m_pDXGIAdapter->GetDesc(&desc);
    switch (desc.VendorId)
    {
        case 0x1002:
            m_vender = RHIVender::AMD;
            break;
        case 0x10DE:
            m_vender = RHIVender::NVidia;
            break;
        case 0x8086:
            m_vender = RHIVender::Intel;
            break;
        default:
            break;        
    }

    MY_INFO("Vendor : {}", magic_enum::enum_name(m_vender));
    MY_INFO("GPU : {}", wstring_to_string(desc.Description));

    if(m_vender == RHIVender::AMD)
    {
        // Use AMD GPU service help function to create device
        if (FAILED(ags::CreateDevice(m_pDXGIAdapter, minimunFeatureLevel, IID_PPV_ARGS(&m_pD3D12Device))))
        {
            return false;
        }
    }
    else
    {
        // Normal create d3d12 device
        HRESULT hResult = D3D12CreateDevice(m_pDXGIAdapter, minimunFeatureLevel, IID_PPV_ARGS(&m_pD3D12Device));
        if (FAILED(hResult))
        {
            return false;
        }
    }

    m_featureSupport.Init(m_pD3D12Device);
    bool capableDevice = m_featureSupport.HighestShaderModel() >= D3D_SHADER_MODEL_6_6
                         && m_featureSupport.ResourceBindingTier() >= D3D12_RESOURCE_BINDING_TIER_3    //< Hardware tier 3
                         && m_featureSupport.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_1
                         && m_featureSupport.RenderPassesTier() >= D3D12_RENDER_PASS_TIER_0            //< Depend UAV write way, no using render pass will be efficient
                         && m_featureSupport.MeshShaderTier() >= D3D12_MESH_SHADER_TIER_1              //< Meshlet
                         && m_featureSupport.WaveOps()
                         && m_featureSupport.Native16BitShaderOpsSupported()
                         && m_featureSupport.EnhancedBarriersSupported();
                            

    if (!capableDevice)
    {
        MY_ERROR("The device is not capable of running this test engine.");
        return false;
    }                 

#if _DEBUG
    ID3D12InfoQueue1* pInfoQueue1 = nullptr;
    m_pD3D12Device->QueryInterface(IID_PPV_ARGS(&pInfoQueue1));
    if (pInfoQueue1)
    {
        DWORD CallbackQueue;
        pInfoQueue1->RegisterMessageCallback(D3D12MessageCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &CallbackQueue);        
    }
    SAFE_RELEASE(pInfoQueue1);
#endif       

// Create command queues
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    m_pD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pGraphicsQueue));
    m_pGraphicsQueue->SetName(L"Graphics queue");

    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    m_pD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pComputeQueue));
    m_pComputeQueue->SetName(L"Compute queue");

    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    m_pD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCopyQueue));
    m_pComputeQueue->SetName(L"Copy queue");

// Create allocators
    D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
    allocatorDesc.pDevice = m_pD3D12Device;
    allocatorDesc.pAdapter = m_pDXGIAdapter;

    if (FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &m_pResourceAllocator)))
    {
        MY_ERROR("Create D3D12 memory allocator faield");
        return false;
    }

    for (uint32_t i = 0; i < RHI_MAX_INFLIGHT_FRAMES; ++i)
    {
        eastl::string name = fmt::format("CB Allocator {}", i).c_str();
        m_pConstantBufferAllocator[i] = eastl::make_unique<D3D12ConstantBufferAllocator> (this, 8 * 1024 * 1024, name);
    }

    m_pRTVAllocator = eastl::make_unique<D3D12DescriptorAllocator> (m_pD3D12Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false, 512, "RTV Heap");
    m_pDSVAllocator = eastl::make_unique<D3D12DescriptorAllocator> (m_pD3D12Device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false, 128, "DSV Heap");
    m_pResourceDescriptorAllocator = eastl::make_unique<D3D12DescriptorAllocator>(m_pD3D12Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true, 65536, "Resource Heap");
    m_pSamplerAllocator = eastl::make_unique<D3D12DescriptorAllocator>(m_pD3D12Device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, true, 128, "Sampler Heap");
    m_pNonShaderVisibleUAVAllocator = eastl::make_unique<D3D12DescriptorAllocator>(m_pD3D12Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false, 4096, "None Shader Visisble UAV Heap");

    CreateRootSignature();
    CreateIndirectCommandSignatures();
    
    PIX::Init();

#if MICROPROFILE_GPU_TIMERS_D3D12
    m_profileGraphicsQueue = MicroProfileInitGpuQueue("GPU graphics queue");
    m_profileComputeQueue = MicroProfileInitGpuQueue("GPU compute queue");
    m_profileCopyQueue = MicroProfileInitGpuQueue("GPU copy queue");

    // todo: maybe can add compute and copy queue
    MicroProfileGpuInitD3D12(m_pD3D12Device, 1, (void**) &m_pGraphicsQueue);
    MicroProfileSetCurrentNodeD3D12(0);
#endif
 
    return true;
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12Device::AllocateConstantBuffer(const void* data, size_t dataSize)
{
    void* cpuAddress;
    uint64_t gpuAddress;
    
    uint32_t index = m_frameID % RHI_MAX_INFLIGHT_FRAMES;
    m_pConstantBufferAllocator[index]->Allocate((uint32_t) dataSize, &cpuAddress, &gpuAddress);

    memcpy(cpuAddress, data, dataSize);
    return gpuAddress;
}

void D3D12Device::FlushDeferredDeletions()
{
    DoDeferredDeletion(true);
}

void D3D12Device::Delete(IUnknown* pObject)
{
    if (pObject)
    {
        m_objectDeletionQueue.push({ pObject, m_frameID });
    }
}

void D3D12Device::Delete(D3D12MA::Allocation* pAllocation)
{
    if (pAllocation)
    {
        m_allocationDeletionQueue.push({ pAllocation, m_frameID });
    }
}

void D3D12Device::DeleteRTV(const D3D12Descriptor& descriptor)
{
    if (!IsNullDescriptor(descriptor))
    {
        m_rtvDeletionQueue.push({descriptor, m_frameID});
    }
}

void D3D12Device::DeleteDSV(const D3D12Descriptor& descriptor)
{
    if (!IsNullDescriptor(descriptor))
    {
        m_dsvDeletionQueue.push({ descriptor, m_frameID });
    }
}

void D3D12Device::DeleteResoruce(const D3D12Descriptor& descriptor)
{
    if (!IsNullDescriptor(descriptor))
    {
        m_resourceDeletionQueue.push({ descriptor, m_frameID });
    }
}

void D3D12Device::DeleteSampler(const D3D12Descriptor& descriptor)
{
    if (!IsNullDescriptor(descriptor))
    {
        m_samplerDeletionQueue.push({ descriptor, m_frameID });
    }
}

void D3D12Device::DeleteNonShaderVisiableUAV(const D3D12Descriptor& descriptor)
{
    if (!IsNullDescriptor(descriptor))
    {
        m_nonShaderVisibleUAVDeletionQueue.push({ descriptor, m_frameID });
    }
}

void D3D12Device::DoDeferredDeletion(bool forceDelete)
{
    while (!m_objectDeletionQueue.empty())
    {
        auto item = m_objectDeletionQueue.front();
        if(!forceDelete && item.m_frame + m_desc.m_maxFrameDelay > m_frameID)
        {
            break;
        }

        SAFE_RELEASE(item.m_pObject);
        m_objectDeletionQueue.pop();
    }

    while (!m_allocationDeletionQueue.empty())
    {
        auto item = m_allocationDeletionQueue.front();
        if (!forceDelete && item.m_frame + m_desc.m_maxFrameDelay > m_frameID)
        {
            break;
        }

        SAFE_RELEASE(item.m_pAllocation);
        m_allocationDeletionQueue.pop();
    }

    while (!m_rtvDeletionQueue.empty())
    {
        auto item = m_rtvDeletionQueue.front();
        if(!forceDelete && item.m_frame + m_desc.m_maxFrameDelay > m_frameID)
        {
            break;
        }

        m_pRTVAllocator->Free(item.m_descriptor);
        m_rtvDeletionQueue.pop();
    }

    while (!m_dsvDeletionQueue.empty())
    {
        auto item = m_dsvDeletionQueue.front();
        if (!forceDelete && item.m_frame + m_desc.m_maxFrameDelay > m_frameID)
        {
            break;
        }

        m_pDSVAllocator->Free(item.m_descriptor);
        m_dsvDeletionQueue.pop();
    }
    
    while (!m_resourceDeletionQueue.empty())
    {
        auto item = m_resourceDeletionQueue.front();
        if (!forceDelete && item.m_frame + m_desc.m_maxFrameDelay > m_frameID)
        {
            break;
        }

        m_pResourceDescriptorAllocator->Free(item.m_descriptor);
        m_resourceDeletionQueue.pop();
    }

    while (!m_samplerDeletionQueue.empty())
    {
        auto item = m_samplerDeletionQueue.front();
        if (!forceDelete && item.m_frame + m_desc.m_maxFrameDelay > m_frameID)
        {
            break;
        }

        m_pSamplerAllocator->Free(item.m_descriptor);
        m_samplerDeletionQueue.pop();
    }

    while (!m_nonShaderVisibleUAVDeletionQueue.empty())
    {
        auto item = m_nonShaderVisibleUAVDeletionQueue.front();
        if (!forceDelete && item.m_frame + m_desc.m_maxFrameDelay > m_frameID)
        {
            break;
        }

        m_pNonShaderVisibleUAVAllocator->Free(item.m_descriptor);
        m_nonShaderVisibleUAVDeletionQueue.pop();
    }
}

void D3D12Device::CreateRootSignature()
{
    //AMD : Try to stay below 13 DWORDs https://gpuopen.com/performance/
    //8 root constants + 2 root CBVs == 12 DWORDs, everything else is bindless   
    CD3DX12_ROOT_PARAMETER1 rootParameters[RHI_MAX_CBV_BINDINGS] = {};
    rootParameters[0].InitAsConstants(RHI_MAX_ROOT_CONSTATNS, 0);
    for (uint32_t i = 1; i < RHI_MAX_CBV_BINDINGS; ++i)
    {
        rootParameters[i].InitAsConstantBufferView(i);
    }

    D3D12_ROOT_SIGNATURE_FLAGS flags = 
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | //todo : does this SM6.6 flag cost additional DWORDs ?
        D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
    desc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, flags);
    
    ID3DBlob* signature = nullptr;
    ID3DBlob* error = nullptr;    
    HRESULT result = D3D12SerializeVersionedRootSignature(&desc, &signature, &error);
    MY_ASSERT(SUCCEEDED(result));

    result = m_pD3D12Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature));
    MY_ASSERT(SUCCEEDED(result));

    SAFE_RELEASE(signature);
    SAFE_RELEASE(error);
    
    m_pRootSignature->SetName(L"D3D12Device::m_pRootSignature");
}

void D3D12Device::CreateIndirectCommandSignatures()
{
    D3D12_INDIRECT_ARGUMENT_DESC argumentDesc = {};

    D3D12_COMMAND_SIGNATURE_DESC desc = {};
    desc.NumArgumentDescs = 1;
    desc.pArgumentDescs = &argumentDesc;
    
    desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
    argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    HRESULT result = m_pD3D12Device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_pDrawSignature));
    MY_ASSERT(SUCCEEDED(result));
    m_pDrawSignature->SetName(L"D3D12Device::m_pDrawSignature");

    desc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    result = m_pD3D12Device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_pDrawIndexedSignature));
    MY_ASSERT(SUCCEEDED(result));
    m_pDrawIndexedSignature->SetName(L"D3D12Device::m_pDrawIndexedSignature");

    desc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
    argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
    result = m_pD3D12Device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_pDispatchSignature));
    MY_ASSERT(SUCCEEDED(result));
    m_pDispatchSignature->SetName(L"D3D12Device::m_pDispatchSignature");

    desc.ByteStride = sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
    argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
    result = m_pD3D12Device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(&m_pDispatchMeshSignature));
    MY_ASSERT(SUCCEEDED(result));
    m_pDispatchMeshSignature->SetName(L"D3D12Device::m_pDispatchMeshSignature");

    // Multi draw command root signature
    D3D12_INDIRECT_ARGUMENT_DESC mdiArgumentDesc[2] = {};
    mdiArgumentDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT; //a uint32 root constant (like "gl_DrawID")
    mdiArgumentDesc[0].Constant.RootParameterIndex = 0;
    mdiArgumentDesc[0].Constant.Num32BitValuesToSet = 1;

    desc.NumArgumentDescs = 2;
    desc.pArgumentDescs = mdiArgumentDesc;

    desc.ByteStride = sizeof(uint32_t) + sizeof(D3D12_DRAW_ARGUMENTS);
    mdiArgumentDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    result = m_pD3D12Device->CreateCommandSignature(&desc, m_pRootSignature, IID_PPV_ARGS(&m_pMultiDrawSignature));
    MY_ASSERT(SUCCEEDED(result));
    m_pMultiDrawSignature->SetName(L"D3D12Device::m_pMultiDrawSignature");

    desc.ByteStride = sizeof(uint32_t) + sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    mdiArgumentDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    result = m_pD3D12Device->CreateCommandSignature(&desc, m_pRootSignature, IID_PPV_ARGS(&m_pMultiDrawIndexedSignature));
    MY_ASSERT(SUCCEEDED(result));
    m_pMultiDrawIndexedSignature->SetName(L"D3D12Device::m_pMultiDrawIndexedSignature");

    desc.ByteStride = sizeof(uint32_t) + sizeof(D3D12_DISPATCH_ARGUMENTS);
    mdiArgumentDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
    result = m_pD3D12Device->CreateCommandSignature(&desc, m_pRootSignature, IID_PPV_ARGS(&m_pMultiDispatchSignature));
    MY_ASSERT(SUCCEEDED(result));
    m_pMultiDispatchSignature->SetName(L"D3D12Device::m_pMultiDispatchSignature");

    desc.ByteStride = sizeof(uint32_t) + sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
    argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
    result = m_pD3D12Device->CreateCommandSignature(&desc, m_pRootSignature, IID_PPV_ARGS(&m_pMultiDispatchMeshSignature));
    MY_ASSERT(SUCCEEDED(result));
    m_pMultiDispatchMeshSignature->SetName(L"D3D12Device::m_pMultiDispatchMeshSignature");
}

D3D12Descriptor D3D12Device::AllocateRTV()
{
    return m_pRTVAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateDSV()
{
    return m_pDSVAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateResourceDescriptor()
{
    return m_pResourceDescriptorAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateSampler()
{
    return m_pSamplerAllocator->Allocate();
}

D3D12Descriptor D3D12Device::AllocateNonShaderVisibleUAV()
{
    return m_pNonShaderVisibleUAVAllocator->Allocate();
}