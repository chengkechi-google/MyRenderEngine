#pragma once
#include "RHI/RHI.h"
#include "EASTL/hash_map.h"
#include "EASTL/unique_ptr.h"

namespace eastl
{
    // Template specilization
    template <>
    struct hash<RHIShaderDesc>
    {
        size_t operator()(const RHIShaderDesc& desc) const
        {
            eastl::string s = desc.m_file + desc.m_entryPoint;
            for (size_t i = 0; i < desc.m_defines.size(); ++i)
            {
                s += desc.m_defines[i];
            }

            return eastl::hash<eastl::string>{} (s);    // Constructor then call operator()
        }
    };
}

class Renderer;

class ShaderCache
{
public:
    ShaderCache(Renderer* pRenderer);

    IRHIShader* GetShader(const eastl::string& file, const eastl::string& entryPoint, RHIShaderType type, const eastl::vector<eastl::string>& defines, RHIShaderCompilerFlags flags);
    eastl::string GetCachedFileContent(const eastl::string& file);
    void ReloadShaders();
private:
    IRHIShader* CreateShader(const eastl::string& file, const eastl::string& entryPoint, RHIShaderType type, const eastl::vector<eastl::string>& defines, RHIShaderCompilerFlags flags);
    void RecompileShader(IRHIShader* pShader);
    
    eastl::vector<IRHIShader*> GetShaderList(const eastl::string& file);
    bool IsFileIncluded(const IRHIShader* pShader, const eastl::string& file);
private:
    Renderer* m_pRenderer;
    eastl::hash_map<RHIShaderDesc, eastl::unique_ptr<IRHIShader>> m_cachedShaders;
    eastl::hash_map<eastl::string, eastl::string> m_cachedFiles;
};