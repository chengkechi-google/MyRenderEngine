#include "../Common.hlsli"
#include "../GPUScene.hlsli"

cbuffer FrustumLightCulling : register(b0)
{
    uint c_lightCount;
    uint c_culledLightCountBuffer;
    uint c_culledLightBuffer;
}

[numthreads(64, 1, 1)]
void cs_frustum_culling_main(uint3 dispatchThreadID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    if (dispatchThreadID.x < c_lightCount)
    {
        RWBuffer<uint> frustumCullingLightCountBuffer = ResourceDescriptorHeap[c_culledLightCountBuffer];
        RWBuffer<uint> frustumCullingLightIDBuffer = ResourceDescriptorHeap[c_culledLightBuffer];
        LocalLightData lightData = GetLocalLightData(dispatchThreadID.x);
        bool visible = FrustumCulling(lightData.m_position, lightData.m_radius);
        if (visible)
        {
            uint lightID = 0;
            InterlockedAdd(frustumCullingLightCountBuffer[0], 1, lightID);
            frustumCullingLightIDBuffer[lightID] = dispatchThreadID.x;
        }
    }
}