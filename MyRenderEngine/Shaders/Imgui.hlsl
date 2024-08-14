cbuffer ResourceCB : register(b0)
{
    uint c_VertexBufferID;
    uint c_VertexOffset;
    uint c_TextureID;
    uint c_SamplerID;
};

cbuffer CB : register(b1)
{
    float4x4 ProjectionMatrix;
}

struct Vertex
{
    float2 m_pos;
    float2 m_uv;
    uint m_color;
};

#define IM_COLOR32_R_SHIFT 0
#define IM_COLOR32_G_SHIFT 8
#define IM_COLOR32_B_SHIFT 16
#define IM_COLOR32_A_SHIFT 24

float4 UnpackColor(uint color)
{
    float s = 1.0f / 255.0f;
    return float4(((color >> IM_COLOR32_R_SHIFT) & 0xFF) * s,
                ((color >> IM_COLOR32_G_SHIFT) & 0xFF) * s,
                ((color >> IM_COLOR32_B_SHIFT) & 0xFF) * s,
                ((color >> IM_COLOR32_A_SHIFT) & 0xFF) * s);
}

struct PSInput
{
    float4 m_pos    : SV_POSITION;
    float4 m_color  : COLOR0;
    float2 m_uv     : TEXCOORD0;
};

PSInput vs_main(uint vertexID : SV_VertexID)
{
    StructuredBuffer<Vertex> VB = ResourceDescriptorHeap[c_VertexBufferID];
    Vertex vertex = VB[vertexID + c_VertexOffset];
    
    PSInput output;
    output.m_pos = mul(ProjectionMatrix, float4(vertex.m_pos.xy, 0.0f, 1.0f));
    output.m_color = UnpackColor(vertex.m_color);
    output.m_uv = vertex.m_uv;
        
    return output;
}

float4 ps_main(PSInput input) : SV_TARGET
{
    Texture2D texture = ResourceDescriptorHeap[c_TextureID];
    SamplerState linearSampler = SamplerDescriptorHeap[c_SamplerID];
    
    return input.m_color * texture.Sample(linearSampler, input.m_uv);
}