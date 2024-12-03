#include "Model.hlsli"
#include "Meshlet.hlsli"

cbuffer MeshletDirectConstants : register(b0)
{
    uint c_instanceIndex;
    uint c_meshletCount;
}

// Per mesh shader output 1 meshlet
[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void ms_main(
    uint groupThreadID : SV_GroupThreadID,
    uint groupID : SV_GroupID,
    in payload MeshletPayload payload,
    out indices uint3 indices[124],                 //< Have to match the mesh optimizaer setting
    out vertices model::VertexOutput vertices[64])  //< Have to match the mesh optimizaer setting
{
    uint instanceIndex = payload.m_instanceIndices[groupID];
    uint meshletIndex = payload.m_meshletIndices[groupID];
    
    InstanceData instanceData = GetInstanceData(instanceIndex);
    if(meshletIndex >= instanceData.m_meshletCount)
    {
        return;    
    }
    
    Meshlet meshlet = LoadSceneStaticBuffer<Meshlet>(instanceData.m_meshletBufferAddress, meshletIndex);
    
    SetMeshOutputCounts(meshlet.m_vertexCount, meshlet.m_triangleCount);
    
    if(groupThreadID < meshlet.m_triangleCount)
    {
        uint3 index = uint3(
            LoadSceneStaticBuffer<uint16_t>(instanceData.m_meshletIndicesBufferAddress, meshlet.m_triangleOffset + groupThreadID * 3),
            LoadSceneStaticBuffer<uint16_t>(instanceData.m_meshletIndicesBufferAddress, meshlet.m_triangleOffset + groupThreadID * 3 + 1),
            LoadSceneStaticBuffer<uint16_t>(instanceData.m_meshletIndicesBufferAddress, meshlet.m_triangleOffset + groupThreadID * 3 + 2));
        
        indices[groupThreadID] = index;
    }
    
    if(groupThreadID < meshlet.m_vertexCount)
    {
        uint vertexID = LoadSceneStaticBuffer<uint>(instanceData.m_meshletVerticesBufferAddress, meshlet.m_vertexOffset + groupThreadID);
        
        model::VertexOutput v = model::GetVertexOutput(instanceIndex, vertexID);
        v.m_meshletIndex = meshletIndex;
        v.m_instanceIndex = instanceIndex;
        
        vertices[groupThreadID] = v;
    }
}

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void ms_direct_main(
    uint groupThreadID : SV_GroupThreadID,
    uint groupID : SV_GroupID,
    out indices uint3 indices[124],
    out vertices model::VertexOutput vertices[64])
{
    uint instanceIndex = c_instanceIndex;
    uint meshletIndex = groupID;
    
    InstanceData instanceData = GetInstanceData(instanceIndex);
    if(meshletIndex > c_meshletCount)
    {
        return;    
    }
    
    Meshlet meshlet = LoadSceneStaticBuffer<Meshlet>(instanceData.m_meshletBufferAddress, meshletIndex);
    SetMeshOutputCounts(meshlet.m_vertexCount, meshlet.m_triangleCount);
    
    if(groupThreadID < meshlet.m_triangleCount)
    {
        uint3 index = uint3(LoadSceneStaticBuffer<uint16_t>(instanceData.m_meshletIndicesBufferAddress, meshlet.m_triangleOffset + groupThreadID * 3),
                            LoadSceneStaticBuffer<uint16_t>(instanceData.m_meshletIndicesBufferAddress, meshlet.m_triangleOffset + groupThreadID * 3 + 1),
                            LoadSceneStaticBuffer<uint16_t>(instanceData.m_meshletIndicesBufferAddress, meshlet.m_triangleOffset + groupThreadID * 3 + 2));
        
        indices[groupThreadID] = index;
    }
    
    if(groupThreadID < meshlet.m_vertexCount)
    {
        uint vertexID = LoadSceneStaticBuffer<uint>(instanceData.m_meshletVerticesBufferAddress, meshlet.m_vertexOffset + groupThreadID);
        
        model::VertexOutput v = model::GetVertexOutput(instanceIndex, vertexID);
        v.m_meshletIndex = meshletIndex;
        v.m_instanceIndex = instanceIndex;
        vertices[groupThreadID] = v;
    }
}
