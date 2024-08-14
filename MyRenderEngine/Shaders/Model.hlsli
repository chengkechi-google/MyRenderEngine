#pragma once

#include "Common.hlsli"
#include "ModelConstants.hlsli"
#include "GPUScene.hlsli"
#include "Debug.hlsli"

namespace model
{
    ModelMaterialConstant GetMaterialConstant(uint instanceID)
    {
        return LoadSceneConstantBuffer<ModelMaterialConstant>(GetInstanceData(instanceID).m_materialDataAddress);
    }
    
    struct Vertex
    {
        float3 m_pos;
        float2 m_uv;
        float3 m_normal;
        float4 m_tangent;
    };
    
    struct VertexOutput
    {
        float4 m_pos : SV_Position;
        float2 m_uv : TEXCOORD;
        float3 m_normal : NORMAL;
        float3 m_tangent : TANGENT;
        float3 m_bitangent : BITANGENT;
        float3 m_worldPos : WORLD_POSITION;
        
        nointerpolation uint m_meshletIndex : COLOR0;
        nointerpolation uint m_instanceIndex : COLOR1;
    };
    
    Vertex GetVertex(uint instanceID, uint vertexID)
    {
        InstanceData instanceData = GetInstanceData(instanceID);
        
        Vertex v;
        v.m_uv = LoadSceneStaticBuffer<float2>(instanceData.m_uvBufferAddress, vertexID);
        
        if (instanceData.m_bVertexAnimation)
        {
            v.m_pos = LoadSceneAnimationBuffer<float3>(instanceData.m_posBufferAddress, vertexID);
            v.m_normal = LoadSceneAnimationBuffer<float3>(instanceData.m_normalBufferAddress, vertexID);
            v.m_tangent = LoadSceneAnimationBuffer<float4>(instanceData.m_tangentBufferAddress, vertexID);
        }
        else
        {
            v.m_pos = LoadSceneStaticBuffer<float3>(instanceData.m_posBufferAddress, vertexID);
            v.m_normal = LoadSceneStaticBuffer<float3>(instanceData.m_normalBufferAddress, vertexID);
            v.m_tangent = LoadSceneStaticBuffer<float4>(instanceData.m_tangentBufferAddress, vertexID);
        }
        
        return v;
    }
    
    VertexOutput GetVertexOutput(uint instanceID, uint vertexID)
    {
        InstanceData instanceData = GetInstanceData(instanceID);
        Vertex v = GetVertex(instanceID, vertexID);
        
        float4 worldPos = mul(instanceData.m_mtxWorld, float4(v.m_pos, 1.0));
        
        VertexOutput output = (VertexOutput) 0;
        output.m_pos = mul(GetCameraCB().m_mtxViewProjection, worldPos);
        output.m_worldPos = worldPos.xyz;
        output.m_uv = v.m_uv;
        output.m_normal = normalize(mul(instanceData.m_mtxWorldInverseTranspose, float4(v.m_normal, 1.0)).xyz);
        output.m_tangent = normalize(mul(instanceData.m_mtxWorldInverseTranspose, float4(v.m_tangent.xyz, 1.0)).xyz);
        output.m_bitangent = normalize(cross(output.m_normal, output.m_tangent) * v.m_tangent.w);   //< Tangent w is bitangent sign
        
        // Need debug info
        
        return output;
    }
    
    struct Primitive
    {
        uint m_instanceID;
        uint m_primitiveID;
        Vertex m_v0;
        Vertex m_v1;
        Vertex m_v2;
        float m_lodConstant;
        
        static Primitive Create(uint instanceID, uint primitiveID)
        {
            Primitive primitive;
            primitive.m_instanceID = instanceID;
            primitive.m_primitiveID = primitiveID;
            
            uint3 primitiveIndices = GetPrimitiveIndices(instanceID, primitiveID);
            primitive.m_v0 = GetVertex(instanceID, primitiveIndices.x);
            primitive.m_v1 = GetVertex(instanceID, primitiveIndices.y);
            primitive.m_v2 = GetVertex(instanceID, primitiveIndices.z);
            
            InstanceData instanceData = GetInstanceData(instanceID);
            float3 p0 = mul(instanceData.m_mtxWorld, float4(primitive.m_v0.m_pos, 1.0)).xyz;
            float3 p1 = mul(instanceData.m_mtxWorld, float4(primitive.m_v1.m_pos, 1.0)).xyz;
            float3 p2 = mul(instanceData.m_mtxWorld, float4(primitive.m_v2.m_pos, 1.0)).xyz;
            float2 uv0 = primitive.m_v0.m_uv;
            float2 uv1 = primitive.m_v1.m_uv;
            float2 uv2 = primitive.m_v2.m_uv;
            
            // Not correct, will fix it later
            primitive.m_lodConstant = ComputeTriangleLODConstant(p0, p1, p2, uv0, uv1, uv2);
            
            return primitive;
        }
        
