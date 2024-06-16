#pragma once
#include "Texture.hlsli"

struct ModelMaterialConstant
{
    float3 m_albedo;
    float m_alphCutoff;
    
    float m_metallic;
    float3 m_specular;
    
    float m_roughness;
    float3 m_diffuse;
    
    float m_glossiness;
    float3 m_emissive;
    
    uint m_shadingModel;
    float m_anisotropy;
    float m_sheenRoughness;
    float m_clearCoatRoughness;
    
    float3 m_sheenColor;
    float m_clearCoar;
    
    MaterialTextureInfo m_albedoTexture;
    MaterialTextureInfo m_metallicRoughnessTexture;
    MaterialTextureInfo m_normalTexture;
    MaterialTextureInfo m_emissiveTexture;
    MaterialTextureInfo m_aoTexture;
    MaterialTextureInfo m_diffuseTexture;
    MaterialTextureInfo m_specularGlossinessTexture;
    MaterialTextureInfo m_anisotrupyTexture;
    MaterialTextureInfo m_sheenColorTexture;
    MaterialTextureInfo m_sheenRoughnessTexture;
    MaterialTextureInfo m_clearCoatTexture;
    MaterialTextureInfo m_clearCoatRoughnessTexture;
    MaterialTextureInfo m_clearCoatNormalTexture;
    
    uint m_bPBRMatallicRoughness;
    uint m_bPBRSpecularGlossiness;
    uint m_bRGNormalTexture;
    uint m_bDoubleSide;
    
    uint m_bRGClearCoatNormalTexture;
    uint3 _padding;   
};

#ifndef __cplusplus
cbuffer RootConstants : register(b0)
{
    uint m_instanceIndex;
    uint m_prevAnimationPostAddress;    //< Used for velocity pass 
}
#endif