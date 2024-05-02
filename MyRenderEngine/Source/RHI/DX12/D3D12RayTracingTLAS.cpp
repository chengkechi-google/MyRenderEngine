#include "D3D12RayTracingTLAS.h"
#include "D3D12RayTracingBLAS.h"
#include "D3D12Device.h"
#include "Utils/math.h"
#include "d3d12ma/D3D12MemAlloc.h"

D3D12RayTracingTLAS::D3D12RayTracingTLAS(D3D12Device* pDevice, const RHIRayTracingTLASDesc& desc, const eastl::string name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

D3D12RayTracingTLAS::~D3D12RayTracingTLAS()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;

    pDevice->Delete(m_pASBuffer);
    pDevice->Delete(m_pASAllocation);
    pDevice->Delete(m_pScratchBuffer);
    pDevice->Delete(m_pScratchAllocation);
    pDevice->Delete(m_pInstanceBuffer);
    pDevice->Delete(m_pInstanceBufferAllocation);
}

bool D3D12RayTracingTLAS::Create()
{
    ID3D12Device10* pDevice = (ID3D12Device10*)m_pDevice->GetHandle();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS preBuildDesc = {};
    preBuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    preBuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    preBuildDesc.NumDescs = m_desc.m_instanceCount;
    preBuildDesc.Flags = RHIToD3D12RTASBuildFlags(m_desc.m_flags);

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&preBuildDesc, &info);

    D3D12MA::Allocator* pAllocator = ((D3D12Device*) m_pDevice)->GetResourceAllocator();
    D3D12MA::ALLOCATION_DESC allocationDesc = {};
    allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    CD3DX12_RESOURCE_DESC asBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    CD3DX12_RESOURCE_DESC scratchBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    pAllocator->CreateResource(&allocationDesc, &asBufferDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, &m_pASAllocation, IID_PPV_ARGS(&m_pASBuffer));
    pAllocator->CreateResource(&allocationDesc, &scratchBufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, &m_pScratchAllocation, IID_PPV_ARGS(&m_pScratchBuffer));

    allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
    m_nInstanceBufferSize = RoundUpPow2(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * m_desc.m_instanceCount, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT) * RHI_MAX_INFLIGHT_FRAMES;
    CD3DX12_RESOURCE_DESC instanceBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_nInstanceBufferSize);
    pAllocator->CreateResource(&allocationDesc, &instanceBufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, &m_pInstanceBufferAllocation, IID_PPV_ARGS(&m_pInstanceBuffer));

    m_pASBuffer->SetName(string_to_wstring(m_name).c_str());
    m_pScratchBuffer->SetName(string_to_wstring(m_name).c_str());

    CD3DX12_RANGE readRange(0, 0);
    m_pInstanceBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pInstanceBufferCPUAddress));
    
    return true;
}

void D3D12RayTracingTLAS::GetBuildDesc(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& desc, const RHIRayTracingInstance* instances, uint32_t instanceCount)
{
    MY_ASSERT(instanceCount <= m_desc.m_instanceCount);

    if (m_nCurrentInstanceBufferOffset + sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceCount > m_nInstanceBufferSize)
    {
        m_nCurrentInstanceBufferOffset = 0;
    }

    desc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    desc.Inputs.InstanceDescs = m_pInstanceBuffer->GetGPUVirtualAddress() + m_nCurrentInstanceBufferOffset;
    desc.Inputs.NumDescs = instanceCount;
    desc.Inputs.Flags = RHIToD3D12RTASBuildFlags(m_desc.m_flags);
    desc.DestAccelerationStructureData = m_pASBuffer->GetGPUVirtualAddress();
    desc.ScratchAccelerationStructureData = m_pScratchBuffer->GetGPUVirtualAddress();
    
    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs = (D3D12_RAYTRACING_INSTANCE_DESC*)((char*) m_pInstanceBufferCPUAddress + m_nCurrentInstanceBufferOffset);
    for (uint32_t i = 0; i < instanceCount; ++i)
    {
        memcpy(instanceDescs[i].Transform, instances[i].m_transform, sizeof(float) * 12);
        instanceDescs[i].InstanceID = instances[i].m_instanceID;
        instanceDescs[i].InstanceMask = instances[i].m_instanceMask;
        instanceDescs[i].InstanceContributionToHitGroupIndex = 0; // DXR1.1 doesn't need it
        instanceDescs[i].Flags = RHIToD3D12RTInstanceFlags(instances[i].m_flags);
        instanceDescs[i].AccelerationStructure = ((D3D12RayTracingBLAS*) instances[i].m_blas)->GetGPUAddress();
    }

    m_nCurrentInstanceBufferOffset += sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceCount;
    m_nCurrentInstanceBufferOffset = RoundUpPow2(m_nCurrentInstanceBufferOffset, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);
}