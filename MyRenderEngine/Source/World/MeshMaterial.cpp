#include "MeshMaterial.h"
#include "ResourceCache.h"
#include "Core/Engine.h"
#include "Utils/guiUtil.h"

MeshMaterial::~MeshMaterial()
{
    ResourceCache* pCache = ResourceCache::GetInstance();
    pCache->ReleaseTexture2D(m_pDiffuseTexture);
    pCache->ReleaseTexture2D(m_pSpecularGlossinessTexture);
    pCache->ReleaseTexture2D(m_pAlbedoTexture);
    pCache->ReleaseTexture2D(m_pMetallicRoughnessTexture);
    pCache->ReleaseTexture2D(m_pNormalTexture);
    pCache->ReleaseTexture2D(m_pEmissiveTexture);
    pCache->ReleaseTexture2D(m_pAOTexture);
    pCache->ReleaseTexture2D(m_pAnisotropicTagentTexture);
    pCache->ReleaseTexture2D(m_pSheenColorTexture);
    pCache->ReleaseTexture2D(m_pSheenRoughnessTexture);
    pCache->ReleaseTexture2D(m_pClearCoatTexture);
    pCache->ReleaseTexture2D(m_pClearCoatNormalTexture);
    pCache->ReleaseTexture2D(m_pClearCoatRoughnessTexture);
}

IRHIPipelineState* MeshMaterial::GetPSO()
{
    if (m_pPSO != nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    
        eastl::vector<eastl::string> defines;
        AddMaterialDefines(defines);
        defines.push_back("UNIFORM_RESOURCE=1");

        RHIGraphicsPipelineDesc psoDesc;
        psoDesc.m_pVS = pRenderer->GetShader("Model.hlsl", "vs_main", "vs_6_6", defines);
        psoDesc.m_pPS = pRenderer->GetShader("Model.hlsl", "ps_main", "ps_6_6", defines);
        psoDesc.m_rasterizerState.m_cullMode = m_bDoubleSided ? RHICullMode::None : RHICullMode::Back;
        psoDesc.m_rasterizerState.m_frontCCW = m_bFrontFaceCCW;
        psoDesc.m_depthStencilState.m_depthTest = true;
        psoDesc.m_depthStencilState.m_depthFunc = RHICompareFunc::GreaterEqual;     //Use reversed depth
        psoDesc.m_rtFormat[0] = RHIFormat::RGBA8SRGB;
        psoDesc.m_rtFormat[1] = RHIFormat::RGBA8SRGB;
        psoDesc.m_rtFormat[2] = RHIFormat::RGBA8UNORM;
        psoDesc.m_rtFormat[3] = RHIFormat::R11G11B10F;
        psoDesc.m_rtFormat[4] = RHIFormat::RGBA8UNORM;
        psoDesc.m_depthStencilFromat = RHIFormat::D32F;

        m_pPSO = pRenderer->GetPipelineState(psoDesc, "Model PSO");
    }

    return m_pPSO;
}

IRHIPipelineState* MeshMaterial::GetMeshletPSO()
{
    if (m_pMeshletPSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        eastl::vector<eastl::string> defines;
        AddMaterialDefines(defines);

        RHIMeshShaderPipelineDesc psoDesc;
        psoDesc.m_pAS = pRenderer->GetShader("MeshletCulling.hlsl", "as_main", "as_6_6", defines);
        psoDesc.m_pMS = pRenderer->GetShader("ModelMeshlet.hlsl", "ms_main", "ms_6_6", defines);
        psoDesc.m_pPS = pRenderer->GetShader("Model.hlsl", "ps_main", "ps_6_6", defines);
        psoDesc.m_rasterizerState.m_cullMode = m_bDoubleSided ? RHICullMode::None : RHICullMode::Back;
        psoDesc.m_rasterizerState.m_frontCCW = m_bFrontFaceCCW;
        psoDesc.m_depthStencilState.m_depthTest = true;
        psoDesc.m_depthStencilState.m_depthFunc = RHICompareFunc::GreaterEqual;
        psoDesc.m_rtFormat[0] = RHIFormat::RGBA8SRGB;
        psoDesc.m_rtFormat[1] = RHIFormat::RGBA8SRGB;
        psoDesc.m_rtFormat[2] = RHIFormat::RGBA8UNORM;
        psoDesc.m_rtFormat[3] = RHIFormat::R11G11B10F;
        psoDesc.m_rtFormat[4] = RHIFormat::RGBA8UNORM;

        m_pMeshletPSO = pRenderer->GetPipelineState(psoDesc, "Model meshlet PSO");
    }

    return m_pMeshletPSO;
}

