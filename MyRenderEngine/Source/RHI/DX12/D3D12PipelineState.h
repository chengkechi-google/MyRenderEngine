#pragma once
#include "D3D12Headers.h"
#include "../RHIPipelineState.h"

class D3D12Device;

class D3D12GraphicsPipelineState : public IRHIPipelineState
{
public:
    D3D12GraphicsPipelineState(D3D12Device* pDevice, const RHIGraphicsPipelineDesc& desc, const eastl::string name);
    ~D3D12GraphicsPipelineState();

    virtual void* GetHandle() const { return m_pPipelineState; }
    virtual bool Create() override;

    D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveTopology() const { return m_primitiveTipology; }
    
private:
    ID3D12PipelineState* m_pPipelineState = nullptr;
    RHIGraphicsPipelineDesc m_desc;
    D3D_PRIMITIVE_TOPOLOGY m_primitiveTipology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
};

class D3D12ComputePipeline : public IRHIPipelineState
{
public:
    D3D12ComputePipeline(D3D12Device* pDevice, const RHIComputePipelineDesc& desc, const eastl::string& name);
    ~D3D12ComputePipeline();
    
    virtual void* GetHandle() const { return m_pPipelineState; }
    virtual bool Create();
    
private:
    ID3D12PipelineState* m_pPipelineState = nullptr;
    RHIComputePipelineDesc m_desc;
};

class D3D12MeshShaderPipeline : public IRHIPipelineState
{
public:
    D3D12MeshShaderPipeline(D3D12Device* pDevice, const RHIMeshShaderPipelineDesc& desc, const eastl::string& name);
    ~D3D12MeshShaderPipeline();

    virtual void* GetHandle() const { return m_pPipelineState; }
    virtual bool Create();

private:
    ID3D12PipelineState* m_pPipelineState = nullptr;
    RHIMeshShaderPipelineDesc m_desc;
};