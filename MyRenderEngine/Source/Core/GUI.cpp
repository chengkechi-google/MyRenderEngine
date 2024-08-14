#include "GUI.h"
#include "Engine.h"
#include "imgui/imgui_impl_win32.h" //< Platform dependence header
#include "ImGuizmo/ImGuizmo.h"

GUI::GUI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.BackendRendererName = "imgui_impl_MyRenderEngine";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // This enables output of large meshes (64K+ vertices) while still using 16-bits indices.

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(Engine::GetInstance()->GetWindowHandle());
}

GUI::~GUI()
{
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

bool GUI::Init()
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IRHIDevice* pDevice = pRenderer->GetDevice();

    float scaling = ImGui_ImplWin32_GetDpiScaleForHwnd(Engine::GetInstance()->GetWindowHandle());
    ImGui::GetStyle().ScaleAllSizes(scaling);

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = scaling;

    ImFontConfig fontConfig;
    fontConfig.OversampleH = fontConfig.OversampleV = 3;

    eastl::string fontFile = Engine::GetInstance()->GetAssetPath() + "fonts/DroidSans.ttf";
    io.Fonts->AddFontFromFileTTF(fontFile.c_str(), 13.0f, &fontConfig);

    unsigned char* pPixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pPixels, &width, &height);

    m_pFontTexture.reset(pRenderer->CreateTexture2D(width, height, 1, RHIFormat::RGBA8UNORM, 0, "GUI::m_pFontTexture"));
    pRenderer->UploadTexture(m_pFontTexture->GetTexture(), pPixels);

    io.Fonts->TexID = (ImTextureID) m_pFontTexture->GetSRV();

    RHIGraphicsPipelineDesc psoDesc;
    psoDesc.m_pVS = pRenderer->GetShader("Imgui.hlsl", "vs_main", RHIShaderType::VS);
    psoDesc.m_pPS = pRenderer->GetShader("Imgui.hlsl", "ps_main", RHIShaderType::PS);
    psoDesc.m_depthStencilState.m_depthWrite = false;
    psoDesc.m_blendState[0].m_blendEnable = true;
    psoDesc.m_blendState[0].m_colorSrc = RHIBlendFactor::SrcAlpha;
    psoDesc.m_blendState[0].m_colorDst = RHIBlendFactor::InvSrcAlpha;
    psoDesc.m_blendState[0].m_alphaSrc = RHIBlendFactor::One;
    psoDesc.m_blendState[0].m_alphaDst = RHIBlendFactor::InvSrcAlpha;
    psoDesc.m_rtFormat[0] = pRenderer->GetSwapChain()->GetDesc().m_format;
    psoDesc.m_depthStencilFromat = RHIFormat::D32F;
    m_pPSO = pRenderer->GetPipelineState(psoDesc, "GUI PSO");
    return true;
}

void GUI::Tick()
{
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuizmo::BeginFrame();
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
}

