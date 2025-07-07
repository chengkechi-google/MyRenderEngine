#pragma once
#include "Renderer/Renderer.h"
#include "Renderer/RenderGraph/RenderGraph.h"

struct ClusteredLightData
{
    int3 m_clusterCount;
    RGHandle m_lightGrid;
    uint32_t m_clusterSize;

    float2 m_lightGridParams;
    float4x4 m_debugClustersViewMatrix;
    bool m_dirtyDebugData = true;
};

class ClusteredLightCulling
{
public:
    ClusteredLightCulling(Renderer* pRenderer);

    void AddPass(RenderGraph* pRenderGraph, ClusteredLightData& clusteredLightData, uint32_t lightCount);

private:
    void FrustumCulling(IRHICommandList* pCommandList, uint32_t lightCount,
        RGBuffer* pFrustumLightCullingCountBuffer, RGBuffer* pFrustumLightCullingIDBuffer);
    
private:
    Renderer* m_pRenderer;

    // PSOs
    IRHIPipelineState* m_pFrustumLightCullingPSO;
    IRHIPipelineState* m_pClusteredLightCullingPSO;
    IRHIPipelineState* m_pFrustumLightCullingVisualizePSO;
    IRHIPipelineState* m_pClusteredLightCullingVisualizePSO;
    IRHIPipelineState* m_pClusteredLightCullingVisualizeTopDownPSO;
};