        float2 GetUV(float3 barycentricCoordinates)
        {
            return m_v0.m_uv * barycentricCoordinates.x + m_v1.m_uv * barycentricCoordinates.y + m_v2.m_uv * barycentricCoordinates.z;
        }
        
        float3 GetNormal(float3 barycentricCoordinates)
        {
            return m_v0.m_normal * barycentricCoordinates.x + m_v1.m_normal * barycentricCoordinates.y + m_v2.m_normal * barycentricCoordinates.z;
        }
        
        float4 GetTangent(float3 barycentricCoordinates)
        {
             return m_v0.m_tangent * barycentricCoordinates.x + m_v1.m_tangent * barycentricCoordinates.y + m_v2.m_tangent * barycentricCoordinates.z;
        }

    };
    
    struct PBRMetallicRoughness
    {
        float3 m_albedo;
        float m_alpha;
        float m_metallic;
        float m_roughness;
        float m_ao;
    };
    
    struct PBRSpecularGlossiness
    {
        float3 m_diffuse;
        float m_alpha;
        float3 m_specular;
        float m_glossiness;
    };    
    
    Texture2D GetMaterialTexture2D(uint heapIndex)
    {
#if UNIFORM_RESOURCE
        return ResourceDescriptorHeap[heapIndex];
#else
        return ResourceDescriptorHeap[NonUniformResourceIndex(heapIndex)];
#endif
    }
    
    SamplerState GetMaterialSampler()
    {
#ifdef RAY_TRACING
        return SamplerDescriptorHeap[SceneCB.m_bilinearRepeatSampler];
#else
        return SamplerDescriptorHeap[SceneCB.m_aniso8xSampler];
#endif
    }
    
    float4 SampleMaterialTexture(MaterialTextureInfo textureInfo, float2 uv, float mipLOD)
    {
        Texture2D texture = GetMaterialTexture2D(textureInfo.m_index);
        SamplerState textureSampler = GetMaterialSampler();
        uv = textureInfo.TransformUV(uv);
        
#ifdef RAY_TRACING
        return texture.SampleLevel(textureSampler, uv, mipLOD);
#else
        return texture.Sample(textureSampler, uv);
#endif
    }
    
    bool IsAlbedoTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_albedoTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if ALBEDO_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    bool IsMetallicRoughnessTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_metallicRhoughnessTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if METALLIC_ROUGHNESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }   
    
    bool IsAOMetallicRoughnessTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        retrun GetMaterialConstant(instanceIndex).m_metallicRoughnessTexture.m_index != INVALID_RESOURCE_INDEX && 
        GetMaterialConstant(instanceIndex).m_metallicRoughnessTexture.index == GetMaterialConstant(instanceIndex).m_aoTexture.m_index;
#else
    #if AO_METALLIC_ROUGHNESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    PBRMetallicRoughness GetMaterialMetallicRoughness(uint instanceID, float2 uv, float albedoMipLOD = 0.0, float metallicMipLOD = 0.0)
    {
        ModelMaterialConstant material = GetMaterialConstant(instanceID);
        
        PBRMetallicRoughness pbrMetallicRoughness;
        pbrMetallicRoughness.m_albedo = material.m_albedo;
        pbrMetallicRoughness.m_alpha = 1.0;
        pbrMetallicRoughness.m_metallic = material.m_metallic;
        pbrMetallicRoughness.m_roughness = material.m_roughness;
        pbrMetallicRoughness.m_ao = 1.0;
        
        if(IsAlbedoTextureEnabled(instanceID))
        {
            float4 albedo = SampleMaterialTexture(material.m_albedoTexture, uv, albedoMipLOD);
            pbrMetallicRoughness.m_albedo *= albedo.xyz;
            pbrMetallicRoughness.m_alpha = albedo.w;
        }
        
        if(IsMetallicRoughnessTextureEnabled(instanceID))
        {
            float4 metallicRoughness = SampleMaterialTexture(material.m_metallicRoughnessTexture, uv, metallicMipLOD);
            pbrMetallicRoughness.m_metallic *= metallicRoughness.b;
            pbrMetallicRoughness.m_roughness *= metallicRoughness.g;
            
            if(IsAOMetallicRoughnessTextureEnabled(instanceID))
            {
                pbrMetallicRoughness.m_ao = metallicRoughness.r;
            }
        }
        
        return pbrMetallicRoughness;
    }
    
    bool IsDiffuseTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_diffuseTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if DIFFUSE_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    bool IsSpecularGlossinessTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_specularGlossinessTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if SPECULAR_GLOSSINESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    PBRSpecularGlossiness GetMaterialSpecularGlossiness(uint instanceID, float2 uv, float diffuseMipLOD = 0.0, float specularMipLOD = 0.0)
    {
        ModelMaterialConstant material = GetMaterialConstant(instanceID);
        
        PBRSpecularGlossiness pbrSpecularGlossiness;
        pbrSpecularGlossiness.m_diffuse = material.m_diffuse;
        pbrSpecularGlossiness.m_specular = material.m_specular;
        pbrSpecularGlossiness.m_glossiness = material.m_glossiness;
        pbrSpecularGlossiness.m_alpha = 1.0;
        
        if(IsDiffuseTextureEnabled(instanceID))
        {
            float4 diffuse = SampleMaterialTexture(material.m_diffuseTexture, uv, diffuseMipLOD);
            pbrSpecularGlossiness.m_diffuse *= diffuse.xyz;
            pbrSpecularGlossiness.m_alpha = diffuse.w;            
        }
        
        if(IsSpecularGlossinessTextureEnabled(instanceID))
        {
            float4 specularGlossiness = SampleMaterialTexture(material.m_specularGlossinessTexture, uv, specularMipLOD);
            pbrSpecularGlossiness.m_specular *= specularGlossiness.xyz;
            pbrSpecularGlossiness.m_glossiness *= specularGlossiness.w;
        }
        
        return pbrSpecularGlossiness;
    }
    
    bool IsNormalTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_normalTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if NORMAL_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    bool IsRGNormalTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_bRGNormalTexture;
