cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_outputTexture;
    uint c_samplerState;
    uint c_padding;
    float2 c_pixelSize;
}

[numthreads(8, 8, 1)]
void cs_main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    //Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];
    //SamplerState pointSampler = SamplerDescriptorHeap[c_samplerState];
    
    //float2 uv = ((float2) dispatchThreadID.xy + 0.5f) * c_pixelSize;
    //outputTexture[dispatchThreadID.xy] = inputTexture.SampleLevel(pointSampler, uv, 0).rgba;
    //outputTexture[dispatchThreadID.xy].z = 0.0f;
    outputTexture[dispatchThreadID.xy] = float4(1.0f, 0.0f, 0.0f, 1.0f);
}