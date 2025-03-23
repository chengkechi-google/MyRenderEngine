#include "Model.hlsli"
#include "ShadingModel.hlsli"

model::VertexOutput vs_main(uint vertexID : SV_VertexID)
{
    model::VertexOutput v = model::GetVertexOutput(m_instanceIndex, vertexID);
    return v;
}

struct GBufferOutput
{
    float4 m_diffuseRT : SV_TARGET0;
    float4 m_specularRT : SV_TARGET1;
    float4 m_normalRT : SV_TARGET2;
    float3 m_emissiveRT : SV_TARGET3;
    float4 m_customDataRT : SV_TARGET4;
};

GBufferOutput ps_main(model::VertexOutput input, bool isFrontFace : SV_IsFrontFace)
{
#if UNIFORM_RESOURCE
    uint instanceID = m_instanceIndex;
#else
    uint instanceID = input.m_instanceIndex;
#endif
    
    ShadingModel shadingModel = (ShadingModel)model::GetMaterialConstant(instanceID).m_shadingModel;
    
    model::PBRMetallicRoughness pbrMetallicRoughness = model::GetMaterialMetallicRoughness(instanceID, input.m_uv);
    model::PBRSpecularGlossiness pbrSpecularGlossiness = model::GetMaterialSpecularGlossiness(instanceID, input.m_uv);
    
    float3 normal = input.m_normal;
#if NORMAL_TEXTURE
    normal = model::GetMaterialNormal(instanceID, input.m_uv, input.m_tangent, input.m_bitangent, normal);
#endif
    
#if DOUBLE_SIDE
    normal *= isFrontFace ? 1.0 : -1.0;
#endif
    float3 anisotropyT = model::GetMaterialAnisotropyTangent(instanceID, input.m_uv, input.m_tangent, input.m_bitangent, normal);
    float ao = model::GetMaterialAO(instanceID, input.m_uv);
    float3 emissive = model::GetMaterialEmissive(instanceID, input.m_uv);
    
    float3 diffuse = float3(1, 1, 1);
    float3 specular = float3(0, 0, 0);
    float roughness = 1.0;
    float alpha = 1.0;
    
#if PBR_METALLIC_ROUGHNESS
    diffuse = pbrMetallicRoughness.m_albedo * (1.0 - pbrMetallicRoughness.m_metallic);
    specular = lerp(0.04, pbrMetallicRoughness.m_albedo, pbrMetallicRoughness.m_metallic);
    roughness = pbrMetallicRoughness.m_roughness;
    alpha = pbrMetallicRoughness.m_alpha;
    ao *= pbrMetallicRoughness.m_ao;
#endif // PBR_METALLIC_ROUGHNESS

    if (SceneCB.m_showMeshlets)
    {
        //uint hash = 
    }
    
    GBufferOutput output;
    output.m_diffuseRT = float4(diffuse, 1.0);
    output.m_specularRT = float4(specular, EncodeShadingModel(shadingModel));
    output.m_normalRT = float4(EncodeNormal(normal), roughness);
    output.m_emissiveRT = emissive;
    output.m_customDataRT = float4(0, 0, 0, 0);
    
    return output;
}