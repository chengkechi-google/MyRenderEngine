#include "ShaderCache.h"
#include "Renderer.h"
#include "ShaderCompiler.h"
#include "PipelineCache.h"
#include "Utils/log.h"
#include "Core/Engine.h"
#include <fstream>
#include <filesystem>
#include <regex>

inline bool operator==(const RHIShaderDesc& lhs, const RHIShaderDesc& rhs)
{
    if (lhs.m_file != rhs.m_file || lhs.m_entryPoint != rhs.m_entryPoint || lhs.m_profile != rhs.m_profile)
    {
        return false;
    }

    if (lhs.m_defines.size() != rhs.m_defines.size())
    {
        return false;
    }

    for (size_t i = 0; i < lhs.m_defines.size(); ++ i)
    {   
        if (lhs.m_defines[i] != rhs.m_defines[i])
        {
            return false;
        }

    }

    return true;
}

static inline eastl::string LoadFile(const eastl::string& path)
{
    std::ifstream is;
    is.open(path.c_str(), std::ios::binary);
    if (is.fail())
    {
        return "";
    }

    is.seekg(0, std::ios::end);
    uint32_t length = (uint32_t) is.tellg();
    is.seekg(0, std::ios::beg);

    eastl::string content;
    content.resize(length);

    is.read((char*)content.data(), length);
    is.close();

    return content;
}

ShaderCache::ShaderCache(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

IRHIShader* ShaderCache::GetShader(const eastl::string& file, const eastl::string& entryPoint, const eastl::string& profile, const eastl::vector<eastl::string>& defines, RHIShaderCompilerFlags flags)
{
    eastl::string filePath = Engine::GetInstance()->GetShaderPath() + file;
    eastl::string absolutePath = std::filesystem::absolute(filePath.c_str()).string().c_str();

    RHIShaderDesc desc;
    desc.m_file = absolutePath;
    desc.m_entryPoint = entryPoint;
    desc.m_profile = profile;
    desc.m_defines = defines; //< vector copy?
    desc.m_flags = flags;

    auto iter = m_cachedShaders.find(desc);
    if (iter != m_cachedShaders.end())
    {
        return iter->second.get();
    }

    IRHIShader* pShader = CreateShader(absolutePath, entryPoint, profile, defines, flags);
    if (pShader != nullptr)
    {
        m_cachedShaders.insert(eastl::make_pair(desc, eastl::unique_ptr<IRHIShader>(pShader)));
    }

    return pShader;
}

eastl::string ShaderCache::GetCachedFileContent(const eastl::string& file)
{
    auto iter = m_cachedFiles.find(file);
    if (iter != m_cachedFiles.end())
    {
        return iter->second;
    }

    eastl::string source = LoadFile(file);
    m_cachedFiles.insert(eastl::make_pair(file, source));
    
    return source;
}

void ShaderCache::ReloadShaders()
{
    for (auto iter = m_cachedFiles.begin(); iter != m_cachedFiles.end(); ++iter)
    {
        const eastl::string& path = iter->first;
        const eastl::string& source = iter->second;

        eastl::string newSource = LoadFile(path);
        if (source != newSource)
        {
            m_cachedFiles[path] = newSource;                //< Update cached source contents
            eastl::vector<IRHIShader*> changedShaders = GetShaderList(path);
            for (size_t i = 0; i < changedShaders.size(); ++i)
            {
                RecompileShader(changedShaders[i]);
            }
        }
    }
}

IRHIShader* ShaderCache::CreateShader(const eastl::string& file, const eastl::string& entryPoint, const eastl::string& profile, const eastl::vector<eastl::string>& defines, RHIShaderCompilerFlags flags)
{
    eastl::string source = GetCachedFileContent(file);
    
    eastl::vector<uint8_t> shaderBlob;
    if(!m_pRenderer->GetShaderCompiler()->Compile(source, file, entryPoint, profile, defines, flags, shaderBlob))
    {
        return nullptr;
    }

    RHIShaderDesc desc;
    desc.m_file = file;
    desc.m_entryPoint = entryPoint;
    desc.m_profile = profile;
    desc.m_defines = defines;

    eastl::string name = file + " : " + entryPoint + "(" + profile + ")";
    IRHIShader* pShader = m_pRenderer->GetDevice()->CreateShader(desc, shaderBlob, name);
    return pShader;
}

void ShaderCache::RecompileShader(IRHIShader* pShader)
{
    const RHIShaderDesc& desc = pShader->GetDesc();
    MY_INFO("Recompile shader : {}", desc.m_file);

    eastl::string source = GetCachedFileContent(desc.m_file);

    eastl::vector<uint8_t> shaderBlob;
    if(!m_pRenderer->GetShaderCompiler()->Compile(source, desc.m_file, desc.m_entryPoint, desc.m_profile, desc.m_defines, desc.m_flags, shaderBlob))
    {
        return;
    }

    pShader->SetShaderData(shaderBlob.data(), (uint32_t) shaderBlob.size());
    
    PipelineStateCache* pPipelineCache = m_pRenderer->GetPipelineStateCache();
    pPipelineCache->ReCreatePSO(pShader);
}

eastl::vector<IRHIShader*> ShaderCache::GetShaderList(const eastl::string& file)
{
    eastl::vector<IRHIShader*> shaders;

    for (auto iter = m_cachedShaders.begin(); iter != m_cachedShaders.end(); ++iter)
    {
        if (IsFileIncluded(iter->second.get(), file))
        {
            shaders.push_back(iter->second.get());
        }
    }

    return shaders;
}

bool ShaderCache::IsFileIncluded(const IRHIShader* pShader, const eastl::string& file)
{
    // If changed file is shader file
    const RHIShaderDesc& desc = pShader->GetDesc();
    if (desc.m_file == file)
    {
        return true;
    }

    // No shader file, we have to search include files
    eastl::string extension = std::filesystem::path(file.c_str()).extension().string().c_str();
    bool isHeader = extension == ".hlsli" || extension == ".h";

    if (isHeader)
    {
        std::string source = GetCachedFileContent(desc.m_file).c_str();
        std::regex r("#include\\s*\"\\s*\\S+.\\S+\\s*\"");

        std::smatch result;
        while (std::regex_search(source, result, r))
        {
            eastl::string include = result[0].str().c_str();
            
            size_t first = include.find_first_of('\"');
            size_t last = include.find_last_of('\"');
            eastl::string header = include.substr(first + 1, last - first - 1);

            std::filesystem::path path(desc.m_file.c_str());
            std::filesystem::path headerPath = path.parent_path() / header.c_str(); //< append

            if (std::filesystem::absolute(headerPath).string().c_str() == file)
            {
                return true;
            }

            source = result.suffix();
        }
    }

    return false;
}