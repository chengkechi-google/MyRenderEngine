#pragma once
#include "D3D12Headers.h"
#include "../RHIRayTracingTLAS.h"

class D3D12Device;

namespace D3D12MA
{
    class Allocation;
}

class D3D12RayTracingTLAS : public IRHIRayTracingTLAS
{
public:
    D3D12RayTracingTLAS(D3D12Device* pDevice, const RHIRayTracingTLASDesc& desc, const eastl::string name);
    ~D3D12RayTracingTLAS();

    virtual void* GetHandle() const override { return m_pASBuffer; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return m_pASBuffer->GetGPUVirtualAddress(); }

    bool Create();
    void GetBuildDesc(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc, const RHIRayTracingInstance* instances, uint32_t instanceCount);

private:
    ID3D12Resource* m_pASBuffer = nullptr;
    D3D12MA::Allocation* m_pASAllocation = nullptr;
    ID3D12Resource* m_pScratchBuffer = nullptr;
    D3D12MA::Allocation* m_pScratchAllocation = nullptr;
   
    ID3D12Resource* m_pInstanceBuffer = nullptr;
    D3D12MA::Allocation* m_pInstanceBufferAllocation = nullptr;
    void* m_pInstanceBufferCPUAddress = nullptr;
    uint32_t m_nInstanceBufferSize = 0;
    uint32_t m_nCurrentInstanceBufferOffset = 0;
};