void GUI::Render(IRHICommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "GUI");

    for (size_t i = 0; i < m_commands.size(); ++i)
    {
        m_commands[i]();
    }
    m_commands.clear();

    ImGui::Render();

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IRHIDevice* pDevice = pRenderer->GetDevice();
    ImDrawData* pDrawData = ImGui::GetDrawData();

    // Avoid rendering when minimized
    if (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0)
    {
        return;
    }

    uint32_t frameIndex = pDevice->GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;
    if(m_pVertexBuffer[frameIndex] == nullptr || m_pVertexBuffer[frameIndex]->GetBuffer()->GetDesc().m_size < pDrawData->TotalVtxCount * sizeof(ImDrawVert));
    {
        m_pVertexBuffer[frameIndex].reset(pRenderer->CreateStructedBuffer(nullptr, sizeof(ImDrawVert), pDrawData->TotalVtxCount + 5000, "GUI VB", RHIMemoryType::CPUToGPU));
    }

    if (m_pIndexBuffer[frameIndex] == nullptr || m_pIndexBuffer[frameIndex]->GetIndexCount() < (uint32_t)pDrawData->TotalIdxCount)
    {
        m_pIndexBuffer[frameIndex].reset(pRenderer->CreateIndexBuffer(nullptr, sizeof(ImDrawIdx), pDrawData->TotalIdxCount + 10000, "GUI IB", RHIMemoryType::CPUToGPU));
    }

    ImDrawVert* pVertexDest = (ImDrawVert*) m_pVertexBuffer[frameIndex]->GetBuffer()->GetCPUAddress();
    ImDrawIdx* pIndexDest = (ImDrawIdx*) m_pIndexBuffer[frameIndex]->GetBuffer()->GetCPUAddress();
    for (int i = 0; i < pDrawData->CmdListsCount; ++i)
    {
        const ImDrawList* pImDrawList = pDrawData->CmdLists[i];
        memcpy(pVertexDest, pImDrawList->VtxBuffer.Data, pImDrawList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(pIndexDest, pImDrawList->IdxBuffer.Data, pImDrawList->IdxBuffer.Size * sizeof(ImDrawIdx));
        pVertexDest += pImDrawList->VtxBuffer.Size;
        pIndexDest += pImDrawList->IdxBuffer.Size;
    }

    SetupRenderStates(pCommandList, frameIndex);

    int globalVertexOffset = 0;
    int globalIndexOffset = 0;

    ImVec2 clipOff = pDrawData->DisplayPos;
    ImVec2 clipScale = pDrawData->FramebufferScale;

    for (int i = 0; i < pDrawData->CmdListsCount; ++i)
    {
        const ImDrawList* pImDrawList = pDrawData->CmdLists[i];
        for (int j = 0; j < pImDrawList->CmdBuffer.Size; ++j)
        {
            const ImDrawCmd* pCmd = &pImDrawList->CmdBuffer[j];
            if (pCmd->UserCallback != NULL)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pCmd->UserCallback == ImDrawCallback_ResetRenderState)
                {
                    SetupRenderStates(pCommandList, frameIndex);
                }
                else
                {
                    pCmd->UserCallback(pImDrawList, pCmd);
                }
            }
            else
            {
                ImVec2 clipMin((pCmd->ClipRect.x - clipOff.x) * clipScale.x, (pCmd->ClipRect.y - clipOff.y) * clipScale.y);
                ImVec2 clipMax((pCmd->ClipRect.z - clipOff.x) * clipScale.x, (pCmd->ClipRect.w - clipOff.y) * clipScale.y);
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                {
                    continue;
                }

                pCommandList->SetScissorRect((uint32_t)clipMin.x, (uint32_t)clipMin.y, (uint32_t)clipMax.x - clipMin.x, (uint32_t)clipMax.y - clipMin.y);
                uint32_t resourceIDs[4] = {
                    m_pVertexBuffer[frameIndex]->GetSRV()->GetHeapIndex(),
                    pCmd->VtxOffset + globalVertexOffset,
                    ((IRHIDescriptor*) pCmd->TextureId)->GetHeapIndex(),
                    pRenderer->GetLinearSampler()->GetHeapIndex() };

                pCommandList->SetGraphicsConstants(0, resourceIDs, sizeof(resourceIDs));
                pCommandList->DrawIndexed(pCmd->ElemCount, 1, pCmd->IdxOffset + globalIndexOffset);
            }
        }
        globalIndexOffset += pImDrawList->IdxBuffer.Size;
        globalVertexOffset += pImDrawList->VtxBuffer.Size;
    }
}

void GUI::SetupRenderStates(IRHICommandList* pCommandList, uint32_t frameIndex)
{
    ImDrawData* pDrawData = ImGui::GetDrawData();

    pCommandList->SetViewport(0, 0, (uint32_t)(pDrawData->DisplaySize.x * pDrawData->FramebufferScale.x), (uint32_t)(pDrawData->DisplaySize.y * pDrawData->FramebufferScale.y));
    pCommandList->SetPipelineState(m_pPSO);
    pCommandList->SetIndexBuffer(m_pIndexBuffer[frameIndex]->GetBuffer(), 0, m_pIndexBuffer[frameIndex]->GetFormat());

    float L = pDrawData->DisplayPos.x;
    float R = pDrawData->DisplayPos.x + pDrawData->DisplaySize.x;
    float T = pDrawData->DisplayPos.y;
    float B = pDrawData->DisplayPos.y + pDrawData->DisplaySize.y;

    float mvp[4][4] = 
    {
        { 2.0f/(R-L),   0.0f,           0.0f,       0.0f},
        { 0.0f,         2.0f/(T-B),     0.0f,       0.0f},
        { 0.0f,         0.0f,           0.5f,       0.0f},
        { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f}
    };

    pCommandList->SetGraphicsConstants(1, mvp, sizeof(mvp));
}