#include "D3D12CommandList.h"
#include "D3D12Device.h"
#include "D3D12Fence.h"
#include "D3D12Texture.h"
#include "D3D12Buffer.h"
#include "D3D12PipelineState.h"
#include "D3D12Heap.h"
#include "D3D12RayTracingBLAS.h"
#include "D3D12RayTracingTLAS.h"
#include "PixRuntime.h"
#include "ags.h"
#include "../RHI.h"
#include "Utils/assert.h"
#include "Utils/profiler.h"
#include "Utils/log.h"

D3D12CommandList::D3D12CommandList(IRHIDevice* pDevice, RHICommandQueue queueType, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_queueType = queueType;
    m_name = name;
}

D3D12CommandList::~D3D12CommandList()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    pDevice->Delete(m_pCommandAllocator);
    pDevice->Delete(m_pCommandList);
}
bool D3D12CommandList::Create()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    D3D12_COMMAND_LIST_TYPE type;

    switch (m_queueType)
    {
        case RHICommandQueue::Graphics:
        {
            type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            m_pCommandQueue = pDevice->GetGraphicsQueue();
#if MICROPROFILE_GPU_TIMERS_D3D12
            m_profileQueue = pDevice->GetProfileGraphicsQueue();
#endif
            break;
        }

        case RHICommandQueue::Compute:
        {
            type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            m_pCommandQueue = pDevice->GetComputeQueue();
#if MICROPROFILE_GPU_TIMERS_D3D12
            m_profileQueue = pDevice->GetProfileComputeQueue();
#endif
            break;
        }

        case RHICommandQueue::Copy:
        {
            type = D3D12_COMMAND_LIST_TYPE_COPY;
            m_pCommandQueue = pDevice->GetCopyQueue();
#if MICROPROFILE_GPU_TIMERS_D3D12
            m_profileQueue = pDevice->GetProfileCopyQueue();
#endif
            break;
        }
        
        default:
        {
            MY_ERROR("[Command List] use unknown queuse type {}", (int) m_queueType);
            break;
        }
    }

    ID3D12Device* pD3D12Device = (ID3D12Device*) pDevice->GetHandle();
    HRESULT hResult = pD3D12Device->CreateCommandAllocator(type, IID_PPV_ARGS(&m_pCommandAllocator));
    if (FAILED(hResult))
    {
        MY_ERROR("[Command List] failed to create command allocator {}", m_name);
        return false;
    }
    m_pCommandAllocator->SetName(string_to_wstring(m_name + "allocator").c_str());

    hResult = pD3D12Device->CreateCommandList(0, type,m_pCommandAllocator, nullptr, IID_PPV_ARGS(&m_pCommandList));     //< Low cost without initial state
    if (FAILED(hResult))
    {
        MY_ERROR("[Command List] failed to create command list {}", m_name);
        return false;
    }

    m_pCommandList->Close();
    return true;
}

void D3D12CommandList::ResetAllocator()
{
    m_pCommandAllocator->Reset();
}

void D3D12CommandList::Begin()
{
    m_pCommandList->Reset(m_pCommandAllocator, nullptr);
    m_pCommandList->SetName(string_to_wstring(m_name).c_str()); //< Need resize string, maybe store wsting in the class
    
    ClearState();
}

void D3D12CommandList::End()
{
    FlushBarriers();
    m_pCommandList->Close();
}

void D3D12CommandList::Wait(IRHIFence* pFence, uint64_t value)
{
    m_pendingWaits.emplace_back(pFence, value);
}

void D3D12CommandList::Signal(IRHIFence* pFence, uint64_t value)
{
    m_pendingSignals.emplace_back(pFence, value);
}

void D3D12CommandList::Submit()
{
    for (size_t i = 0; i < m_pendingWaits.size(); ++i)
    {
        m_pCommandQueue->Wait((ID3D12Fence*) m_pendingWaits[i].first->GetHandle(), m_pendingWaits[i].second);
    }
    m_pendingWaits.clear();

    if (m_commandCount > 0)
    {
        ID3D12CommandList* ppCommandLists[] = { m_pCommandList };
        m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
    }

    for (size_t i = 0; i < m_pendingSignals.size(); ++i)
    {
        m_pCommandQueue->Signal((ID3D12Fence*) m_pendingSignals[i].first->GetHandle(), m_pendingSignals[i].second);
    }
    m_pendingSignals.clear();
}

void D3D12CommandList::ClearState()
{
    m_commandCount = 0;
    m_pCurrentPSO = nullptr;

    if (m_queueType == RHICommandQueue::Graphics || m_queueType == RHICommandQueue::Compute)
    {
        D3D12Device* pDevice = (D3D12Device*) m_pDevice;
        ID3D12DescriptorHeap* heaps[2] = { pDevice->GetResourceDescriptorHeap(), pDevice->GetSamplerDescriptorHeap() };
        m_pCommandList->SetDescriptorHeaps(2, heaps);

        ID3D12RootSignature* pRootSignature = pDevice->GetRootSignature();
        m_pCommandList->SetComputeRootSignature(pRootSignature);
        
        if (m_queueType == RHICommandQueue::Graphics)
        {
            m_pCommandList->SetGraphicsRootSignature(pRootSignature);
        }
    }    
}