#pragma once

#include "Editor/Editor.h"


#include "sigslot/signal.hpp"
#include "simpleini/SimpleIni.h"
#include "Renderer/Renderer.h"


namespace enki
{
    class TaskScheduler;
}

class Engine
{
public:
    static Engine* GetInstance();     //< Singleton form
    void Init(const eastl::string& workPath, void* windowHandle, uint32_t windowWidth, uint32_t windowHeight);
    void Tick();
    void Shutdown();

    Renderer* GetRenderer() const { return m_pRenderer.get(); }
    Editor* GetEditor() const { return m_pEditor.get(); }
    
    void* GetWindowHandle() const { return m_windowHandle; }
    const eastl::string& GetWorkPath() const { return m_workPath; }
    const eastl::string& GetAssetPath() const { return m_assetPath; }
    const eastl::string& GetShaderPath() const { return m_shaderPath; }

public:
    sigslot::signal<void*, uint32_t, uint32_t> WindowResizeSignal;

private:
    ~Engine();
    void LoadEngineConfig();
    
private:
    eastl::unique_ptr<Editor> m_pEditor;

    uint64_t m_lastFrameTime;
    float m_frameTime = 0.0f; //< In second

    eastl::unique_ptr<enki::TaskScheduler> m_pTaskScheduler;

    CSimpleIniA m_configIni;
    
    void* m_windowHandle = nullptr;
    eastl::string m_workPath;
    eastl::string m_assetPath;
    eastl::string m_shaderPath;

    eastl::unique_ptr<Renderer> m_pRenderer;
};