#include "PipelineCache.h"
#include "Renderer.h"

inline bool operator==(const RHIGraphicsPipelineDesc& lhs, const RHIGraphicsPipelineDesc& rhs)
{
    // Maybe just use raw pointer equal
    if (lhs.m_pVS->GetHash() != rhs.m_pVS->GetHash())
    {
        return false;
    }

    uint64_t lhsPSHash = lhs.m_pPS ? lhs.m_pPS->GetHash() : 0;
    uint64_t rhsPSHash = lhs.m_pPS ? rhs.m_pPS->GetHash() : 0;
    if (lhsPSHash != rhsPSHash)
    {
        return false;
    }

    const size_t stateOffset = offsetof(RHIGraphicsPipelineDesc, m_rasterizerState);
    void* lhsStates = (char*)&lhs + stateOffset;
    void* rhsStates = (char*)&rhs + stateOffset;

    return memcmp(lhsStates, rhsStates, sizeof(RHIGraphicsPipelineDesc) - stateOffset) == 0;
}

inline bool operator==(const RHIMeshShaderPipelineDesc& lhs, const RHIMeshShaderPipelineDesc& rhs)
{
    if (lhs.m_pMS->GetHash() != rhs.m_pMS->GetHash())
    {
        return false;
    }

    uint64_t lhsPSHash = lhs.m_pPS ? lhs.m_pPS->GetHash() : 0;
    uint64_t rhsPSHash = rhs.m_pPS ? rhs.m_pPS->GetHash() : 0;
    if (lhsPSHash != rhsPSHash)
    {
        return false;
    }

    uint64_t lhsASHash = lhs.m_pAS ? lhs.m_pAS->GetHash() : 0;
    uint64_t rhsASHash = rhs.m_pAS ? rhs.m_pAS->GetHash() : 0;
    if (lhsASHash != rhsASHash)
    {
        return false;
    }

    const size_t stateOffset = offsetof(RHIMeshShaderPipelineDesc, m_rasterizerState);
    void* lhsStates = (char*)&lhs + stateOffset;
    void* rhsStates = (char*)&rhs + stateOffset;

    return memcmp(lhsStates, rhsStates, sizeof(RHIMeshShaderPipelineDesc) - stateOffset) == 0;
}

inline bool operator==(const RHIComputePipelineDesc& lhs, const RHIComputePipelineDesc& rhs)
{
    return lhs.m_pCS->GetHash() == rhs.m_pCS->GetHash();
}

PipelineStateCache::PipelineStateCache(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;
}

IRHIPipelineState* PipelineStateCache::GetPipelineState(const RHIGraphicsPipelineDesc& desc, const eastl::string& name)
{
    // If alread in the cache
    auto iter = m_cachedGraphicsPSO.find(desc);
    if (iter != m_cachedGraphicsPSO.end())
    {
        return iter->second.get();
    }

    // Not in the cache
    IRHIPipelineState* pPSO = m_pRenderer->GetDevice()->CreateGraphicsPipelineState(desc, name);
    if (pPSO)
    {
        m_cachedGraphicsPSO.insert(eastl::make_pair(desc, eastl::unique_ptr<IRHIPipelineState>(pPSO)));
    }

    return pPSO;
}

IRHIPipelineState* PipelineStateCache::GetPipelineState(const RHIMeshShaderPipelineDesc& desc, const eastl::string& name)
{
    auto iter = m_cachedMeshShaderPSO.find(desc);
    if (iter != m_cachedMeshShaderPSO.end())
    {
        return iter->second.get();
    }

    IRHIPipelineState* pPSO = m_pRenderer->GetDevice()->CreateMeshShaderPipelineState(desc, name);
    if (pPSO)
    {
        m_cachedMeshShaderPSO.insert(eastl::make_pair(desc, eastl::unique_ptr<IRHIPipelineState>(pPSO)));
    }

    return pPSO;
}

IRHIPipelineState* PipelineStateCache::GetPipelineState(const RHIComputePipelineDesc& desc, const eastl::string& name)
{
    auto iter = m_cachedComputePSO.find(desc);
    if (iter != m_cachedComputePSO.end())
    {
        return iter->second.get();
    }

    IRHIPipelineState* pPSO = m_pRenderer->GetDevice()->CreateComputePipelineState(desc, name);
    if (pPSO)
    {
        m_cachedComputePSO.insert(eastl::make_pair(desc, eastl::unique_ptr<IRHIPipelineState>(pPSO)));
    }

    return pPSO;
}

void PipelineStateCache::ReCreatePSO(IRHIShader* pShader)
{
    for(auto iter = m_cachedGraphicsPSO.begin(); iter != m_cachedGraphicsPSO.end(); ++iter)
    {
        const RHIGraphicsPipelineDesc& desc = iter->first;
        IRHIPipelineState* pPSO = iter->second.get();

        if (desc.m_pVS == pShader || desc.m_pPS == pShader)
        {
            pPSO->Create();
        }
    }

    for(auto iter = m_cachedMeshShaderPSO.begin(); iter != m_cachedMeshShaderPSO.end(); ++iter)
    {
        const RHIMeshShaderPipelineDesc& desc = iter->first;
        IRHIPipelineState* pPSO = iter->second.get();
        if(desc.m_pMS == pShader || desc.m_pAS == pShader || desc.m_pPS == pShader)
        { 
            pPSO->Create();
        }
    }

    for (auto iter = m_cachedComputePSO.begin(); iter != m_cachedComputePSO.end(); ++iter)
    {
        const RHIComputePipelineDesc& desc = iter->first;
        IRHIPipelineState* pPSO = iter->second.get();
        if (desc.m_pCS == pShader)
        {
            pPSO->Create();
        }
    }
    
}