IRHIPipelineState* MeshMaterial::GetShadowPSO()
{
    if (m_pShadowPSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        eastl::vector<eastl::string> defines;
        defines.push_back("UNIFORM_RESOURCE=1");

        if(m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if(m_pDiffuseTexture) defines.push_back("DIFFUSE_TEXTURE=1");
        if(m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

        RHIGraphicsPipelineDesc psoDesc;
        psoDesc.m_pVS = pRenderer->GetShader("ModelShadow.hlsl", "vs_main", "vs_6_6", defines);
        if (m_pAlbedoTexture && m_bAlphaTest)
        {
            psoDesc.m_pPS = pRenderer->GetShader("ModelShadow.hlsl", "ps_main", "vs_6_6", defines);
        }

        psoDesc.m_rasterizerState.m_cullMode = m_bDoubleSided ? RHICullMode::None : RHICullMode::Back;
        psoDesc.m_rasterizerState.m_frontCCW = m_bFrontFaceCCW;
        psoDesc.m_rasterizerState.m_depthBias = 5.0f;
        psoDesc.m_rasterizerState.m_depthSlopeScale = 1.0f;
        psoDesc.m_depthStencilState.m_depthTest = true;
        psoDesc.m_depthStencilState.m_depthFunc = RHICompareFunc::GreaterEqual;
        psoDesc.m_depthStencilFromat = RHIFormat::D16;

        m_pShadowPSO = pRenderer->GetPipelineState(psoDesc, "Model shadoe PSO");       
    }

    return m_pShadowPSO;
}

IRHIPipelineState* MeshMaterial::GetVelocityPSO()
{
    if (m_pVelocityPSO = nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        eastl::vector<eastl::string> defines;
        defines.push_back("UNIFORM_RESOURCE=1");

        if(m_bSkeletalAnim) defines.push_back("ANIME_POS=1");
        if(m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if(m_pDiffuseTexture) defines.push_back("DIFFUSE_TEXTURE=1");
        if(m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

        RHIGraphicsPipelineDesc psoDesc;
        psoDesc.m_pVS = pRenderer->GetShader("ModelVelocity.hlsl", "vs_main", "vs_6_6", defines);
        psoDesc.m_pPS = pRenderer->GetShader("ModelVelocity.hlsl", "ps_main", "ps_6_6", defines);        
        psoDesc.m_rasterizerState.m_cullMode = m_bDoubleSided ? RHICullMode::None : RHICullMode::Back;
        psoDesc.m_rasterizerState.m_frontCCW = m_bFrontFaceCCW;
        psoDesc.m_depthStencilState.m_depthWrite = false;
        psoDesc.m_depthStencilState.m_depthTest = true;
        psoDesc.m_depthStencilState.m_depthFunc = RHICompareFunc::GreaterEqual;
        psoDesc.m_rtFormat[0] = RHIFormat::RGBA16F;
        psoDesc.m_depthStencilFromat = RHIFormat::D32F;

        m_pVelocityPSO = pRenderer->GetPipelineState(psoDesc, "Model velocity PSO");
    }

    return m_pVelocityPSO;
}

IRHIPipelineState* MeshMaterial::GetIDPSO()
{
    if (m_pIDPSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        eastl::vector<eastl::string> defines;
        defines.push_back("UNIFORM_RESOURCE=1");

        if(m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if(m_pDiffuseTexture) defines.push_back("DIFFUSE_TEXTURE=1");
        if(m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

        RHIGraphicsPipelineDesc psoDesc;
        psoDesc.m_pVS = pRenderer->GetShader("MidelID.hlsl", "vs_main", "vs_6_6", defines);
        psoDesc.m_pPS = pRenderer->GetShader("MidelID.hlsl", "ps_main", "ps_6_6", defines);
        psoDesc.m_rasterizerState.m_cullMode = m_bDoubleSided ? RHICullMode::None : RHICullMode::Back;
        psoDesc.m_rasterizerState.m_frontCCW = m_bFrontFaceCCW;
        psoDesc.m_depthStencilState.m_depthWrite = false;
        psoDesc.m_depthStencilState.m_depthTest = true;
        psoDesc.m_depthStencilState.m_depthFunc = RHICompareFunc::GreaterEqual;     //Use reversed depth
        psoDesc.m_rtFormat[0] = RHIFormat::R32UI;
        psoDesc.m_depthStencilFromat = RHIFormat::D32F;

        m_pIDPSO = pRenderer->GetPipelineState(psoDesc, "Model ID PSO");
    }

    return m_pIDPSO;
}

IRHIPipelineState* MeshMaterial::GetOutlinePSO()
{
    if (m_pOutlinePSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    
        eastl::vector<eastl::string> defines;
        AddMaterialDefines(defines);
        defines.push_back("UNIFORM_RESOURCE=1");

         if(m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if(m_pDiffuseTexture) defines.push_back("DIFFUSE_TEXTURE=1");
        if(m_bAlphaTest) defines.push_back("ALPHA_TEST=1");

        RHIGraphicsPipelineDesc psoDesc;
        psoDesc.m_pVS = pRenderer->GetShader("ModelOutline.hlsl", "vs_main", "vs_6_6", defines);
        psoDesc.m_pPS = pRenderer->GetShader("ModelOutline.hlsl", "ps_main", "ps_6_6", defines);
        psoDesc.m_rasterizerState.m_cullMode = RHICullMode::Front;
        psoDesc.m_rasterizerState.m_frontCCW = m_bFrontFaceCCW;
        psoDesc.m_depthStencilState.m_depthWrite = false;
        psoDesc.m_depthStencilState.m_depthTest = true;
        psoDesc.m_depthStencilState.m_depthFunc = RHICompareFunc::GreaterEqual;     //Use reversed depth
        psoDesc.m_rtFormat[0] = RHIFormat::RGBA16F;
        psoDesc.m_depthStencilFromat = RHIFormat::D32F;

        m_pOutlinePSO = pRenderer->GetPipelineState(psoDesc, "Model Outline PSO");
    }

    return m_pOutlinePSO;
}

IRHIPipelineState* MeshMaterial::GetVertexSkinningPSO()
{
    if (m_pVertexSkinningPSO == nullptr)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

        RHIComputePipelineDesc desc;
        desc.m_pCS = pRenderer->GetShader("VertexSkining.hlsl", "main", "cs_6_6", {});
        m_pVertexSkinningPSO = pRenderer->GetPipelineState(desc, "Vertex skinning PSO");
    }
    return m_pVertexSkinningPSO;
}

void MeshMaterial::UpdateConstants()
{
    m_materialCB.m_shadingModel = (uint) m_shadingModel;
    m_materialCB.m_albedo = m_albedoColor;
    m_materialCB.m_emissive = m_emissiveColor;
    m_materialCB.m_metallic = m_metallic;
    m_materialCB.m_roughness = m_roughness;
    m_materialCB.m_alphCutoff = m_alphaCutoff;
    m_materialCB.m_diffuse = m_diffuseColor;
    m_materialCB.m_specular = m_specularColor;
    m_materialCB.m_glossiness = m_glossiness;
    m_materialCB.m_anisotropy = m_anisotropy;
    m_materialCB.m_sheenColor = m_sheenColor;
    m_materialCB.m_sheenRoughness = m_sheenRoughness;
    m_materialCB.m_clearCoat = m_clearCoat;
    m_materialCB.m_clearCoatRoughness = m_clearCoatRoughness;

    m_materialCB.m_bPBRMatallicRoughness = m_bPBRMetallicRoughness;
    m_materialCB.m_bPBRSpecularGlossiness = m_bPBRSpecularGlossiness;
    m_materialCB.m_bRGNormalTexture = m_pNormalTexture && (m_pNormalTexture->GetTexture()->GetDesc().m_format == RHIFormat::BC5UNORM);
    m_materialCB.m_bRGClearCoatNormalTexture = m_pClearCoatNormalTexture && (m_pClearCoatNormalTexture->GetTexture()->GetDesc().m_format == RHIFormat::BC5UNORM);
    m_materialCB.m_bDoubleSided = m_bDoubleSided;
}

void MeshMaterial::OnGUI()
{
    
    GUI("Inspector", "Material", [&]()
        {
            bool resetPSO = ImGui::Combo("Shading Model##Material", (int*)&m_shadingModel, "Default\0Anisotropy\0Sheen\0ClearCoat\0Hair\0\0", (int)ShadingModel::Max);

            // todo: material textures
            
            if (m_bPBRMetallicRoughness)
            {
                ImGui::ColorEdit3("Albedo##Material", (float*)&m_albedoColor);
                ImGui::SliderFloat("Metallic##Material", &m_metallic, 0.0f, 1.0f);
                ImGui::SliderFloat("Roughness##Material", &m_roughness, 0.0f, 1.0f);
            }
            else if (m_bPBRSpecularGlossiness)
            {
                ImGui::ColorEdit3("Diffuse##Material", (float*)&m_diffuseColor);
                ImGui::ColorEdit3("Specular(F0)##Material", (float*)&m_specularColor);
                ImGui::SliderFloat("Glossness##Material", &m_glossiness, 0.0f, 1.0f);
            }

            if (m_bAlphaTest)
            {
                ImGui::SliderFloat("Alpha Cutoff##Material", &m_alphaCutoff, 0.0f, 1.0f);
            }

            ImGui::ColorEdit3("Emissive##Material", (float*)&m_emissiveColor);

            switch (m_shadingModel)
            {
                case ShadingModel::Anisotropy:
                    ImGui::SliderFloat("Anisotropy##Material", &m_anisotropy, -1.0f, 1.0f);
                    break;
                case ShadingModel::Sheen:
                    ImGui::ColorEdit3("Sheen Color#Material", (float*)&m_sheenColor);
                    ImGui::SliderFloat("Sheen Roughness#Material", &m_sheenRoughness, 0.0f, 1.0f);
                    break;
                case ShadingModel::ClearCoat:
                    ImGui::SliderFloat("ClearCoat##Material", &m_clearCoat, 0.0f, 1.0f);
                    ImGui::SliderFloat("ClearCoat Roughness##Material", &m_clearCoatRoughness, 0.0f, 1.0f);
                    break;
                default:
                    break;

            }

            if (resetPSO)
            {
                m_pPSO = nullptr;
                m_pMeshletPSO = nullptr;
            }
        });
}

void MeshMaterial::AddMaterialDefines(eastl::vector<eastl::string>& defines)
{
    switch (m_shadingModel)
    {
        case ShadingModel::Anisotropy:
            defines.push_back("SHADING_MODEL_ANISOTROPY=1");
            break;
        case ShadingModel::Sheen:
            defines.push_back("SHADING_MODEL_SHEEN=1");
            break;
        case ShadingModel::ClearCoat:
            defines.push_back("SHADING_MODEL_CLEAR_COAT=1");
            break;
        case ShadingModel::Hair:
            defines.push_back("SHADING_MODEL_HAIR=1");
            break;
        default:
            defines.push_back("SHADING_MODEL_DEFAULT=1");
            break;
    }

    if(m_bPBRMetallicRoughness) defines.push_back("PBR_METALLIC_ROUGHNESS=1");
    if(m_pAlbedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
    if (m_pMetallicRoughnessTexture)
    {
        defines.push_back("METALLIC_ROUGHNESS_TEXTURE=1");
        if (m_pAOTexture == m_pMetallicRoughnessTexture)
        {
            defines.push_back("AO_METALLIC_ROUGHNESS_TEXTURE=1");
        }
    }

    if(m_bPBRSpecularGlossiness) defines.push_back("PBR_SPECULAR_GLOSSINESS=1");
    if(m_pDiffuseTexture) defines.push_back("DIFFUSE_TEXTURE=1");
    if(m_pSpecularGlossinessTexture) defines.push_back("SPECULAR_GLOSSINESS_TEXTURE=1");

    if (m_pNormalTexture)
    {
        defines.push_back("NORMAL_TEXTURE=1");

        if (m_pNormalTexture->GetTexture()->GetDesc().m_format == RHIFormat::BC5UNORM)
        {
            defines.push_back("RG_NORMAL_TEXTURE=1");
        }
    }

    if (m_pAnisotropicTagentTexture)
    {
        defines.push_back("ANISOTROPIC_TANGENT_TEXTURE=1");
    }

    if(m_bAlphaTest) defines.push_back("ALPHA_TEST=1");
    if(m_pEmissiveTexture) defines.push_back("EMISSIVE_TEXTURE=1");
    if(m_pAOTexture) defines.push_back("AO_TEXTURE=1");
    if(m_bDoubleSided) defines.push_back("DOUBLE_SIDED=1");
    
    if (m_pSheenColorTexture)
    {
        defines.push_back("SHEEN_COLOR_TEXTURE=1");
        if (m_pSheenRoughnessTexture == m_pSheenColorTexture)
        {
            defines.push_back("SHEEN_COLOR_ROUGHNESS_TEXTURE=1");
        }
    }
    if(m_pSheenRoughnessTexture) defines.push_back("SHEEN_ROUGHNESS_TEXTURE=1");

    if (m_pClearCoatTexture)
    {
        defines.push_back("CLEAR_COAT_TEXTURE=1");
        if (m_pClearCoatRoughnessTexture == m_pClearCoatTexture)
        {
            defines.push_back("CLEAR_COAT_ROUGHNESS_COMBINED_TEXTURE=1");
        }
    }
    if(m_pClearCoatRoughnessTexture) defines.push_back("CLEAR_COAT_ROUGHNESS_TEXTURE=1");
    if (m_pClearCoatNormalTexture)
    {
        defines.push_back("CLEAR_COAT_NORMAL_TEXTURE=1");
        if (m_pClearCoatNormalTexture->GetTexture()->GetDesc().m_format == RHIFormat::BC5UNORM)
        {
            defines.push_back("RG_CLEAR_COAT_NORMAL_TEXTURE=1");
        }
    }
}