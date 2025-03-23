#pragma once

#include "../../RenderGraph/RenderGraph.h"

class LightingPasses
{
public:
    LightingPasses(Renderer* pRenderer);
    ~LightingPasses();

    RGHandle AddPass(RenderGraph* pRenderGraph, RGHandle depthRT, RGHandle linearDepthRT, RGHandle velocityRT, uint32_t width, uint32_t height);

private:
    Renderer* m_pRenderer;
    
    eastl::unique_ptr<class GTAO> m_pGTAO;
};