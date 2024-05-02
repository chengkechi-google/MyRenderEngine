#include "PixRuntime.h"

typedef HRESULT(WINAPI* BeginEventOnCommandList)(ID3D12CommandList* pCommandList, UINT64 color, _In_ PCSTR formatString);
typedef HRESULT(WINAPI* EndEventOnCommandList)(ID3D12CommandList* pCommandList);
typedef HRESULT(WINAPI* SetMarkerOnCommandList)(ID3D12CommandList* pCommandList, UINT64 color, _In_ PCSTR formatString);

static BeginEventOnCommandList pBeginEventOnCommandList = nullptr;
static EndEventOnCommandList pEndEventOnCommandList = nullptr;
static SetMarkerOnCommandList pSetMarkerOnCommandList = nullptr;

namespace PIX
{
    void Init()
    {
        HMODULE module = LoadLibrary(L"WinPixEventRuntime.dll");

        if (module)
        {
            pBeginEventOnCommandList = (BeginEventOnCommandList)(GetProcAddress(module, "BeginEventOnCommandList"));
            pEndEventOnCommandList = (EndEventOnCommandList)(GetProcAddress(module, "EndEventOnCommandList"));
            pSetMarkerOnCommandList = (SetMarkerOnCommandList)(GetProcAddress(module, "SetMarkerOnCommandList"));
        }       
    }

    void BeginEvent(ID3D12GraphicsCommandList* pCommandList, const char* event)
    {
        if (pBeginEventOnCommandList)
        {
            pBeginEventOnCommandList(pCommandList, 0, event);
        }
    }

    void EndEvent(ID3D12GraphicsCommandList* pCommandList)
    {
        if (pEndEventOnCommandList)
        {
            pEndEventOnCommandList(pCommandList);
        }
    }
}