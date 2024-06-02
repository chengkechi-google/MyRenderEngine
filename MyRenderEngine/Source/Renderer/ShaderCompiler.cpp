#include "ShaderCompiler.h"
#include "Renderer.h"
#include "ShaderCache.h"
#include "Utils/log.h"
#include "Utils/string.h"
#include "Utils/assert.h"

#include <filesystem>
#include <atlbase.h> //< CComptr
#include "dxc/dxcapi.h"

class DXCIncludeHandler : public IDxcIncludeHandler
{
public:
    DXCIncludeHandler(ShaderCache* pShaderCache, IDxcUtils* pDxcUtils) : m_pShaderCache(pShaderCache), m_pDxcUtils(pDxcUtils)
    {        
    }

    HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR filename, IDxcBlob** includeSource) override
    {
        eastl::string absolutePath = std::filesystem::absolute(filename).string().c_str();
        eastl::string source = m_pShaderCache->GetCachedFileContent(absolutePath);

        *includeSource = nullptr;
        return m_pDxcUtils->CreateBlob(source.data(), (UINT32) source.size(), CP_UTF8, reinterpret_cast<IDxcBlobEncoding**>(includeSource));
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        ++ m_ref;
        return m_ref;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        -- m_ref;
        ULONG result = m_ref;
        if (result == 0)
        {
            delete this;
        }
        return result;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
    {
        if (IsEqualIID(iid, __uuidof(IDxcIncludeHandler)))
        {
            *object = dynamic_cast<IDxcIncludeHandler*>(this);
            this->AddRef();
            return S_OK;
        }
        else if (IsEqualIID(iid, __uuidof(IUnknown)))
        {
            *object = dynamic_cast<IUnknown*>(this);
            this->AddRef();
            return S_OK;
        }
        else
        {
            return E_NOINTERFACE;
        }
    }

private:
    ShaderCache* m_pShaderCache = nullptr;
    IDxcUtils* m_pDxcUtils = nullptr;
    std::atomic<ULONG> m_ref = 0;

};

ShaderCompiler::ShaderCompiler(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
    HMODULE dxc = LoadLibrary(L"dxccompiler.dll");
    if (dxc)
    {
        DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)GetProcAddress(dxc, "DxcCreateInstance");
        
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_pDxcUtils));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_pDxcCompiler));

        m_pDxcIncludeHandler = new DXCIncludeHandler(pRenderer->GetShaderCache(), m_pDxcUtils);
        m_pDxcIncludeHandler->AddRef();
    }
}

ShaderCompiler::~ShaderCompiler()
{
    m_pDxcIncludeHandler->Release();
    m_pDxcCompiler->Release();
    m_pDxcUtils->Release();
}

bool ShaderCompiler::Compile(const eastl::string& source, const eastl::string& file, const eastl::string& entryPoint,
    const eastl::string& profile, const eastl::vector<eastl::string>& defines, RHIShaderCompilerFlags flags, eastl::vector<uint8_t>& outputBlob)
{
    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = source.data();
    sourceBuffer.Size = source.length();
    sourceBuffer.Encoding = DXC_CP_ACP;

    eastl::wstring wstrFile = string_to_wstring(file);
    eastl::wstring wstrEntryPoint = string_to_wstring(entryPoint);
    eastl::wstring wstrProfile = string_to_wstring(profile);

    eastl::vector<eastl::wstring> wstrDefines;
    for (size_t i = 0; i < defines.size(); ++i)
    {
        wstrDefines.push_back(string_to_wstring(defines[i]));
    }

    eastl::vector<LPCWSTR> arguments;
    arguments.push_back(wstrFile.c_str());
    arguments.push_back(L"-E"); arguments.push_back(wstrEntryPoint.c_str());
    arguments.push_back(L"-T"); arguments.push_back(wstrProfile.c_str());
    for (size_t i = 0; i < wstrDefines.size(); ++i)
    {
        arguments.push_back(L"-D"); arguments.push_back(wstrDefines[i].c_str());
    }
    
    arguments.push_back(L"-D");
    switch (m_pRenderer->GetDevice()->GetVender())
    {
        case RHIVender::AMD:
            arguments.push_back(L"RHI_VENDOR_AMD=1");
            break;
        case RHIVender::Intel:
            arguments.push_back(L"RHI_VENDER_INTEL=1");
            break;
        case RHIVender::NVidia:
            arguments.push_back(L"RHI_VENDER_NV=1");
            break;
        default:
            MY_ASSERT(false);
            break;
    }

    arguments.push_back(L"-HV 2021");
    arguments.push_back(L"-enable-16bit-types");

#ifdef _DEBUG
    arguments.push_back(L"-Od");
    arguments.push_back(L"-Zi");
    arguments.push_back(L"-Qembed_debug");    
#endif

    if (flags & RHIShaderCompilerFlagBit::RHIShaderCompilerFlag3)
    {
        arguments.push_back(L"-O3");
    }
    else if (flags & RHIShaderCompilerFlagBit::RHIShaderCompilerFlag2)
    {
        arguments.push_back(L"-O2");
    }
    else if (flags & RHIShaderCompilerFlagBit::RHIShaderCompilerFlag1)
    {
        arguments.push_back(L"-O1");
    }
    else if (flags & RHIShaderCompilerFlagBit::RHIShaderCompilerFlag0)
    {
        arguments.push_back(L"-O0");
    }
    else
    {
#ifdef _DEBUG
    arguments.push_back(L"O0");
#else
    arguments.push_back(L"O3");
#endif
    }

    CComPtr<IDxcResult> pResults;
    m_pDxcCompiler->Compile(&sourceBuffer, arguments.data(), (UINT32) arguments.size(), m_pDxcIncludeHandler, IID_PPV_ARGS(&pResults));

    CComPtr<IDxcBlobUtf8> pErrors = nullptr;
    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
    if (pErrors != nullptr && pErrors->GetStringLength() != 0)
    {
        MY_ERROR(pErrors->GetStringPointer());
    }

    HRESULT hResult;
    pResults->GetStatus(&hResult);
    if (FAILED(hResult))
    {
        MY_ERROR("[ShaderCompiler] faild to compile shader : {} {}", file, entryPoint);
    }

    CComPtr<IDxcBlob> pShader = nullptr;
    if(FAILED(pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr)))
    {
        return false;
    }

    outputBlob.resize(pShader->GetBufferSize());
    memcpy(outputBlob.data(), pShader->GetBufferPointer(), pShader->GetBufferSize());

    return true;
}