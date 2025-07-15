#include "Common.hlsli"

cbuffer CB : register(b0)
{
    uint c_primitiveCount;
    uint c_vertexBuffeID;
    uint c_vertexBufferOffset;
}

// Im3D::VertexData
struct Vertex
{
    float4 m_positionSize;
    uint m_color
};

struct VertexOutput
{
    float4 m_position : SV_POSITION;
    float4 m_color : COLOR;
#if POINTS || LINES
    float2 m_uv : TEXCOORD0;
    noperspective float size : SIZE;
    noperspective float edgeDistance : EDGE_DISTANCE;
#endif
};

#define ANTIALIASING 2.0

#define GROUP_SIZE 64
#if POINTS || LINES
    #define MAX_VERTEX_COUNT (GROUP_SIZE * 4)
    #define MAX_PRIMITIVE_COUNT (GROUP_SIZE * 2) 
#else
    #define MAX_VERTEX_COUNT (GROUP_SIZE * 3)
    #define MAX_PRIMITIVE_COUNT (GROUP_SIZE)
#endif

groupshared uint s_validPrimitiveCount;

[numthreads(GROUP_SIZE, 1, 1)]
[outputtopology("triangle")]
void ms_main(uint3 dispatchThreadID : SV_DispatchThreadID,
    uint groupIndex : SV_GroupIndex,
    out indices uint3 indices[MAX_PRIMITIVE_COUNT],
    out vertices VertexOutput vertices[MAX_VERTEX_COUNT])
{
    s_validPrimitiveCount = 0;

    uint primitiveIndex = dispatchThreadID.x;
    if (primitiveIndex < c_primitiveCount)
    {
        InterlockedAdd(s_validPrimitiveCount, 1);
    }

    GroupMemoryBarrierWithGroupSync();

#if POINTS || LINES
    SetMeshOutputCounts(s_validPrimitiveCount * 4, s_validPrimitiveCount * 2);
#else
    SetMeshOutputCounts(s_validPrimitiveCount * 3, s_validPrimitiveCount);
#endif

    if (groupIndex < s_validPrimitiveCount)
    {
        ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[c_vertexBuffeID];

#if POINTS
        Vertex vertex = vertexBuffer.Load<Vertex>(c_vertexBufferOffset + sizeof(Vertex) * primitiveIndex);
        float4 clipPos = mul(GetCameraCB().m_mtxViewProjectionNoJitter, float4(vertex.m_positionSize.xyz, 1.0));
        float size = max(vertex.m_positionSize.w, ANTIALIASING);
        float4 color = UnpackRGBA8Unorm(vertex.color).abgr;
        color.a *= smoothstep(0.0, 1.0, vertex.m_positionSize.w / ANTIALIASING);

        float2 scale = SceneCB.m_rcpDisplaySize * size;

        uint v0 = groupIndex * 4 + 0;
        uint v1 = groupIndex * 4 + 1;
        uint v2 = groupIndex * 4 + 2;
        uint v3 = groupIndex * 4 + 3;

        vertices[v0].m_position = float4(clipPos.xy + float2(-1.0, -1.0) * scale * clipPos.w, clipPos.zw);
        vertices[v0].m_uv = float2(0.0, 0.0);
        vertices[v0].m_color = color;
        vertices[v0].m_size = size;
        vertices[v0].m_edgeDistance = 0;

        vertices[v1].m_position = float4(clipPos.xy + float2(1.0, -1.0) * scale * clipPos.w, clipPos.zw);
        vertices[v1].m_uv = float2(1.0, 0.0);
        vertices[v1].m_color = color;
        vertices[v1].m_size = size;
        vertices[v1].m_edgeDistance = 0;

        vertices[v2].m_position = float4(clipPos.xy + float2(-1.0, 1.0) * scale * clipPos.w, clipPos.zw);
        vertices[v2].m_uv = float2(0.0, 1.0);
        vertices[v2].m_color = color;
        vertices[v2].m_size = size;
        vertices[v2].m_edgeDistance = 0;

        vertices[v3].m_position = float4(clipPos.xy + float2(1.0, 1.0) * scale * clipPos.w, clipPos.zw);
        vertices[v3].m_uv = float2(1.0, 1.0);
        vertices[v3].m_color = color;
        vertices[v3].m_size = size;
        vertices[v3].m_edgeDistance = 0;

        uint primitive0 = groupIndex * 2;
        uint primitive1 = groupIndex * 2 + 1;
        indices[primitive0] = uint3(v0, v1, v2);
        indices[primitive1] = uint3(v1, v3, v2);
#endif

#if LINES
        Vertex vertex0 = vertexBuffer.Load<Vertex>(c_vertexBuffeID + sizeof(Vertex) * (primitiveIndex * 2 + 0));
        Vertex vertex1 = vertexBuffer.Load<Vertex>(c_vertexBuffeID + sizeof(Vertex) * (primitiveIndex * 2 + 1));

        float4 clipPos0 = mul(GetCameraCB().m_mtxViewPorjectionNoJitter, float4(vertex0.m_positionSize.xyz, 1.0));
        float4 clipPos1 = mul(GetCameraCB().m_mtxViewProjectionNoJitter, float4(vertex1.m_positionSize.xyz, 1.0));
        float size0 = max(vertex0.m_positionSize.w, ANTIALIASING);
        float size1 = max(vertex1.m_positionSize.w, ANTIALIASING);
        float4 color0 = UnpackRGBA8Unorm(vertex0.m_color).agbr;
        float4 color1 = UnpackRGBA8Unorm(vertex1.m_color).agbr;
        color0.a *= smoothstep(0.0, 1.0, vertex0.m_positionSize.w / ANTIALIASING);
        color1.a *= smoothstep(0.0, 1.0, vertex1.m_positionSize.w / ANTIALIASING);

        float2 pos0 = clipPos0.xy / clipPos0.w;
        float2 pos1 = clipPos1.xy / clipPos1.w;

        float2 dir = pos0 - pos1;
        dir = normalize(float2(dir.x, dir.y * SceneCB.m_displaySize.y / SceneCB.m_displaySize.x)); // correct for aspect rate
        float2 tng0 = float2(-dir.y, dir.x);
        float2 tng1 = tng0 * size1 / SceneCB.m_displaySize;
        tng0 = tng0 * size0 / SceneCB.m_displaySize;

        uint v0 = groupIndex * 4 + 0;
        uint v1 = groupIndex * 4 + 1;
        uint v2 = groupIndex * 4 + 2;
        uint v3 = groupIndex * 4 + 3;

        // Line start
        vertices[v0].m_position = float4(pos0 - tng0) * clipPos0.w, clipPos0.zw);
        vertices[v0].m_color = color0;
        vertices[v0].m_uv = float2(0.0, 0.0);
        vertices[v0].m_size = size0;
        vertices[v0].m_edgeDistance = -size0;

        vertices[v1].m_position = float4(pos0 + tng0) * clipPos0.w, clipPos0.zw);
        vertices[v1].m_color = color0;
        vertices[v1].m_uv = float2(0.0, 0.0);
        vertices[v1].m_size = size0;
        vertices[v1].m_edgeDistance = size0;

        // Line end
        vertices[v2].m_position = float4(pos1 - tng1) * clipPos1.w, clipPos1.zw);
        vertices[v2].m_color = color1;
        vertices[v2].m_uv = float2(1.0, 1.0);
        vertices[v2].m_size = size1;
        vertices[v2].m_edgeDistance = -size1;

        vertices[v3].m_position = float4(pos1 + tng1) * clipPos1.w, clipPos1.zw);
        vertices[v3].m_color = color1;
        vertices[v3].m_uv = float2(1.0, 1.0);
        vertices[v3].m_size = size1;
        vertices[v3].m_edgeDistance = size1;

        uint primitive0 = groupIndex * 2;
        uint primitive1 = groupIndex * 2 + 1;
        indices[primitive0] = uint3(v0, v1, v2);
        indices[primitive1] = uint3(v1, v3, v2);
