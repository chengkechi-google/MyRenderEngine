#pragma once
#include "D3D12Headers.h"
#include "../RHIBuffer.h"

class D3D12Device;
class D3D12Heap;

namespace D3D12MA
{
    class Allocation;
}

class D3D12Buffer : public IRHIBuffer
{
public:
    D3D12Buffer(D3D12Device* pDevice, const RHIBufferDesc& desc, const eastl::string& name);
    ~D3D12Buffer();

    virtual void* GetHandle() const override { return m_pBuffer; }
    virtual void* GetCPUAddress() const override;
    virtual uint64_t GetGPUAddress() const override;
    virtual uint32_t GetRequiredStagingBufferSize() const override;

    bool Create();

protected:
    ID3D12Resource* m_pBuffer = nullptr;
    D3D12MA::Allocation* m_pAllocation = nullptr;
    void* m_pCPUAddress = nullptr;
    
};