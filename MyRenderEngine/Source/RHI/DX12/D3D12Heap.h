#pragma once

#include "D3D12Headers.h"
#include "../RHIHeap.h"

class D3D12Device;

namespace D3D12MA
{
    class Allocation;
}

class D3D12Heap : public IRHIHeap
{
public:
    D3D12Heap(D3D12Device* pDevice, const RHIHeapDesc& desc, const eastl::string& name);
    ~D3D12Heap();

    virtual void* GetHandle() const override;
    ID3D12Heap* GetHeap() const;

    bool Create();
    
protected:
    D3D12MA::Allocation* m_pAllocation = nullptr;
};
