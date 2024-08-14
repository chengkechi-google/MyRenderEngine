#pragma once

#include "Common.hlsli"

namespace debug
{
    struct LineVertex
    {
        float3 m_position;
        uint m_color;
    };
    
    void DrawLine(float3 start, float3 end, float3 color)
    {
        RWByteAddressBuffer argumentsBuffer = ResourceDescriptorHeap[SceneCB.m_debugLineDrawCommandUAV];
        RWStructuredBuffer<LineVertex> vertexBuffer = ResourceDescriptorHeap[SceneCB.m_debugLineVertexBufferUAV];
        
        uint vertexCount;
        argumentsBuffer.InterlockedAdd(0, 2, vertexCount);  // Increment vertex count by 2
        
        LineVertex p;
        p.m_color = PackRGBA8Unorm(float4(color, 1.0));
        
        p.m_position = start;
        vertexBuffer[vertexCount] = p;
        p.m_position = end;
        vertexBuffer[vertexCount + 1] = p;
    }
    
    void DrawTriangle(float3 p0, float3 p1, float3 p2, float3 color)
    {
        DrawLine(p0, p1, color);
        DrawLine(p1, p2, color);
        DrawLine(p2, p0, color);
    }

}