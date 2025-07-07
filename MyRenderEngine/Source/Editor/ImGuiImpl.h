#pragma once

#include "Renderer/Renderer.h"
#include "EASTL/functional.h"

class ImGuiImpl
{
public:
    ImGuiImpl(Renderer* pRenderer);
    ~ImGuiImpl();

    bool Init();
    void NewFrame();
    void Render(IRHICommandList* pCommandList);

private:
    void SetupRenderStates(IRHICommandList* pCommandList, uint32_t frameIndex);
    
private:
    Renderer* m_pRenderer = nullptr;
    IRHIPipelineState* m_pPSO = nullptr;

    eastl::unique_ptr<Texture2D> m_pFontTexture;
    eastl::unique_ptr<StructedBuffer> m_pVertexBuffer[RHI_MAX_INFLIGHT_FRAMES];
    eastl::unique_ptr<IndexBuffer> m_pIndexBuffer[RHI_MAX_INFLIGHT_FRAMES];
};