#endif

#if TRIANGLES
        Vertex vertex0 = vertexBuffer.Load<Vertex>(c_vertexBufferOffset + sizeof(Vertex) * (PrimitiveIndex * 3 + 0));
        Vertex vertex1 = vertexBuffer.Load<Vertex>(c_vertexBufferOffset + sizeof(Vertex) * (PrimitiveIndex * 3 + 1));
        Vertex vertex2 = vertexBuffer.Load<Vertex>(c_vertexBufferOffset + sizeof(Vertex) * (PrimitiveIndex * 3 + 2));

        uint v0 = groupIndex * 4 + 0;
        uint v1 = groupIndex * 4 + 1;
        uint v2 = groupIndex * 4 + 2;

        vertices[v0].m_position = mul(GetCameraCB().m_mtxViewProjectionNoJitter, float4(vertex0.m_positionSize.xyz, 1.0));
        vertices[v0].m_color = UnpackRGBA8Unorm(vertex0.m_color).abgr;
        
        vertices[v1].m_position = mul(GetCameraCB().m_mtxViewProjectionNoJitter, float4(vertex1.m_positionSize.xyz, 1.0));
        vertices[v1].m_color = UnpackRGBA8Unorm(vertex1.m_color).abgr;
        
        vertices[v2].m_position = mul(GetCameraCB().m_mtxViewProjectionNoJitter, float4(vertex2.m_positionSize.xyz, 1.0));
        vertices[v2].m_color = UnpackRGBA8Unorm(vertex2.m_color).abgr;

        indices[groupIndex] = uint3(v0, v1, v2);
#endif
    }
}

float4 ps_main(VertexOutput input) : SV_Target
{
    float4 ret = input.m_color;

#if LINES
    float d = abs(input.m_edgeDistance) / input.m_size;
    d = smoothstep(1.0, 1.0 - (ANTIALIASING / input.m_size), d);
    ret.a *= d;
#elif POINTS
    float d = length(input.m_uv - float2(0.5, 0.5));
    d = smoothstep(0.5, 0.5 - (ANTIALIASING / input.m_size), d);
    ret.a *= d;
#endif

    return ret;
}