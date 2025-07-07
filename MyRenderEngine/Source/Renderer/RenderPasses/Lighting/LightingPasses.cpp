#include "LightingPasses.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderPasses/BasePassGPUDriven.h"
#include "GTAO.h"
#include "ClusteredLighting/ClusteredLightCulling.h"

LightingPasses::LightingPasses(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    m_pClusteredLightCulling = eastl::make_unique<ClusteredLightCulling>(m_pRenderer);
    m_pGTAO = eastl::make_unique<GTAO>(m_pRenderer);
}

LightingPasses::~LightingPasses() = default;

RGHandle LightingPasses::AddPass(RenderGraph* pRenderGraph, RGHandle depthRT, RGHandle linearDepthRT, RGHandle velocityRT, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "Lighting Process");

    BasePassGPUDriven* pBasePass = m_pRenderer->GetBasePassGPUDriven();
    RGHandle diffuse = pBasePass->GetDiffuseRT();
    RGHandle specular = pBasePass->GetSoecularRT();
    RGHandle normal = pBasePass->GetNormalRT();
    RGHandle emissive = pBasePass->GetEmissiveRT();
    RGHandle customData = pBasePass->GetCustomDataRT();

    // AO pass
    RGHandle gtao = m_pGTAO->AddPasse(pRenderGraph, depthRT, normal, width, height);

    // Light culling pass
    

    

    return gtao;
}

