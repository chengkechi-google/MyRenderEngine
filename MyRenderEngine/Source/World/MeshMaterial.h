#pragma once
#include "Renderer/Renderer.h"
#include "ModelConstants.hlsli"
#include "ShadingModel.hlsli"

class MeshMaterial
{
    friend class GLTFLoader;
    friend class World;

public:
    ~MeshMaterial();

    IRHIPipelineState* GetPSO();
    IRHIPipelineState* GetShadowPSO();
    IRHIPipelineState* GetVelocityPSO();
    IRHIPipelineState* GetIDPSO();
    IRHIPipelineState* GetOutlinePSO();
    IRHIPipelineState* GetMeshletPSO();
    IRHIPipelineState* GetMeshletDirectPSO();
    IRHIPipelineState* GetVertexSkinningPSO();

    void UpdateConstants();
    const ModelMaterialConstant* GetConstants() const { return &m_materialCB; }
    void OnGUI();

    bool IsFrontFaceCCW() const { return m_bFrontFaceCCW; }
    bool IsAlphaTest() const { return m_bAlphaTest; }
    bool IsAlphaBlend() const { return m_bAlphaBlend; }
    bool IsVertexSkinned() const { return m_bSkeletalAnim; } 

private:
    void AddMaterialDefines(eastl::vector<eastl::string>& defines);

private:
    eastl::string m_name;
    ModelMaterialConstant m_materialCB = {};
    
    IRHIPipelineState* m_pPSO = nullptr;
    IRHIPipelineState* m_pShadowPSO = nullptr;
    IRHIPipelineState* m_pVelocityPSO = nullptr;
    IRHIPipelineState* m_pIDPSO = nullptr;
    IRHIPipelineState* m_pOutlinePSO = nullptr;
    IRHIPipelineState* m_pMeshletPSO = nullptr;
    IRHIPipelineState* m_pMeshletDirectPSO = nullptr;
    IRHIPipelineState* m_pVertexSkinningPSO = nullptr;

    ShadingModel m_shadingModel = ShadingModel::Default;

    // PBR specular glossiness
    Texture2D* m_pDiffuseTexture = nullptr;
    Texture2D* m_pSpecularGlossinessTexture = nullptr;
    float3 m_diffuseColor = float3(1.0f, 1.0f, 1.0f);
    float3 m_specularColor = float3(0.0f, 0.0f, 0.0f);
    float m_glossiness = 0.0f;

    // PBR metallic roughness
    Texture2D* m_pAlbedoTexture = nullptr;
    Texture2D* m_pMetallicRoughnessTexture = nullptr;
    float3 m_albedoColor = float3(1.0f, 1.0f, 1.0f);
    float m_metallic = 0.0f;
    float m_roughness = 0.0f;

    Texture2D* m_pNormalTexture = nullptr;
    Texture2D* m_pEmissiveTexture = nullptr;
    Texture2D* m_pAOTexture = nullptr;
    float3 m_emissiveColor = float3(0.0f, 0.0f, 0.0f);
    float m_alphaCutoff = 0.0f;
    bool m_bAlphaTest = false;

    // Anisotropy
    Texture2D* m_pAnisotropicTagentTexture = nullptr;
    float m_anisotropy = 0.5f;
    
    // KHR material sheen
    Texture2D* m_pSheenColorTexture = nullptr;
    Texture2D* m_pSheenRoughnessTexture = nullptr;
    float3 m_sheenColor = float3(0.0f, 0.0f, 0.0f);
    float m_sheenRoughness = 0.0f;

    // KHR material clearcoat
    Texture2D* m_pClearCoatTexture = nullptr;
    Texture2D* m_pClearCoatRoughnessTexture = nullptr;
    Texture2D* m_pClearCoatNormalTexture = nullptr;
    float m_clearCoat = 0.0f;
    float m_clearCoatRoughness = 0.0f;

    bool m_bAlphaBlend = false;
    bool m_bSkeletalAnim = false;
    bool m_bFrontFaceCCW = true;
    bool m_bDoubleSided = false;
    bool m_bPBRSpecularGlossiness = false;
    bool m_bPBRMetallicRoughness = false;
};