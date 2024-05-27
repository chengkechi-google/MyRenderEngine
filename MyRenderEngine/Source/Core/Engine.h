#pragma once
#include "Renderer/Renderer.h"
#include "sigslot/signal.hpp"
#include "simpleini/SimpleIni.h"


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