#else
    #if RG_NORMAL_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    float3 GetMaterialNormal(uint instanceID, float2 uv, float3 T, float3 B, float3 N, float mipLOD = 0.0f)
    {
        ModelMaterialConstant material = GetMaterialConstant(instanceID);
        float3 normal = SampleMaterialTexture(material.m_normalTexture, uv, mipLOD).xyz;
        
        if(IsRGNormalTextureEnabled(instanceID))
        {
            normal.xy = normal.xy * 2.0f - 1.0f;
            normal.z = sqrt(1.0 - normal.x * normal.x - normal.y * normal.y);
        }
        else
        {
            normal = normal * 2.0 - 1.0;
        }
        
        N = normalize(normal.x * T + normal.y * B + normal.z * N);
        return N;
    }
    
    bool IsAOTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_aoTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if AO_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    float GetMaterialAO(uint instanceID, float2 uv, float mipLOD = 0.0)
    {
        float ao = 1.0;
        
        if(IsAOTextureEnabled(instanceID))
        {
            ModelMaterialConstant material = GetMaterialConstant(instanceID);
            ao = SampleMaterialTexture(material.m_aoTexture, uv, mipLOD).x;
        }
        
        return ao;
    }
    
    bool IsEmissiveTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_emissiveTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if EMISSIVE_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    float3 GetMaterialEmissive(uint instanceID, float2 uv, float mipLOD = 0.0)
    {
        ModelMaterialConstant material = GetMaterialConstant(instanceID);
        float3 emissive = material.m_emissive;
        if(IsEmissiveTextureEnabled(instanceID))
        {
            emissive *= SampleMaterialTexture(material.m_emissiveTexture, uv, mipLOD).xyz;
        }
        return emissive;
    }
    
    bool IsAnisotropyTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_anisotropyTexture.m_index = INVALID_RESOURCE_INDEX;
#else
    #if ANISOTROPY_TANGENT_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    float3 GetMaterialAnisotropyTangent(uint instanceID, float2 uv, float3 T, float3 B, float3 N, float mipLOD = 0.0)
    {
        float3 anisotropyT = T;
        if(IsAnisotropyTextureEnabled(instanceID))
        {
            ModelMaterialConstant material = GetMaterialConstant(instanceID);
            float2 anisotropy = SampleMaterialTexture(material.m_anisotrupyTexture, uv, mipLOD).xy * 2.0 - 1.0;
            anisotropyT = T * anisotropy.x + B * anisotropy.y;
        }
        
        anisotropyT = normalize(anisotropyT - dot(anisotropyT, N) * N); //< reproject on normal plane, should not happen, because T & B are on the normal plane
        return anisotropyT;
    }
    
    bool IsSheenColorTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_sheenColorTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if SHEEN_COLOR_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    bool IsSheenColorRoughnessTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_sheenColorTexture.m_index != INVALID_RESOURCE_INDEX &&
            GetMaterialConstant(instanceID).m_sheenColorTexture.m_index == GetMaterialConstant(instanceID).m_sheenRoughnessTexture.m_index;
