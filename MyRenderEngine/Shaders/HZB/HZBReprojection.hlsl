#include "../Common.hlsli"

cbuffer RootConstants : register(b0)
{
    uint c_inputSRV;
    uint c_outputUAV;
    uint c_hzbWidth;
    uint c_hzbHeight;
};

[numthreads(8, 8, 1)]
void DepthReprojection(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> prevDepthTexture = ResourceDescriptorHeap[SceneCB.m_prevSceneDepthSRV];
    RWTexture2D<float> reprojectedDepthTexture = ResourceDescriptorHeap[c_outputUAV];
    SamplerState maxReductionSampler = SamplerDescriptorHeap[SceneCB.m_maxReductionSampler];
    
    float2 uv = (dispatchThreadID.xy + 0.5f) / float2(c_hzbWidth, c_hzbHeight);
    
    float prevNDCDepth = prevDepthTexture.SampleLevel(maxReductionSampler, uv, 0);
    
    float4 clipPos = float4((uv * 2.0 - 1.0) * float2(1.0, -1.0), prevNDCDepth, 1.0);
    float4 worldPos = mul(GetCameraCB().m_mtxPrevViewProjectionInverse, clipPos);
    worldPos /= worldPos.w;
    
    float4 reprojectedPosition = mul(GetCameraCB().m_mtxViewProjection, worldPos);
    reprojectedPosition /= reprojectedPosition.w;
    
    float reprojectedDepth = reprojectedPosition.w < 0.0f ? prevNDCDepth : reprojectedPosition.z;
    
    float2 reprojectedUV = GetScreenUV(reprojectedPosition.xy);
    float2 reprojectedScreenPos = reprojectedUV * float2(c_hzbWidth, c_hzbHeight);
    
    reprojectedDepthTexture[reprojectedScreenPos] = saturate(reprojectedDepth);
}

[numthreads(8, 8, 1)]
void DepthDilation(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> reprojectedDepthTexture = ResourceDescriptorHeap[c_inputSRV];
    RWTexture2D<float> hzbMip0UAV = ResourceDescriptorHeap[c_outputUAV];
    
    float depth = reprojectedDepthTexture[dispatchThreadID.xy];
    if(depth == 0.0)
    {
        const int2 offsets[8] = { int2(-1, -1), int2(-1, 0), int2(-1, 1), int2(0, -1), int2(0, 1), int2(1, -1), int2(1, 0), int2(1, 1) };
        
        float minDepth = 1.0f;
        for(int i = 0; i < 8; ++ i)
        {
            float d = reprojectedDepthTexture[dispatchThreadID.xy + offsets[i]];
            if(d > 0.0 && d < minDepth)
            {
                minDepth = d;
            }
        }
        
        if(minDepth != 1.0f)
        {
            depth = minDepth;    
        }
    }
    
    hzbMip0UAV[dispatchThreadID.xy] = depth;
}

[numthreads(8, 8, 1)]
void InitHZB(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> inputDepthTexture = ResourceDescriptorHeap[c_inputSRV];
    RWTexture2D<float> outputDepthTexture = ResourceDescriptorHeap[c_outputUAV];
    SamplerState minReductionSampler = SamplerDescriptorHeap[SceneCB.m_minReductionSampler];
    
    float2 uv = (dispatchThreadID.xy + 0.5) / float2(c_hzbWidth, c_hzbHeight);
    outputDepthTexture[dispatchThreadID.xy] = inputDepthTexture.SampleLevel(minReductionSampler, uv, 0);
}

[numthreads(8, 8, 1)]
void InitSceneHZB(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float> inputDepthTexture = ResourceDescriptorHeap[c_inputSRV];
    RWTexture2D<float2> outputDepthTexture = ResourceDescriptorHeap[c_outputUAV];
    SamplerState minReductionSampler = SamplerDescriptorHeap[SceneCB.m_minReductionSampler];
    SamplerState maxReductionSampler = SamplerDescriptorHeap[SceneCB.m_maxReductionSampler];
    
    float2 uv = (dispatchThreadID.xy + 0.5) / float2(c_hzbWidth, c_hzbHeight);
    
    float min = inputDepthTexture.SampleLevel(minReductionSampler, uv, 0);
    float max = inputDepthTexture.SampleLevel(maxReductionSampler, uv, 0);
    
    outputDepthTexture[dispatchThreadID.xy] = float2(min, max);
}