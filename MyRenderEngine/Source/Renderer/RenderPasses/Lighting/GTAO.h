#pragma once
#include "../../RenderGraph/RenderGraph.h"
#include "../../Resource/Texture2D.h"

class GTAO
{
public:
    GTAO(Renderer* pRenderer);
    RGHandle AddPasse(RenderGraph* pRenderGraph, RGHandle depthRT, RGHandle normalRT, uint32_t width, uint32_t height);

private:
    void CreateHilbertLUT();
    void UpdateGTAOConstants(IRHICommandList* pCommandList, uint32_t width, uint32_t height);

    void FilterDepth(IRHICommandList* pCommandList, RGTexture* pDepthTexture, RGTexture* pHZB, uint32_t width, uint32_t height);
    void RenderAO(IRHICommandList* pCommandList, RGTexture* pHZB, RGTexture* pNormal, RGTexture* pOutputAO, RGTexture* pOutputEdge, uint32_t width, uint32_t height);
    void Denoise(IRHICommandList* pCommandList, RGTexture* pInputAO, RGTexture* pEdge, RGTexture* pOutputAO, uint32_t width, uint32_t height);
    
private:
    Renderer* m_pRenderer;

    IRHIPipelineState* m_pPrefilterDepthPSO = nullptr;
    IRHIPipelineState* m_pGTAOLowPSO = nullptr;
    IRHIPipelineState* m_pGTAOMediumPSO = nullptr;
    IRHIPipelineState* m_pGTAOHighPSO = nullptr;
    IRHIPipelineState* m_pGTAOUltraPSO = nullptr;
    IRHIPipelineState* m_pDenoisePSO = nullptr;

    IRHIPipelineState* m_pGTAOSOLowPSO = nullptr;
    IRHIPipelineState* m_pGTAOSOMediumPSO = nullptr;
    IRHIPipelineState* m_pGTAOSOHighPSO = nullptr;
    IRHIPipelineState* m_pGTAOSOUltraPSO = nullptr;
    IRHIPipelineState* m_pSODenoisePSO = nullptr;

    eastl::unique_ptr<Texture2D> m_pHilbertLUT;

    bool m_bEnable = true;
    bool m_bEnableGTSO = false;
    int m_qualityLevel  = 2;
    float m_radius = 0.5f;
};