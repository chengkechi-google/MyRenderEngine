#pragma once
#include "GlobalConstants.hlsli"

enum class InstanceType : uint
{
    Model = 1 << 0,
};

struct InstanceData
{
    uint m_instanceType;
    uint m_indexBufferAddress;
    uint m_indexStride;
    uint m_triangleCount;
    
    uint m_meshletCount;
    uint m_meshletBufferAddress;
    uint m_meshletVerticesBufferAddress;
    uint m_meshletIndicesBufferAddress;
    
    uint m_posBufferAddress;
    uint m_uvBufferAddress;
    uint m_normalBufferAddress;
    uint m_tangentBufferAddress;
    
    uint m_bVertexAnimation;
    uint m_materialDataAddress;
    uint m_objectID;
    float m_scale;
    
    float3 m_center;
    float m_radius;
    
    uint m_bShowBoundingSphere;
    uint m_bShowTangent;
    uint m_bShowBitangent;
    uint m_bShowNormal;
    
    float4x4 m_mtxWorld;
    float4x4 m_mtxWorldInverseTranspose; //< For normal
    float4x4 m_mtxPrevWorld;
    
    InstanceType GetInstanceType()
    {
        return (InstanceType) m_instanceType;

    }
};

enum class LocalLightType : uint
{
    Point,
    Spot,
    Rect,
};

struct LocalLightData
{
    uint m_lightType;
    float3 m_position;

    float m_radius;
    float3 m_color;

    float m_falloff;
    float3 m_direction;

    float3 m_spotAngles;
    float m_sourceRadius;

    LocalLightType GetLocalLightType()
    {
        return (LocalLightType) m_lightType;
    }
};
    
#ifndef __cplusplus
    
template<typename T>
T LoadSceneStaticBuffer(uint bufferAddress, uint elementID)
{
   ByteAddressBuffer sceneBuffer = ResourceDescriptorHeap[SceneCB.m_sceneStaticBufferSRV];
   return sceneBuffer.Load<T>(bufferAddress + sizeof(T) * elementID);
}

template<typename T>
T LoadSceneAnimationBuffer(uint bufferAddress, uint elementID)
{
    ByteAddressBuffer sceneBuffer = ResourceDescriptorHeap[SceneCB.m_sceneAnimationBufferSRV];
    return sceneBuffer.Load<T>(bufferAddress + sizeof(T) * elementID);
}

template<typename T>
void StoreScenceAnimationBuffer(uint bufferAddress, uint elementID, T value)
{
    RWByteAddressBuffer sceneBuffer = ResourceDescriptorHeap[SceneCB.m_sceneAnimationBufferUAV];
    sceneBuffer.Store<T>(bufferAddress + sizeof(T) * elementID, value);
}

template<typename T>
T LoadSceneConstantBuffer(uint bufferAddress)
{
    ByteAddressBuffer constantBuffer = ResourceDescriptorHeap[SceneCB.m_sceneConstantBufferSRV];
    return constantBuffer.Load<T>(bufferAddress);
}

InstanceData GetInstanceData(uint instanceID)
{
    return LoadSceneConstantBuffer<InstanceData>(SceneCB.m_instanceDataAddress + sizeof(InstanceData) * instanceID);
}

uint3 GetPrimitiveIndices(uint instanceID, uint primitiveID)
{
    InstanceData instanceData = GetInstanceData(instanceID);
    
    if(instanceData.m_indexStride == 2)
    {
        return LoadSceneStaticBuffer<uint16_t3>(instanceData.m_indexBufferAddress, primitiveID);
    }
    else
    {
        return LoadSceneStaticBuffer<uint3>(instanceData.m_indexBufferAddress, primitiveID);
    }
}    

LocalLightData GetLocalLightData(uint lightIndex)
{
    return LoadSceneConstantBuffer<LocalLightData>(SceneCB.m_localLightDataAddress + sizeof(LocalLightData) * lightIndex);
}
#endif // __cplusplus