#else
    #if SHENN_COLOR_ROUGHNESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    bool IsSheenRoughnessTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_sheenRoughnessTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if SHEEN_ROUGHNESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    float4 GetMaterialSheenColorAndRoughness(uint instanceID, float2 uv, float mipLOD = 0.0)
    {
        ModelMaterialConstant material = GetMaterialConstant(instanceID);
        float3 sheenColor = material.m_sheenColor;
        float sheenRoughness = material.m_sheenRoughness;
        
        if(IsSheenColorTextureEnabled(instanceID))
        {
            float4 sheenTexture = SampleMaterialTexture(material.m_sheenColorTexture, uv, mipLOD);
            sheenColor *= sheenTexture.xyz;
            
            if(IsSheenColorRoughnessTextureEnabled(instanceID))
            {
                sheenRoughness *= sheenTexture.w;
            }
        }
        
        if(IsSheenRoughnessTextureEnabled(instanceID) && !IsSheenColorRoughnessTextureEnabled(instanceID))
        {
            sheenRoughness *= SampleMaterialTexture(material.m_sheenRoughnessTexture, uv, mipLOD).w;
        }
        
        return float4(sheenColor, sheenRoughness);
    }
    
    bool IsClearCoatTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_clearCoatTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if CLEAR_COAT_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    bool IsClearCoatRoughnessCombinedTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_clearCoatTexture.m_index != INVALID_RESOURCE_INDEX &&
            GetMaterialConstant(instanceID).m_clearCoatTexture.m_index == GetMaterialConstant(instanceID).m_clearCoatRoughnessTexture.m_index;
#else
    #if CLEAR_COAT_ROUGHNESS_COMBINED_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    bool IsClearCoatRoughnessTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_clearCoatRoughnessTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if CLEAR_COAT_ROUGHNESS_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    float2 GetMaterialClearCoatAndRoughness(uint instanceID, float2 uv, float mipLOD = 0.0)
    {
        ModelMaterialConstant material = GetMaterialConstant(instanceID);
        float clearCoat = material.m_clearCoat;
        float clearCoatRoughness = material.m_clearCoatRoughness;
        
        if(IsClearCoatTextureEnabled(instanceID))
        {
            float4 clearCoatTexture = SampleMaterialTexture(material.m_clearCoatTexture, uv, mipLOD);
            clearCoat *= clearCoatTexture.r;
            
            if(IsClearCoatRoughnessCombinedTextureEnabled(instanceID))
            {
                clearCoatRoughness *= clearCoatTexture.g;
            }
        }
        
        if(IsClearCoatRoughnessTextureEnabled(instanceID) && !IsClearCoatRoughnessCombinedTextureEnabled(instanceID))
        {
            clearCoatRoughness *= SampleMaterialTexture(material.m_clearCoatRoughnessTexture, uv, mipLOD).g;
        }
        
        return float2(clearCoat, clearCoatRoughness);
    }
    
    bool IsClearCoatNormalTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_clearCoatNormalTexture.m_index != INVALID_RESOURCE_INDEX;
#else
    #if CLEAR_COAT_NORMAL_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    bool IsRGClearCoatNormalTextureEnabled(uint instanceID)
    {
#ifdef RAY_TRACING
        return GetMaterialConstant(instanceID).m_bRGClearCoatNormalTexture;
#else
    #if RG_CLEAR_COAT_NORMAL_TEXTURE
        return true;
    #else
        return false;
    #endif
#endif
    }
    
    float3 GetMaterialClearCoatNormal(uint instanceID, float2 uv, float3 T, float3 B, float3 N, float mipLOD = 0.0)
    {
        ModelMaterialConstant material = GetMaterialConstant(instanceID);
        
        float3 normal = SampleMaterialTexture(material.m_clearCoatNormalTexture, uv, mipLOD).xyz;
        if(IsRGClearCoatNormalTextureEnabled(instanceID))
        {
            normal.xy = normal.xy * 2.0 - 1.0;
            normal.z = sqrt(1.0 - normal.x * normal.x - normal.y * normal.y);
        }
        else
        {
            normal = normal * 2.0 - 1.0;
        }
        
        N = normalize(normal.x * T + normal.y * B + normal.z * N);
        return N;
    }
    
    void AlphaTest(uint instanceID, float2 uv)
    {
        float alpha = 1.0;
        ModelMaterialConstant material = GetMaterialConstant(instanceID);
#if ALBEDO_TEXTURE
        alpha = SampleMaterialTexture(material.m_albedoTexture, uv, 0).a;
#elif DIFFUSE_TEXTURE
        alpha = SampleMaterialTexture(material.m_diffuseTexture, uv, 0).a;
#endif
        
        clip(alpha - material.m_alphCutoff);
    }
}