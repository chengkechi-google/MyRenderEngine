#include "D3D12RayTracingBLAS.h"
#include "D3D12Device.h"
#include "D3D12Buffer.h"
#include "d3d12ma/D3D12MemAlloc.h"

D3D12RayTracingBLAS::D3D12RayTracingBLAS(D3D12Device* pDevice, const RHIRayTracingBLASDesc& desc, const eastl::string name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

D3D12RayTracingBLAS::~D3D12RayTracingBLAS()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    pDevice->Delete(m_pASBuffer);
    pDevice->Delete(m_pASAllocation);
    pDevice->Delete(m_pScratchBuffer);
    pDevice->Delete(m_pScratchAllocation);
}

bool D3D12RayTracingBLAS::Create()
{
    m_geometries.reserve(m_desc.m_geometries.size());

    for (size_t i = 0; i < m_desc.m_geometries.size(); ++i)
    {
        const RHIRayTracingGeometry& geometry = m_desc.m_geometries[i];
        
        D3D12_RAYTRACING_GEOMETRY_DESC d3d12GeometryDesc = {};
        d3d12GeometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES; //< Todo: Procedually intersection, maybe need AABB info
        d3d12GeometryDesc.Flags = geometry.m_opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
        d3d12GeometryDesc.Triangles.IndexFormat = DXGIFormat(geometry.m_indexFormat);
        d3d12GeometryDesc.Triangles.VertexFormat = DXGIFormat(geometry.m_vertexFormat);
        d3d12GeometryDesc.Triangles.IndexCount = geometry.m_indexCount;
        d3d12GeometryDesc.Triangles.VertexCount = geometry.m_vertexCount;
        d3d12GeometryDesc.Triangles.IndexBuffer = geometry.m_indexBuffer->GetGPUAddress() + geometry.m_indexBufferOffset;
        d3d12GeometryDesc.Triangles.VertexBuffer.StartAddress = geometry.m_vertexBuffer->GetGPUAddress() + geometry.m_vertexBufferOffset;
        d3d12GeometryDesc.Triangles.VertexBuffer.StrideInBytes = geometry.m_vertexStride;

        m_geometries.push_back(d3d12GeometryDesc);    
    }

    ID3D12Device10* pDevice = (ID3D12Device10*) m_pDevice->GetHandle();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInput;
    buildInput.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    buildInput.Flags = RHIToD3D12RTASBuildFlags(m_desc.m_flags);
    buildInput.NumDescs = (UINT) m_geometries.size();
    buildInput.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildInput.pGeometryDescs = m_geometries.data();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&buildInput, &info);

    // RTXMU
    D3D12MA::Allocator* pAllocator = ((D3D12Device*) m_pDevice)->GetResourceAllocator();
    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    CD3DX12_RESOURCE_DESC asBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    CD3DX12_RESOURCE_DESC scratchBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(eastl::max(info.ScratchDataSizeInBytes, info.UpdateScratchDataSizeInBytes), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    pAllocator->CreateResource(&allocationDesc, &asBufferDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, &m_pASAllocation, IID_PPV_ARGS(&m_pASBuffer));
    pAllocator->CreateResource(&allocationDesc, &scratchBufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, &m_pScratchAllocation, IID_PPV_ARGS(&m_pScratchBuffer));

    m_pASBuffer->SetName(string_to_wstring(m_name).c_str());
    m_pScratchBuffer->SetName(string_to_wstring(m_name).c_str());

    m_buildDesc.Inputs = buildInput;
    m_buildDesc.DestAccelerationStructureData = m_pASBuffer->GetGPUVirtualAddress();
    m_buildDesc.ScratchAccelerationStructureData = m_pScratchBuffer->GetGPUVirtualAddress();
    m_buildDesc.SourceAccelerationStructureData = 0;

    return true;
}

void D3D12RayTracingBLAS::GetUpdateDesc(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc, D3D12_RAYTRACING_GEOMETRY_DESC& geometry, IRHIBuffer* vertexBuffer, uint32_t vertexBufferOffset)
{
    MY_ASSERT(m_desc.m_flags & RHIRayTracingASFlagBit::RHIRayTracingASFlagAllowUpdate);
    MY_ASSERT(m_geometries.size() == 1); // Now only support 1 geometry

    geometry = m_geometries[0];
    geometry.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUAddress() + vertexBufferOffset;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS updateInput;
    updateInput.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    updateInput.Flags = RHIToD3D12RTASBuildFlags(m_desc.m_flags) | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    updateInput.NumDescs = 1;
    updateInput.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    updateInput.pGeometryDescs = &geometry;

    desc.Inputs = updateInput;
    desc.DestAccelerationStructureData = m_pASBuffer->GetGPUVirtualAddress();
    desc.SourceAccelerationStructureData = m_pASBuffer->GetGPUVirtualAddress();
    desc.ScratchAccelerationStructureData = m_pScratchBuffer->GetGPUVirtualAddress();
}