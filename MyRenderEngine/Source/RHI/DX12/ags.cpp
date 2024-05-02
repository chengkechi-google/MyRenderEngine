#include "ags.h"
#include "amd_ags/amd_ags.h"

typedef AGSReturnCode(*PFN_agsInitialize)(int agsVersion, const AGSConfiguration* pConfig, AGSContext** ppContext, AGSGPUInfo* pGPUInfo);
typedef AGSReturnCode(*PFN_agsDeInitialize)(AGSContext* pContext);
typedef AGSReturnCode(*PFN_agsDriverExtensionsDX12_CreateDevice)(AGSContext* pContext, const AGSDX12DeviceCreationParams* pCreationParams, const AGSDX12ExtensionParams* pExtensionParams, AGSDX12ReturnedParams* pReturnedParams);
typedef AGSReturnCode(*PFN_agsDriverExtensionsDX12_DestroyDevice)(AGSContext* pContext, ID3D12Device* pDevice, unsigned int* pDeviceReferences);
typedef AGSReturnCode(*PFN_agsDriverExtensionsDX12_PushMarker)(AGSContext* pContext, ID3D12GraphicsCommandList* pCommandList, const char* pData);
typedef AGSReturnCode(*PFN_agsDriverExtensionsDX12_PopMarker)(AGSContext* pContext, ID3D12GraphicsCommandList* pCommandList);

namespace ags
{
    static PFN_agsInitialize Initialize = nullptr;
    static PFN_agsDeInitialize DeInitialize = nullptr;
    static PFN_agsDriverExtensionsDX12_CreateDevice DX12CreateDevice = nullptr;
    static PFN_agsDriverExtensionsDX12_DestroyDevice DX12DestroyDevice = nullptr;
    static PFN_agsDriverExtensionsDX12_PushMarker DX12PushMarker = nullptr;
    static PFN_agsDriverExtensionsDX12_PopMarker DX12PopMarker = nullptr;

    static AGSContext* s_pContext = nullptr;
    static AGSGPUInfo* s_pGPUInfo = nullptr;
}

HRESULT ags::CreateDevice(IDXGIAdapter* pAdapter, D3D_FEATURE_LEVEL minimumFeatureLevel, REFIID riid, void** ppDevice)
{
    HMODULE module = LoadLibrary(L"amd_ags_x64.dll");

    MY_ASSERT(module && "Create D3D12 Device");

    // Load function from dll
    if (module)
    {
        Initialize = (PFN_agsInitialize)GetProcAddress(module, "agsInitialize");
        DeInitialize = (PFN_agsDeInitialize)GetProcAddress(module, "agsDeInitialize");
        DX12CreateDevice = (PFN_agsDriverExtensionsDX12_CreateDevice)GetProcAddress(module, "agsDriverExtensionsDX12_CreateDevice");
        DX12DestroyDevice = (PFN_agsDriverExtensionsDX12_DestroyDevice)GetProcAddress(module, "agsDriverExtensionsDX12_DestroyDevice");
        DX12PushMarker = (PFN_agsDriverExtensionsDX12_PushMarker)GetProcAddress(module, "agsDriverExtensionsDX12_PushMarker");
        DX12PopMarker = (PFN_agsDriverExtensionsDX12_PopMarker)GetProcAddress(module, "agsDriverExtensionsDX12_PopMarker");

        int version = AGS_MAKE_VERSION(AMD_AGS_VERSION_MAJOR, AMD_AGS_VERSION_MINOR, AMD_AGS_VERSION_PATCH);
        AGSConfiguration config = {};
        if (AGS_SUCCESS != Initialize(version, &config, &s_pContext, s_pGPUInfo))
        {
            return S_FALSE;
        }

        AGSDX12DeviceCreationParams agsCreateionParams;
        agsCreateionParams.pAdapter = pAdapter;
        agsCreateionParams.FeatureLevel = minimumFeatureLevel;
        agsCreateionParams.iid = riid;

        AGSDX12ExtensionParams agsExtensionParams = {};
        AGSDX12ReturnedParams agsReturnedParams = {};
        AGSReturnCode agsCode = DX12CreateDevice(s_pContext, &agsCreateionParams, &agsExtensionParams, &agsReturnedParams);
        
        *ppDevice = agsReturnedParams.pDevice;
        return agsCode == AGS_SUCCESS ? S_OK : S_FALSE;
    }
    
    // Not use AGS, using normal DX12 creation function 
    return D3D12CreateDevice(pAdapter, minimumFeatureLevel, riid, ppDevice);
}

void ags::ReleaseDevice(ID3D12Device* pDevice)
{
    if (s_pContext)
    {
        DX12DestroyDevice(s_pContext, pDevice, nullptr);
        DeInitialize(s_pContext);
        s_pContext = nullptr;
    }
    else
    {
        SAFE_RELEASE(pDevice);
    }
}

void ags::BeginEvent(ID3D12GraphicsCommandList* pCommandList, const char* event)
{
    if(s_pContext)
    {
        DX12PushMarker(s_pContext, pCommandList, event);
    }
}

void ags::EndEvent(ID3D12GraphicsCommandList* pCommandList)
{
    if (s_pContext)
    {
        DX12PopMarker(s_pContext, pCommandList);
    }
}

