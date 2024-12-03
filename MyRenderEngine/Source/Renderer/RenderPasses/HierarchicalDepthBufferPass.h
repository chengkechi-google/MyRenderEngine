#pragma once
#include "../RenderGraph/RenderGraph.h"
#include "../Resource/TypedBuffer.h"

class Renderer;

class HZBPass
{
public:
    HZBPass(Renderer* pRenderer);

    void Generate1stPhaseCullingHZB(RenderGraph* pGraph);
    void Generate2ndPhaseCullingHZB(RenderGraph* pGraph, RGHandle depthRT);
    void GenerateSceneHZB(RenderGraph* pGraph, RGHandle depthRT);

    uint32_t GetHZBMipCount() const { return m_hzbMipCount; }
    uint32_t GetHZBWidth() const { return m_hzbSize.x; }
    uint32_t GetHZBHeight() const { return m_hzbSize.y; }

    RGHandle Get1stPhaseCullingHZBMip(uint32_t mip) const;
    RGHandle Get2ndPhaseCullingHZBMip(uint32_t mip) const;
    RGHandle GetSceneHZBMip(uint32_t mip) const;


private:
    void CalcHZBSize();
    
    void ReprojectDepth(IRHICommandList* pCommandList, RGTexture* pReprojectedDepthTexture);
    void DilateDepth(IRHICommandList* pCommandList, RGTexture* pReprojectedDepthSRV, RGTexture* pHZBMip0UAV);
    void BuildHZB(IRHICommandList* pCommandList, RGTexture* pTexture, bool bMinMax = false);
    void InitHZB(IRHICommandList* pCommandList, RGTexture* pInputDepthSRV, RGTexture* pHZBMip0UAV, bool bMinMax = false);
private:
    Renderer* m_pRenderer = nullptr;
    
    IRHIPipelineState* m_pDepthReprojectionPSO = nullptr;
    IRHIPipelineState* m_pDepthDilationPSO = nullptr;
    IRHIPipelineState* m_pDepthMipFilterPSO = nullptr;
    IRHIPipelineState* m_pInitHZBPSO = nullptr;

    IRHIPipelineState* m_pInitSceneHZBPSO = nullptr;
    IRHIPipelineState* m_pDepthMipFilterMinMaxPSO = nullptr;

    uint32_t m_hzbMipCount = 0;
    uint2 m_hzbSize;

    static const uint32_t MAX_HZB_MIP_COUNT = 13;
    RGHandle m_1stPhaseCullingHZBMips[MAX_HZB_MIP_COUNT] = {};
    RGHandle m_2ndPhaseCullingHZBMips[MAX_HZB_MIP_COUNT] = {};
    RGHandle m_sceneHZBMips[MAX_HZB_MIP_COUNT] = {};
};