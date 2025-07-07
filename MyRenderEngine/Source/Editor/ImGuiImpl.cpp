#include "ImGuiImpl.h"
#include "Core/Engine.h"
#include "Utils/profiler.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "ImGuizmo/ImGuizmo.h"

ImGuiImpl::ImGuiImpl(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.BackendRendererName = "ImGui_Impl_MyRenderer";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;      //< We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    //io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(Engine::GetInstance()->GetWindowHandle());
}

ImGuiImpl::~ImGuiImpl()
{
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

bool ImGuiImpl::Init()
{
    IRHIDevice* pDevice = m_pRenderer->GetDevice();

    float scaling = ImGui_ImplWin32_GetDpiScaleForHwnd(Engine::GetInstance()->GetWindowHandle());
    ImGui::GetStyle().ScaleAllSizes(scaling);

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = scaling;

    ImFontConfig fontConfig;
    fontConfig.OversampleH = fontConfig.OversampleV = 3;

    eastl::string fontFile = Engine::GetInstance()->GetAssetPath() + "fonts/DroidSans.ttf";
    io.Fonts->AddFontFromFileTTF(fontFile.c_str(), 13.0f, &fontConfig);

    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    m_pFontTexture.reset(m_pRenderer->CreateTexture2D(width, height, 1, RHIFormat::RGBA8UNORM, 0, "ImGuiImpl::m_pFontTexture"));
    m_pRenderer->UploadTexture(m_pFontTexture->GetTexture(), pixels);

    io.Fonts->TexID = (ImTextureID) (m_pFontTexture->GetSRV());

    RHIGraphicsPipelineDesc psoDesc;
    psoDesc.m_pVS = m_pRenderer->GetShader("imgui.hlsl", "vs_main", RHIShaderType::VS);
    psoDesc.m_pPS = m_pRenderer->GetShader("imgui.hlsl", "ps_main", RHIShaderType::PS);
    psoDesc.m_depthStencilState.m_depthWrite = false;
    psoDesc.m_blendState[0].m_blendEnable = true;
    psoDesc.m_blendState[0].m_colorSrc = RHIBlendFactor::SrcAlpha;
    psoDesc.m_blendState[0].m_colorDst = RHIBlendFactor::InvSrcAlpha;
    psoDesc.m_blendState[0].m_alphaSrc = RHIBlendFactor::One;
    psoDesc.m_blendState[0].m_alphaDst = RHIBlendFactor::InvSrcAlpha;
    psoDesc.m_rtFormat[0] = m_pRenderer->GetSwapChain()->GetDesc().m_format;
    psoDesc.m_depthStencilFromat = RHIFormat::D32F;
    m_pPSO = m_pRenderer->GetPipelineState(psoDesc, "ImGuiImpl PSO");

    return true;
}

void ImGuiImpl::NewFrame()
{
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
}

void ImGuiImpl::Render(IRHICommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "ImGui");

    // Prepare the data for rendering so you can call GetDrawData()
    ImGui::Render();

    IRHIDevice* pDevice = m_pRenderer->GetDevice();
    ImDrawData* pDrawData = ImGui::GetDrawData();

    // Avoid rendering when minimized
    if (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f)
    {
        return;
    }

    uint32_t frameIndex = pDevice->GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;

    if (m_pVertexBuffer[frameIndex] == nullptr || m_pVertexBuffer[frameIndex]->GetBuffer()->GetDesc().m_size < pDrawData->TotalVtxCount * sizeof(ImDrawVert))
    {
        // 2048 is use for resvered space, so we don't have to recreate vertex buffer when UI changed
        m_pVertexBuffer[frameIndex].reset(m_pRenderer->CreateStructedBuffer(nullptr, sizeof(ImDrawVert), pDrawData->TotalVtxCount, "ImGuiImpl VB", RHIMemoryType::CPUToGPU));
    }

    if (m_pIndexBuffer[frameIndex] == nullptr || m_pIndexBuffer[frameIndex]->GetIndexCount() < pDrawData->TotalIdxCount)
    {
        // 2048 is use for resvered space, so we don't have to recreate vertex buffer when UI changed
        m_pIndexBuffer[frameIndex].reset(m_pRenderer->CreateIndexBuffer(nullptr, sizeof(ImDrawIdx), pDrawData->TotalIdxCount, "ImGuiImpl IB", RHIMemoryType::CPUToGPU));
    }

    ImDrawVert *pVtxDst = (ImDrawVert*) m_pVertexBuffer[frameIndex]->GetBuffer()->GetCPUAddress();
    ImDrawIdx *pIdxDst = (ImDrawIdx*) m_pIndexBuffer[frameIndex]->GetBuffer()->GetCPUAddress();
    for (int n = 0; n < pDrawData->CmdListsCount; ++ n)
    {
        const ImDrawList* pCmdList = pDrawData->CmdLists[n];
        memcpy(pVtxDst, pCmdList->VtxBuffer.Data, pCmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(pIdxDst, pCmdList->IdxBuffer.Data, pCmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

        pVtxDst += pCmdList->VtxBuffer.Size;
        pIdxDst += pCmdList->IdxBuffer.Size;
    }

    SetupRenderStates(pCommandList, frameIndex);

    int globalVtxOffset = 0;
    int globalIdxOffset = 0;

    ImVec2 clipOff = pDrawData->DisplayPos;
    ImVec2 clipScale = pDrawData->FramebufferScale;
    uint32_t viewportWidth = pDrawData->DisplaySize.x * clipScale.x;
    uint32_t viewportHeight = pDrawData->DisplaySize.y * clipScale.y;

    for (int n = 0; n < pDrawData->CmdListsCount; ++n)
    {
        const ImDrawList* pCmdList = pDrawData->CmdLists[n];
        for (int cmdIndex = 0; cmdIndex < pCmdList->CmdBuffer.Size; ++cmdIndex)
        {
            const ImDrawCmd* pCmd = &pCmdList->CmdBuffer[cmdIndex];

            if (pCmd->UserCallback != NULL)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pCmd->UserCallback == ImDrawCallback_ResetRenderState)
                {
                    SetupRenderStates(pCommandList, frameIndex);
                }else
                {
                    pCmd->UserCallback(pCmdList, pCmd);
                }
            }else
            {
                ImVec2 clipMin((pCmd->ClipRect.x - clipOff.x) * clipScale.x, (pCmd->ClipRect.y - clipOff.y) * clipScale.y);
                ImVec2 clipMax((pCmd->ClipRect.z - clipOff.x) * clipScale.x, (pCmd->ClipRect.w - clipOff.y) * clipScale.y);
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                {
                    continue;
                }

                pCommandList->SetScissorRect(
                    eastl::max(clipMin.x, 0.0f),
                    eastl::max(clipMin.y, 0.0f),
                    eastl::clamp((uint32_t)(clipMax.x - clipMin.x), 0u, viewportWidth),
                    eastl::clamp((uint32_t)(clipMax.y - clipMin.y), 0u, viewportHeight));

                uint32_t resourceIDs[4] = {
                    m_pVertexBuffer[frameIndex]->GetSRV()->GetHeapIndex(),
                    pCmd->VtxOffset + globalVtxOffset,
                    ((IRHIDescriptor*)pCmd->TextureId)->GetHeapIndex(),
                    m_pRenderer->GetLinearSampler()->GetHeapIndex()};

                pCommandList->SetGraphicsConstants(0, resourceIDs, sizeof(resourceIDs));
                pCommandList->DrawIndexed(pCmd->ElemCount, 1, pCmd->IdxOffset + globalIdxOffset);
            }
        }
        globalVtxOffset += pCmdList->VtxBuffer.Size;
        globalIdxOffset += pCmdList->IdxBuffer.Size;
    }
}

