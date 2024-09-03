cbuffer CB : register(b0)
{
    uint c_input1;
    uint c_input2;
    uint c_pointSampler;    
};

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

float4 ps_main(VSOutput input) : SV_TARGET
{
    Texture2D inputTexture1 = ResourceDescriptorHeap[c_input1];
    Texture2D inputTexture2 = ResourceDescriptorHeap[c_input2];
    SamplerState pointSampler = SamplerDescriptorHeap[c_pointSampler];
    
    float4 color1 = inputTexture1.SampleLevel(pointSampler, input.m_uv, 0);
    float4 color2 = inputTexture2.SampleLevel(pointSampler, input.m_uv, 0);
    
    return float4(color1.r, color2.g, color1.b, 1.0f);
}