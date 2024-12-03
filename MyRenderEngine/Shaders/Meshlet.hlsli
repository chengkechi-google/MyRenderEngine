#pragma once

struct Meshlet
{
    float3 m_center;
    float m_radius;
    
    uint m_cone; //< axis + cutoff, rgba8snorm
    
    uint m_vertexCount;
    uint m_triangleCount;
    
    uint m_vertexOffset;
    uint m_triangleOffset;
};

struct MeshletPayload
{
    uint m_instanceIndices[32];
    uint m_meshletIndices[32];
};