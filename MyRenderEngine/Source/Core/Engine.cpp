#include "Engine.h"
#include "Utils/log.h"
#include "utils/profiler.h"
#include "Utils/system.h"
#include "Utils/memory.h"
#include "rpmalloc/rpmalloc.h"
#include "enkiTS/TaskScheduler.h"
#include "spdlog/sinks/msvc_sink.h"
#include "spdlog/sinks/basic_file_sink.h"

#define SOKOL_IMPL
#include "sokol/sokol_time.h"


Engine* Engine::GetInstance()
{
    static Engine engine;
    return &engine;
}

Engine::~Engine()
{

}

void Engine::Init(const eastl::string& workPath, void* windowHandle, uint32_t windowWidth, uint32_t windowHeight)
{
    auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((workPath + "log.txt").c_str(), true);
    auto logger = std::make_shared<spdlog::logger>("MyRenderEngine", spdlog::sinks_init_list{ msvcSink, fileSink });
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [thread %t] %v"); //< Year-Month-Date Hour-Minute-Second Line thread id https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
    spdlog::set_level(spdlog::level::trace); //< trace detail, maybe turn off in release mode
    spdlog::flush_every(std::chrono::milliseconds(10)); 

    MicroProfileSetEnableAllGroups(true);
    MicroProfileSetForceMetaCounters(true);
    MicroProfileOnThreadCreate("Main Thread");

    enki::TaskSchedulerConfig config;
    config.profilerCallbacks.threadStart = [](uint32_t i)
    {
        rpmalloc_thread_initialize();
        eastl::string threadName = fmt::format("Worker Thread {}", i).c_str();
        MicroProfileOnThreadCreate(threadName.c_str());
        SetCurrentThreadName(threadName);
    };

    config.profilerCallbacks.threadStop = [](uint32_t i)
    {
        MicroProfileOnThreadExit();
        rpmalloc_thread_finalize(1);
    };

    config.customAllocator.alloc = [](size_t alignment, size_t size, void* userData, const char* file, int line)
    {
        return MY_ALLOC(size, alignment);
    };

    config.customAllocator.free = [](void* ptr, size_t size, void* userData, const char* file, int line)
    {
        MY_FREE(ptr);
    };

    m_pTaskScheduler.reset(new enki::TaskScheduler());
    m_pTaskScheduler->Initialize(config);
    
    m_windowHandle = windowHandle;
    m_workPath = workPath;
    LoadEngineConfig();

    // Initialized renderer
    m_pRenderer = eastl::make_unique<Renderer>();
    m_pRenderer->CreateDevice(windowHandle, windowWidth, windowHeight);
    m_pRenderer->SetAsyncComputeEnabled(m_configIni.GetBoolValue("Render", "AsyncCompute"));

    stm_setup();

    
}

void Engine::Tick()
{
    CPU_EVENT("Tick", "Engine::Tick");
    m_frameTime = (float)stm_sec(stm_laptime(&m_lastFrameTime));

    m_pRenderer->RenderFrame();
    MicroProfileFlip(0);
}

void Engine::Shutdown()
{
    m_pTaskScheduler->WaitforAll();
    m_pTaskScheduler.reset();

    MicroProfileShutdown();
    m_pRenderer.reset();
    spdlog::shutdown();
}

void Engine::LoadEngineConfig()
{
    eastl::string iniFile = m_workPath + "EngineConfig.ini";
    
    SI_Error error = m_configIni.LoadFile(iniFile.c_str());
    if (error != SI_OK)
    {
        MY_ERROR("Failed to load EngineConfig.ini");
        return;
    }

    m_assetPath = m_workPath + m_configIni.GetValue("MyRenderEngine", "AssetPath");
    m_shaderPath = m_workPath + m_configIni.GetValue("MyRenderEngine", "ShaderPath");
}