#pragma once
#include "Renderer/Renderer.h"
#include "EASTL/hash_map.h"
#include "EASTL/functional.h"

class Editor
{
public:
    Editor(Renderer* pRenderer);
    ~Editor();

    void NewFrame();
    void Tick();
    void Render(IRHICommandList* pCommandList);

private:
    void BuildDockLayout();
    void DrawMenu();
    void DrawToolBar();
    void DrawGizmo();
    void DrawFrameStats();
    
    void CreateGPUMemoryStas();
    void ShowRenderGraoh();
    void FlushPendingTextureDeletions();

private:
    Renderer* m_pRenderer = nullptr;
    eastl::unique_ptr<class ImGuiImpl> m_pImGuiImpl;
    
    bool m_showGPUMemoryStats = false;
    bool m_showImguiDemo = false;
    bool m_showGPUDrivenStats = false;
    bool m_viewFrustumLocked = false;
    bool m_vsync = false;
    bool m_showMeshLets = false;

    bool m_showInspector = false;
    bool m_showSetting = false;
    bool m_showLighting = false;
    bool m_showPostProcess = false;
    bool m_resetLayout = false;

    unsigned int m_dockSpace = 0;

    struct Command
    {
        eastl::string m_section;
        eastl::function<void()> m_func;
    };

    eastl::hash_map<IRHIDescriptor*, Texture2D*> m_fileDialogIcons;
    eastl::vector<IRHIDescriptor*> m_pendingDeletions;

    eastl::unique_ptr<Texture2D> m_pGPUMemoryStats;

    enum class SelectEditMode
    {
        Translate,
        Rotate,
        Scale
    };

    SelectEditMode m_selectEditMode = SelectEditMode::Translate;
    eastl::unique_ptr<Texture2D> m_pTranslateIcon;
    eastl::unique_ptr<Texture2D> m_pRotateIcon;
    eastl::unique_ptr<Texture2D> m_pScaleIcon;

    float3 m_lockedViewPos;
    float3 m_lockedViewRotation;
};