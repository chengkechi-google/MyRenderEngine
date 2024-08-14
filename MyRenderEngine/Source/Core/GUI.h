#pragma once

#include "Renderer/Renderer.h"
#include "EASTL/functional.h"

class GUI
{
public:
    GUI();
    ~GUI();
    
    bool Init();
    void Tick();
    void Render(IRHICommandList* pCommandList);
    void AddCommand(const eastl::function<void()>& command) { m_commands.push_back(command); }

private:
    void SetupRenderStates(IRHICommandList* pCommandList, uint32_t frameIndex);

private:
    IRHIPipelineState* m_pPSO = nullptr;
    
    eastl::unique_ptr<Texture2D> m_pFontTexture;
    eastl::unique_ptr<StructedBuffer> m_pVertexBuffer[RHI_MAX_INFLIGHT_FRAMES];
    eastl::unique_ptr<IndexBuffer> m_pIndexBuffer[RHI_MAX_INFLIGHT_FRAMES];
    eastl::vector<eastl::function<void()>> m_commands;
};