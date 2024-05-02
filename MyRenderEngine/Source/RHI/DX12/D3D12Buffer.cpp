#include "D3D12Buffer.h"
#include "D3D12Device.h"
#include "D3D12Heap.h"
#include "d3d12ma/D3D12MemAlloc.h"
#include "Utils/log.h"

D3D12Buffer::D3D12Buffer(D3D12Device* pDevice, const RHIBufferDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

D3D12Buffer::~D3D12Buffer()
{
    D3D12Device* pDevice = static_cast<D3D12Device*>(m_pDevice);
    pDevice->Delete(m_pBuffer);
    pDevice->Delete(m_pAllocation);
}

void* D3D12Buffer::GetCPUAddress() const
{
    return m_pCPUAddress;
}

uint64_t D3D12Buffer::GetGPUAddress() const
{
    return m_pBuffer->GetGPUVirtualAddress();
}

bool D3D12Buffer::Create()
{
    D3D12MA::Allocator* pAllocator = ((D3D12Device*)m_pDevice)->GetResourceAllocator();
    
    D3D12_RESOURCE_FLAGS flags = (m_desc.m_usage & RHIBufferUsageUnorderedAccess) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_DESC1 resourceDesc1 = CD3DX12_RESOURCE_DESC1::Buffer(m_desc.m_size, flags);

    D3D12_BARRIER_LAYOUT initialLayout = D3D12_BARRIER_LAYOUT_UNDEFINED;
    HRESULT result;

    if (m_desc.m_pHeap != nullptr)
    {
        MY_ASSERT(m_desc.m_allocationType ==  RHIAllocationType::Placed);
        MY_ASSERT(m_desc.m_memoryType == m_desc.m_pHeap->GetDesc().m_memoryType);
        MY_ASSERT(m_desc.m_size + m_desc.m_heapOffset <= m_desc.m_pHeap->GetDesc().m_size);

        result = pAllocator->CreateAliasingResource2((D3D12MA::Allocation*) m_desc.m_pHeap->GetHandle(), m_desc.m_heapOffset, &resourceDesc1,
                                                     initialLayout, nullptr, 0, nullptr, IID_PPV_ARGS(&m_pBuffer));
    }
    else
    {
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = RHIToD3D12HeapType(m_desc.m_memoryType);
        allocationDesc.Flags = m_desc.m_allocationType == RHIAllocationType::Comitted ? D3D12MA::ALLOCATION_FLAG_COMMITTED : D3D12MA::ALLOCATION_FLAG_NONE;
        
        result = pAllocator->CreateResource3(&allocationDesc, &resourceDesc1, initialLayout, nullptr, 0, nullptr, &m_pAllocation, IID_PPV_ARGS(&m_pBuffer));
    }

    if (FAILED(result))
    {
        MY_ERROR("[D3D12Buffer] failed to create");
        return false;
    }

    eastl::wstring name = string_to_wstring(m_name);
    m_pBuffer->SetName(name.c_str());
    if (m_pAllocation)
    {
        m_pAllocation->SetName(name.c_str());
    }

    if (m_desc.m_memoryType != RHIMemoryType::GPUOnly)
    {
        CD3DX12_RANGE readRange(0, 0);
        m_pBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCPUAddress));
    }

    return true;
}

uint32_t D3D12Buffer::GetRequiredStagingBufferSize() const
{
    ID3D12Device* pDevice = static_cast<ID3D12Device*> (m_pDevice->GetHandle());
    D3D12_RESOURCE_DESC desc = m_pBuffer->GetDesc();

    uint64_t size = 0;
    pDevice->GetCopyableFootprints(&desc, 0, 1, 0, nullptr, nullptr, nullptr, &size);
    
    return size;
}

