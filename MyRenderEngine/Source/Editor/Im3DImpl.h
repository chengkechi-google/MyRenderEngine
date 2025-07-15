#pragma once

#include "Renderer/Renderer.h"

class Im3DImpl
{
public:
    Im3DImpl(Renderer* pRenderer);
    ~Im3DImpl();

    bool Init();
    void NewFrame();
    void Render(IRHICommandList* pCommandList);
    
private:
    uint32_t GetVertexCount() const;
    
private:
    Renderer* m_pRenderer;
    IRHIPipelineState* m_pPointPSO = nullptr;
    IRHIPipelineState* m_pLinePSO = nullptr;
    IRHIPipelineState* m_pTrianglePSO = nullptr;

    eastl::unique_ptr<RawBuffer> m_pVertexBuffer[RHI_MAX_INFLIGHT_FRAMES];
    
};