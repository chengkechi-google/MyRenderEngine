struct VSOutput
{
    float4 m_pos : SV_Position;
    float2 m_uv : TEXCOORD;    
};

VSOutput vs_main(uint vertexID : SV_VertexID)
{
    VSOutput output;
    output.m_pos.x = (float) (vertexID / 2) * 4.0 - 1.0;
    output.m_pos.y = (float) (vertexID % 2) * 4.0 - 1.0;
    output.m_pos.z = 0.0;
    output.m_pos.w = 1.0;
    output.m_uv.x = (float) (vertexID / 2) * 2.0;
    output.m_uv.y = 1.0 - (float) (vertexID % 2) * 2.0;
    return output;
}

cbuffer CB : register(b0)
{
    uint c_inputTexture;
    uint c_depthTexture;
    uint c_pointSampler;
    uint padding0;
}

float4 ps_main(VSOutput input) : SV_TARGET
{
    Texture2D inputTexture = ResourceDescriptorHeap[c_inputTexture];
    SamplerState pointSampler = SamplerDescriptorHeap[c_pointSampler];

    return inputTexture.SampleLevel(pointSampler, input.m_uv, 0);
}