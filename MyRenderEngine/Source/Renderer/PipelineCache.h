#pragma once
#include "RHI/RHI.h"
#include "xxHash/xxhash.h"
#include "EASTL/hash_map.h"
#include "EASTL/unique_ptr.h"

// Cityhash Hash128to64
inline uint64_t hash_combine_64(uint64_t hash0, uint64_t hash1)
{
    const uint64_t kMul = 0x9ddfea08eb382d69ULL;
    uint64_t a = (hash0 ^ hash1) * kMul;
    a ^= (a >> 47);
    uint64_t b = (hash0 ^ a) * kMul;
    b ^= (b >> 47);

    return b * kMul;
}

namespace eastl
{
    template<>
    struct hash<RHIGraphicsPipelineDesc>
    {
        size_t operator() (const RHIGraphicsPipelineDesc& desc) const
        {
            uint64_t vsHash = desc.m_pVS->GetHash();
            uint64_t psHash = desc.m_pPS ? desc.m_pPS->GetHash() : 0;

            const size_t stateOffset = offsetof(RHIGraphicsPipelineDesc, m_rasterizerState);
            uint64_t stateHash = XXH3_64bits((char*)&desc + stateOffset, sizeof(RHIGraphicsPipelineDesc) - stateOffset);
            
            
            uint64_t hash = hash_combine_64(hash_combine_64(vsHash, psHash), stateHash);
            static_assert(sizeof(size_t) == sizeof(uint64_t), "only support 64 bits platform");
            return hash;
        }
    };

    template<>
    struct hash<RHIMeshShaderPipelineDesc>
    {
        size_t operator() (const RHIMeshShaderPipelineDesc& desc) const
        {
            uint64_t msHash = desc.m_pMS->GetHash();
            uint64_t asHash = desc.m_pAS ? desc.m_pAS->GetHash() : 0;
            uint64_t psHash = desc.m_pPS ? desc.m_pPS->GetHash() : 0;

            const size_t stateOffset = offsetof(RHIMeshShaderPipelineDesc, m_rasterizerState);
            uint64_t stateHash = XXH3_64bits((char*)&desc + stateOffset, sizeof(RHIMeshShaderPipelineDesc) - stateOffset);
            
            uint64_t hash = hash_combine_64(hash_combine_64(hash_combine_64(msHash, asHash), psHash), stateHash);
            static_assert(sizeof(size_t) == sizeof(uint64_t), "Only support 64 bit platforms");
            return hash;
        }
    };

    template<>
    struct hash<RHIComputePipelineDesc>
    {
        size_t operator() (const RHIComputePipelineDesc& desc) const
        {
            static_assert(sizeof(size_t) == sizeof(uint64_t), "Only support 64 bit platforms");
            return desc.m_pCS->GetHash();
        }
    };
}

class Renderer;

class PipelineStateCache
{
public:
    PipelineStateCache(Renderer* pRenderer);
    
    IRHIPipelineState* GetPipelineState(const RHIGraphicsPipelineDesc& desc, const eastl::string& name);
    IRHIPipelineState* GetPipelineState(const RHIMeshShaderPipelineDesc& desc, const eastl::string& name);
    IRHIPipelineState* GetPipelineState(const RHIComputePipelineDesc& desc, const eastl::string& name);

    void ReCreatePSO(IRHIShader* pShader);

private:
    Renderer* m_pRenderer;
    eastl::hash_map<RHIGraphicsPipelineDesc, eastl::unique_ptr<IRHIPipelineState>> m_cachedGraphicsPSO;
    eastl::hash_map<RHIMeshShaderPipelineDesc, eastl::unique_ptr<IRHIPipelineState>> m_cachedMeshShaderPSO;
    eastl::hash_map<RHIComputePipelineDesc, eastl::unique_ptr<IRHIPipelineState>> m_cachedComputePSO;
};