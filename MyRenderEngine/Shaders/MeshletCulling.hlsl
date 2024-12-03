#include "GPUScene.hlsli"
#include "Meshlet.hlsli"
#include "Debug.hlsli"
#include "Stats.hlsli"

cbuffer MeshletCullingConstant : register(b0)
{
    uint c_meshletListBufferSRV;
    uint c_meshletListCounterBufferSRV;
    
    uint c_meshletListBufferOffset;
    uint c_dispatchIndex;
    
    uint c_bIsFirstPass;
};

groupshared MeshletPayload s_Payload;

bool Cull(Meshlet meshlet, uint instanceIndex, uint meshletIndex)
{
    InstanceData instanceData = GetInstanceData(instanceIndex);
    float3 center = mul(instanceData.m_mtxWorld, float4(meshlet.m_center, 1.0f)).xyz;
    float radius = meshlet.m_radius * instanceData.m_scale;
    
    // 1. frustum culling
    for(uint i = 0; i < 6; ++ i)
    {
        if(dot(center, GetCameraCB().m_culling.m_planes[i].xyz) + GetCameraCB().m_culling.m_planes[i].w + radius < 0)
        {
            stats(c_bIsFirstPass ? STATS_1ST_PHASE_FRUSTUM_CULLED_MESHLET : STATS_2ND_PHASE_FRUSTUM_CULLED_MESHLET, 1);
            return false;
        }
    }
    
#if !DOUBLE_SIDED
    // 2. back face culling
    int16_t4 cone = unpack_s8s16((int8_t4_packed) meshlet.m_cone);
    float3 axis = cone.xyz / 127.0f;
    float cutoff = cone.w / 127.0f;
    
    axis = normalize(mul(instanceData.m_mtxWorld, float4(axis, 0.0)).xyz);
    float3 view = center - GetCameraCB().m_culling.m_viewPos;
    if(dot(view, -axis) >= cutoff * length(view) + radius)
    {
        stats(c_bIsFirstPass ? STATS_1ST_PHASE_BACKFACE_CULLED_MESHLET : STATS_2ND_PHASE_BACKFACE_CULLED_MESHLET, 1);
        return false;
    }
#endif
    
    // 3. occlusion culling
    Texture2D<float> hzbTexture = ResourceDescriptorHeap[c_bIsFirstPass ? SceneCB.m_firstPhaseCullingHZBSRV : SceneCB.m_secondPhaseCullingHZBSRV];
    uint2 hzbSize = uint2(SceneCB.m_hzbWidth, SceneCB.m_hzbHeight);
    
    if(!OcclusionCulling(hzbTexture, hzbSize, center, radius))
    {
        if(c_bIsFirstPass)
        {
            RWBuffer<uint> counterBuffer = ResourceDescriptorHeap[SceneCB.m_secondPhaseMeshletsCounterUAV];
            RWStructuredBuffer<uint2> occlusionCulledMeshletsBuffer = ResourceDescriptorHeap[SceneCB.m_secondPhaseMeshletsListUAV];
            
            uint culledMeshlets;
            InterlockedAdd(counterBuffer[c_dispatchIndex], 1, culledMeshlets);
            
            occlusionCulledMeshletsBuffer[c_meshletListBufferOffset + culledMeshlets] = uint2(instanceIndex, meshletIndex);
            
            stats(STATS_1ST_PHASE_OCCLUSION_CULLED_MESHLET, 1);
        }
        else
        {
            stats(STATS_1ST_PHASE_OCCLUSION_CULLED_MESHLET, 1);
        }
        
        return false;
    }
    
    stats(c_bIsFirstPass ? STATS_1ST_PHASE_RENDERED_MESHLET : STATS_2ND_PHASE_RENDERED_MESHLET, 1);
    return true;
}

[numthreads(32, 1, 1)]
void as_main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Buffer<uint> counterBuffer = ResourceDescriptorHeap[c_meshletListCounterBufferSRV];
    uint totalMeshletCount = counterBuffer[c_dispatchIndex];
    
    bool bIsVisible = false;
    if(dispatchThreadID.x < totalMeshletCount)
    {
        StructuredBuffer<uint2> meshletListBuffer = ResourceDescriptorHeap[c_meshletListBufferSRV];
        uint2 dataPerMeshlet = meshletListBuffer[c_meshletListBufferOffset + dispatchThreadID.x];
        uint instanceIndex = dataPerMeshlet.x;
        uint meshletIndex = dataPerMeshlet.y;
        
        Meshlet meshLet = LoadSceneStaticBuffer<Meshlet>(GetInstanceData(instanceIndex).m_meshletBufferAddress, meshletIndex);
        bIsVisible = true;//Cull(meshLet, instanceIndex, meshletIndex);
        
        if(c_bIsFirstPass)
        {
            stats(bIsVisible ? STATS_1ST_PHASE_RENDERED_TRIANGLE : STATS_1ST_PHASE_CULLED_TRIANGLE, meshLet.m_triangleCount);
        }else
        {
            stats(bIsVisible ? STATS_2ND_PHASE_RENDERED_TRIANGLE : STATS_2ND_PHASE_CULLED_TRIANGLE, meshLet.m_triangleCount);
        }
        
        if(bIsVisible)
        {
            uint index = WavePrefixCountBits(bIsVisible);
            s_Payload.m_instanceIndices[index] = instanceIndex;
            s_Payload.m_meshletIndices[index] = meshletIndex;
        }
    }
    
    uint visibleMeshletCount = WaveActiveCountBits(bIsVisible);
    DispatchMesh(visibleMeshletCount, 1, 1, s_Payload); //< Dispatch threadgroups of mesh shader
}