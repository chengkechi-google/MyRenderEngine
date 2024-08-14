#include "Core/Engine.h"
#include "imgui/imgui_impl_win32.h"
#include "rpmalloc/rpmalloc.h"
#include "resource.h"
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }

    switch (message)
    {
        case WM_SIZE:
        {
            if (wParam != SIZE_MINIMIZED)
            {
                Engine::GetInstance()->WindowResizeSignal(hWnd, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

static eastl::string GetWorkPath()
{
    wchar_t exeFilePath[MAXCHAR];
    GetModuleFileName(NULL, exeFilePath, MAXCHAR);

    DWORD size = WideCharToMultiByte(CP_ACP, 0, exeFilePath, -1, NULL, 0, NULL, false);
    
    eastl::string workPath;
    workPath.resize(size);
    WideCharToMultiByte(CP_ACP, 0, exeFilePath, -1, (LPSTR)workPath.c_str(), size, NULL, false);

    size_t lastSlash = workPath.find_last_of('\\');
    return workPath.substr(0, lastSlash + 1);   //< include '\'
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR cmdLine, _In_ int cmdShow)
{
    rpmalloc_initialize();
    ImGui_ImplWin32_EnableDpiAwareness();

    /*
    // Better icon load method 
    HICON hIcon = static_cast<HICON>(::LoadImage(hInstance,
        MAKEINTRESOURCE(IDI_MAIN),
        IMAGE_ICON,
        32, 32,    // or whatever size icon you want to load
        LR_DEFAULTCOLOR));
    */

    WNDCLASSEX windowClass = {0};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN));
    windowClass.lpszClassName = L"MyWindowClass";
    RegisterClassExW(&windowClass);

    RECT desktopRect;
    GetClientRect(GetDesktopWindow(), &desktopRect);

    const unsigned int desktopWidth = desktopRect.right - desktopRect.left;
    const unsigned int desktopHeight = desktopRect.bottom - desktopRect.top;

    const unsigned int windowWidth = desktopWidth >= 1920 ? 1920 : 1800;
    const unsigned int windowHeight = desktopHeight >= 1080 ? 1080 : 960;

    RECT windowRect = {0, 0, (LONG)windowWidth, (LONG)windowHeight};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int x = (desktopWidth - (windowRect.right - windowRect.left)) * 0.5f;
    int y = (desktopHeight - (windowRect.bottom - windowRect.top)) * 0.5f;
    
    // Create window
    HWND hWnd = CreateWindow(
        windowClass.lpszClassName,
        L"My Render Engine",
        WS_OVERLAPPEDWINDOW,
        x,
        y,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    ShowWindow(hWnd, cmdShow);
    MessageBox(hWnd, L"Wait for debuger", L"Caution", MB_OK);

    Engine::GetInstance()->Init(GetWorkPath(), hWnd, windowWidth, windowHeight);
    
    // Main loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // Process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Engine::GetInstance()->Tick();
    }

    Engine::GetInstance()->Shutdown();

    return 0;
}