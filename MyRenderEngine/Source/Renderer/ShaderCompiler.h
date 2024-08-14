#pragma once
#include "RHI/RHI.h"

struct IDxcCompiler3;
struct IDxcUtils;
struct IDxcIncludeHandler;

class Renderer;

class ShaderCompiler
{
public:
    ShaderCompiler(Renderer* pRenderer);
    ~ShaderCompiler();

    bool Compile(const eastl::string& source, const eastl::string& file, const eastl::string& entryPoint,
        RHIShaderType type, const eastl::vector<eastl::string>& defines, RHIShaderCompilerFlags flags,
        eastl::vector<uint8_t>& outputBlob);
private:
    Renderer* m_pRenderer = nullptr;
    IDxcCompiler3* m_pDxcCompiler = nullptr;
    IDxcUtils* m_pDxcUtils = nullptr;
    IDxcIncludeHandler* m_pDxcIncludeHandler = nullptr;
};