#pragma once
#include "D3D12Headers.h"
#include "../RHIFence.h"

class D3D12Device;

class D3D12Fence : public IRHIFence
{
public:
    D3D12Fence(D3D12Device *pDevice, eastl::string name);
    ~D3D12Fence();

    virtual void* GetHandle() const override { return m_pFence; }
    virtual void Wait(uint64_t value) override;
    virtual void Signal(uint64_t value) override; 

    bool Create();

private:
    ID3D12Fence* m_pFence = nullptr;
    HANDLE m_hEvent = nullptr;
};