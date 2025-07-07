#include "Common.hlsli"
#include "GPUScene.hlsli"

cbuffer CB : register(b0)
{
    uint c_spiriteCount;
    uint c_spriteBufferAddress;
}

struct Sprite
{
    float3 m_position;
    float m_size;
    
    uint m_color;
    uint m_texture;
    uint m_objectID;
    float m_distance;
};

struct VertexOut
{
    float4 m_position : SV_POSITION;
    float2 m_uv : TEXCOORD;
#if OBJECT_ID_PASS
    nointerpolation uint m_objectID : OBJECT_ID;
#else
    float4 m_color : COLOR;
#endif
    nointerpolation  uint m_textureID : TEXTURE_INDEX;
};

#define GROUP_SIZE (64)
#define MAX_VERTEX_COUNT (GROUP_SIZE * 4)
#define MAX_PRIMITIVE_COUNT (GROUP_SIZE * 2)

groupshared uint s_validSpritCount;

[numthreads(GROUP_SIZE, 1, 1)]
[outputtopology("triangle")]
void ms_main(uint3 dispatchThreadID : SV_DispatchThreadID,
    uint groupIndex : SV_GroupIndex,
    out indices uint3 indices[MAX_PRIMITIVE_COUNT],
    out vertices VertexOut vertices[MAX_VERTEX_COUNT])
{
    s_validSpritCount = 0;

    uint spriteIndex = dispatchThreadID.x; 
    if (spriteIndex < c_spiriteCount)
    {
        InterlockedAdd(s_validSpritCount, 1);
    }

    GroupMemoryBarrierWithGroupSync();

    SetMeshOutputCounts(s_validSpritCount * 4, s_validSpritCount * 2);

    if (groupIndex < s_validSpritCount)
    {
        uint v0 = groupIndex * 4 + 0;
        uint v1 = groupIndex * 4 + 1;
        uint v2 = groupIndex * 4 + 2;
        uint v3 = groupIndex * 4 + 3;

        uint primitive0 = groupIndex * 2;
        uint primitive1 = groupIndex * 2 + 1;

        Sprite sprite = LoadSceneConstantBuffer<Sprite>(c_spriteBufferAddress + sizeof(Sprite) * spriteIndex);

        float4 clipPos = mul(GetCameraCB().m_mtxViewProjectionNoJitter, float4(sprite.m_position, 1.0));

        float x0 = clipPos.x - SceneCB.m_rcpDisplaySize.x * clipPos.w * sprite.m_size;
        float y0 = clipPos.y + SceneCB.m_rcpDisplaySize.y * clipPos.w * sprite.m_size;
        float x1 = clipPos.x + SceneCB.m_rcpDisplaySize.x * clipPos.w * sprite.m_size;
        float y1 = clipPos.y - SceneCB.m_rcpDisplaySize.y * clipPos.w * sprite.m_size;

        vertices[v0].m_position = float4(x0, y0, clipPos.zw);
        vertices[v1].m_position = float4(x0, y1, clipPos.zw);
        vertices[v2].m_position = float4(x1, y0, clipPos.zw);
        vertices[v3].m_position = float4(x1, y1, clipPos.zw);

        vertices[v0].m_uv = float2(0.0, 0.0);
        vertices[v1].m_uv = float2(0.0, 1.0);
        vertices[v2].m_uv = float2(1.0, 0.0);
        vertices[v3].m_uv = float2(1.0, 1.0);

#if OBJECT_ID_PASS
        vertices[v0].m_objectID = sprite.m_objectID;
        vertices[v1].m_objectID = sprite.m_objectID;
        vertices[v2].m_objectID = sprite.m_objectID;
        vertices[v3].m_objectID = sprite.m_objectID;
#else
        float4 color = UnpackRGBA8Unorm(sprite.m_color);
        vertices[v0].m_color = color;
        vertices[v1].m_color = color;
        vertices[v2].m_color = color;
        vertices[v3].m_color = color;
#endif
        vertices[v0].m_textureID = sprite.m_texture;
        vertices[v1].m_textureID = sprite.m_texture;
        vertices[v2].m_textureID = sprite.m_texture;
        vertices[v3].m_textureID = sprite.m_texture;

        indices[primitive0] = uint3(v0, v1, v2);
        indices[primitive1] = uint3(v1, v3, v2);
    }
}

#if OBJECT_ID_PASS
uint ps_main(VertexOut input) : SV_Target
{
    Texture2D texture = ResourceDescriptorHeap[NonUniformResourceIndex(input.m_textureID)];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.m_bilinearClampSampler];

    float4 color = texture.SampleLevel(linearSampler, input.m_uv, 0);
    return color.a > 0 ? input.m_objectID : 0;
}
#else
float4 ps_main(VertexOut input) : SV_Target
{
    Texture2D texture = ResourceDescriptorHeap[NonUniformResourceIndex(input.m_textureID)];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.m_bilinearClampSampler];

    float4 color = texture.SampleLevel(linearSampler, input.m_uv, 0);
    color *= input.m_color;

    return color;
}
#endif
