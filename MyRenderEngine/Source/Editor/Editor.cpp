#include "Editor.h"
#include "Core/Engine.h"
#include "Renderer/TextureLoader.h"
#include "Utils/assert.h"
#include "Utils/system.h"
#include "imgui/imgui.h"
#include "ImFileDialog/ImFileDialog.h"
#include "ImGuizmo/ImGuizmo.h"
#include <fstream>

Editor::Editor()
{
    ifd::FileDialog::Instance().CreateTexture = [this](uint8_t* data, int w, int h, char fmt) -> void*
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
        Texture2D* pTexture = pRenderer->CreateTexture2D(w, h, 1, fmt == 1 ? RHIFormat::RGBA8SRGB : RHIFormat::BGRA8SRGB, 0, "ImFileDialog Icon");
        pRenderer->UploadTexture(pTexture->GetTexture(), data);

        m_fileDialogIcons.insert(eastl::make_pair(pTexture->GetSRV(), pTexture));
        return pTexture->GetSRV();
    };

    ifd::FileDialog::Instance().DeleteTexture = [this](void* pSRV)
    {
        m_pendingDeletions.push_back((IRHIDescriptor*) pSRV); //< Delete in next frame

    };

    eastl::string assetPath = Engine::GetInstance()->GetAssetPath();
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    m_pTranslateIcon.reset(pRenderer->CreateTexture2D(assetPath + "ui/translate.png", true));
    m_pRotateIcon.reset(pRenderer->CreateTexture2D(assetPath + "ui/rotate.png", true));
    m_pScaleIcon.reset(pRenderer->CreateTexture2D(assetPath + "ui/scale.png", true));
}

Editor::~Editor()
{
    for (auto iter = m_fileDialogIcons.begin(); iter != m_fileDialogIcons.end(); ++iter)
    {
        delete iter->first;
        delete iter->second;
    }
}

void Editor::Tick()
{
    FlushPendingTextureDeletions();
    m_commands.clear();

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && io.MouseClicked[0])
    {
        ImVec2 mousePos = io.MouseClickedPos[0];

        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
        pRenderer->RequestMouseHitTest((uint32_t) mousePos.x, (uint32_t) mousePos.y);
    }

    DrawMenu();
    //DrawToolBar();
    //DrawGizmo();
    //DrawFrameStats();
}

void Editor::AddGUICommand(const eastl::string& window, const eastl::string& section, const eastl::function<void()>& command)
{
    m_commands[window].push_back({ section, command });
}

void Editor::DrawMenu()
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Scene"))
            {
                ifd::FileDialog::Instance().Open("ScreenOpenDialog", "Open Scene", "XML File (*.xml){.xml},.*");
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug"))
        {
            if (ImGui::MenuItem("Vsync", "", &m_vsync))
            {
                pRenderer->GetSwapChain()->SetVsyncEnabled(m_vsync);
            }

            if (ImGui::MenuItem("GPU Driven Stats", "", &m_showGPUDrivenStats))
            {
                pRenderer->SetGPUDrivenStatsEnabled(m_showGPUDrivenStats);
            }

            if (ImGui::MenuItem("Debug View Frustum", "", &m_viewFrustumLocked))
            {
               
            }

            if (ImGui::MenuItem("Show Meshlets", "", &m_showMeshLets))
            {

            }

            bool asyncCompute = pRenderer->IsAsyncComputeEnabled();
            if (ImGui::MenuItem("Async Compute", "", &asyncCompute))
            {
                pRenderer->SetAsyncComputeEnabled(asyncCompute);
            }

            if (ImGui::MenuItem("Reload Shader"))
            {
                pRenderer->ReloadShaders();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // todo
    if (ifd::FileDialog::Instance().IsDone("SceneOpenDialog"))
    {
        if (ifd::FileDialog::Instance().HasResult())
        {
            // todo: opend the scene
        }

        ifd::FileDialog::Instance().Close();
    }
}

void Editor::DrawToolBar()
{
    // todo:
}

void Editor::DrawGizmo()
{
    // todo:
}

void Editor::DrawFrameStats()
{
    // todo:
}

void Editor::CreateGPUMemoryStas()
{
    // todo:
}

void Editor::ShowRenderGraoh()
{
    // Write graph to Html format
    Engine* pEngine = Engine::GetInstance();
    Renderer* pRenderer = pEngine->GetRenderer();

    eastl::string file = pEngine->GetWorkPath() + "tools/graphviz/rendergraph.html";
    eastl::string graph = pRenderer->GetRenderGraph()->Export();

    std::ofstream stream;
    stream.open(file.c_str());
    stream << R"(<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Render Graph</title>
  <head>
  <body>
    <script src="viz-standalone.js"></script>
    <script>
       Viz.instance()
            .then(viz => {
               document.body.appendChild(viz.renderSVGElement(`
)";
    stream << graph.c_str();
    stream << R"(
                `));
            })
            .catch(error => {
                console.error(error);
            });
    </script>
  </body>
</html>
)";

    stream.close();
    eastl::string command = "start " + file;
    ExecuteCommand(command.c_str());
}

void Editor::FlushPendingTextureDeletions()
{
    for (size_t i = 0; i < m_pendingDeletions.size(); ++i)
    {
        IRHIDescriptor* pSRV = m_pendingDeletions[i];
        auto iter = m_fileDialogIcons.find(pSRV);
        MY_ASSERT(iter != m_fileDialogIcons.end());

        Texture2D* pTexture = iter->second;
        m_fileDialogIcons.erase(pSRV);
        delete pTexture;
    }

    m_pendingDeletions.clear();
}

void Editor::DrawWindow(const eastl::string& window, bool* open)
{

}