void ImGuiImpl::SetupRenderStates(IRHICommandList* pCommandList, uint32_t frameIndex)
{
    ImDrawData *pDrawData = ImGui::GetDrawData();

    pCommandList->SetViewport(
        0,
        0,
        (uint32_t)(pDrawData->DisplaySize.x * pDrawData->FramebufferScale.x),
        (uint32_t)(pDrawData->DisplaySize.y * pDrawData->FramebufferScale.y));
    pCommandList->SetPipelineState(m_pPSO);
    pCommandList->SetIndexBuffer(m_pIndexBuffer[frameIndex]->GetBuffer(), 0, m_pIndexBuffer[frameIndex]->GetFormat());
    
    float left = pDrawData->DisplayPos.x;
    float right = pDrawData->DisplayPos.x + pDrawData->DisplaySize.x;
    float top = pDrawData->DisplayPos.y;
    float bottom = pDrawData->DisplayPos.y + pDrawData->DisplaySize.y;
    float mvp[4][4] =
    {
        {2.0f / (right - left), 0.0f, 0.0f, 0.0f},
        {0.0f, 2.0f / (top - bottom), 0.0f, 0.0f},
        {0.0f, 0.0f, 0.5f, 0.0f},
        {(right + left) / (left - right), (top + bottom) / (bottom - top), 0.5f, 1.0f},
    };
    pCommandList->SetGraphicsConstants(1, mvp, sizeof(mvp));
}
    
