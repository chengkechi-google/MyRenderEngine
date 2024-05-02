#pragma once

#include "D3D12Headers.h"
#include "../RHIRayTracingBLAS.h"

class D3D12Device;

namespace D3D12MA
{
    class Allocation;
}

class D3D12RayTracingBLAS : public IRHIRayTracingBLAS
{
public:
    D3D12RayTracingBLAS(D3D12Device* pDevice, const RHIRayTracingBLASDesc& desc, const eastl::string name);
    ~D3D12RayTracingBLAS();

    virtual void* GetHandle() const override { return m_pASBuffer; }
    
    bool Create();
    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* GetBuildDesc() const { return &m_buildDesc; }
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return m_pASBuffer->GetGPUVirtualAddress(); }
    void GetUpdateDesc(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc, D3D12_RAYTRACING_GEOMETRY_DESC& geometry, IRHIBuffer* pVertexBuffer, uint32_t vertexBufferOffset);
private:
    eastl::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_geometries;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC m_buildDesc;
    
    ID3D12Resource* m_pASBuffer = nullptr;
    D3D12MA::Allocation* m_pASAllocation = nullptr;
    ID3D12Resource* m_pScratchBuffer = nullptr;
    D3D12MA::Allocation* m_pScratchAllocation = nullptr;
};
