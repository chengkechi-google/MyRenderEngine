#include "GPUScene.hlsli"
#include "Meshlet.hlsli"
#include "Model.hlsli"

cbuffer ShowCulledMeshInstanceConstances : register(b0)
{
    uint c_instanceIndex;
    uint c_numInstances;
    uint c_cullingResultSRV;
}

struct Payload
{
    uint m_instanceIndex;
};

// Because every arguments for DispatchMesh must be group-uniform or groupshared, so it is pretty to use 1 more threads to submit mesh shader
[numthreads(1, 1, 1)]
void as_main(uint3 dispatchThread : SV_DispatchThreadID)
{
    Payload payload;
    uint meshletCount = 0;
    payload.m_instanceIndex = c_instanceIndex;
    Buffer<uint> cullingResultBuffer = ResourceDescriptorHeap[c_cullingResultSRV];
    bool bIsVisible = (cullingResultBuffer[c_instanceIndex] == 1 ? true : false);
    InstanceData instanceData = GetInstanceData(c_instanceIndex);
    if(!bIsVisible)
    {
        meshletCount = instanceData.m_meshletCount;
    }
   

    DispatchMesh(meshletCount, 1, 1, payload); // group-uniform or groupshared
}

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void ms_main(uint groupThreadID : SV_GroupThreadID,
             uint groupID : SV_GroupID,
             in payload Payload payload,
             out indices uint3 indices[124],
             out vertices model::VertexOutput vertices[64])
{
    uint instanceIndex = payload.m_instanceIndex;
    uint meshletIndex = groupID;
    
    InstanceData instanceData = GetInstanceData(instanceIndex);
    if (meshletIndex > instanceData.m_meshletCount)
    {
        return;
    }

    Meshlet meshlet = LoadSceneStaticBuffer<Meshlet>(instanceData.m_meshletBufferAddress, meshletIndex);
    SetMeshOutputCounts(meshlet.m_vertexCount, meshlet.m_triangleCount);
    
    if (groupThreadID < meshlet.m_triangleCount)
    {
        uint3 index = uint3(
            LoadSceneStaticBuffer<uint16_t>(instanceData.m_meshletIndicesBufferAddress, meshlet.m_triangleOffset + groupThreadID * 3),
            LoadSceneStaticBuffer<uint16_t>(instanceData.m_meshletIndicesBufferAddress, meshlet.m_triangleOffset + groupThreadID * 3 + 1),
            LoadSceneStaticBuffer<uint16_t>(instanceData.m_meshletIndicesBufferAddress, meshlet.m_triangleOffset + groupThreadID * 3 + 2));

        indices[groupThreadID] = index;
    }

    if (groupThreadID < meshlet.m_vertexCount)
    {
        uint vertexID = LoadSceneStaticBuffer<uint>(instanceData.m_meshletVerticesBufferAddress, meshlet.m_vertexOffset + groupThreadID);
        model::VertexOutput v = model::GetVertexOutput(instanceIndex, vertexID);
        v.m_meshletIndex = meshletIndex;
        v.m_instanceIndex = instanceIndex;
        
        vertices[groupThreadID] = v;
    }
}