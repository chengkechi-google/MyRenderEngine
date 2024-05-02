#include "D3D12Heap.h"
#include "D3D12Device.h"
#include "d3d12ma/D3D12MemAlloc.h"
#include "Utils/log.h"


D3D12Heap::D3D12Heap(D3D12Device* pDevice, const RHIHeapDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

D3D12Heap::~D3D12Heap()
{
    D3D12Device* device = static_cast<D3D12Device*>(m_pDevice);
    device->Delete(m_pAllocation);
}

void* D3D12Heap::GetHandle() const
{
    return m_pAllocation;
}

ID3D12Heap* D3D12Heap::GetHeap() const
{
    return m_pAllocation->GetHeap();
}

bool D3D12Heap::Create()
{
    // Check if smaller 64k
    MY_ASSERT(m_desc.m_size % (64 * 1024) == 0);
    
    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    allocationDesc.HeapType = RHIToD3D12HeapType(m_desc.m_memoryType);
    allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;

    D3D12_RESOURCE_ALLOCATION_INFO allocationInfo = {};
    allocationInfo.SizeInBytes = m_desc.m_size;
    allocationInfo.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    D3D12MA::Allocator *pAllocator = ((D3D12Device *) m_pDevice)->GetResourceAllocator();
    HRESULT result = pAllocator->AllocateMemory(&allocationDesc, &allocationInfo, &m_pAllocation);
    if (FAILED(result))
    {
        MY_ERROR("[D3D12Heap] faield to create");
        return false;
    }
    
    m_pAllocation->SetName(string_to_wstring(m_name).c_str());
    return true;
}