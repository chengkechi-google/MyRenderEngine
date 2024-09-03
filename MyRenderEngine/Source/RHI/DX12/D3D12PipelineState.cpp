#include "D3D12PipelineState.h"
#include "D3D12Device.h"
#include "D3D12Shader.h"
#include "Utils/log.h"

template<class T>
inline bool HasRTBinding(const T& desc)
{
    for (int i = 0; i < RHI_MAX_RENDER_TARGET_ACCOUNT; ++i)
    {
        if (desc.m_rtFormat[i] != RHIFormat::Unknown)
        {
            return true;
        }
    }
    return false;
}

D3D12GraphicsPipelineState::D3D12GraphicsPipelineState(D3D12Device* pDevice, const RHIGraphicsPipelineDesc& desc, const eastl::string name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = RHIPipelineType::Graphics;
    m_primitiveTipology = RHIToD3D12Topology(desc.m_primitiveType);
}

D3D12GraphicsPipelineState::~D3D12GraphicsPipelineState()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    pDevice->Delete(m_pPipelineState);
}

bool D3D12GraphicsPipelineState::Create()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = ((D3D12Device*) m_pDevice)->GetRootSignature();

    desc.VS = ((D3D12Shader*) m_desc.m_pVS)->GetByteCode();
    if (m_desc.m_pPS)
    {
        desc.PS = ((D3D12Shader*) m_desc.m_pPS)->GetByteCode();
    }

    desc.RasterizerState = RHIToD3D12RasterizerDesc(m_desc.m_rasterizerState);
    desc.DepthStencilState = RHIToD3D12DepthStencilDesc(m_desc.m_depthStencilState);
    desc.BlendState = RHIToD3DBlendDesc(m_desc.m_blendState);

    desc.NumRenderTargets = HasRTBinding(m_desc) ? RHI_MAX_RENDER_TARGET_ACCOUNT : 0;
    for (int i = 0; i < RHI_MAX_RENDER_TARGET_ACCOUNT; ++i)
    {
        desc.RTVFormats[i] = DXGIFormat(m_desc.m_rtFormat[i]);
    }
    desc.DSVFormat = DXGIFormat(m_desc.m_depthStencilFromat);
    desc.PrimitiveTopologyType = RHIToD3D12TopologyType(m_desc.m_primitiveType);
    desc.SampleMask = 0xFFFFFFFF;
    desc.SampleDesc.Count = 1;

    ID3D12PipelineState* pPipelineState = nullptr;
    ID3D12Device* pDevice = (ID3D12Device*) m_pDevice->GetHandle();

    if (FAILED(pDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pPipelineState))))
    {
        MY_ERROR("[D3D12GraphicsPipelineState] failed to create {}", m_name);
        return false; 
    }

    pPipelineState->SetName(string_to_wstring(m_name).c_str());
    if (m_pPipelineState)
    {
        ((D3D12Device*) m_pDevice)->Delete(m_pPipelineState);
    }

    m_pPipelineState = pPipelineState;
    return true;
}

D3D12ComputePipelineState::D3D12ComputePipelineState(D3D12Device* pDevice, const RHIComputePipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
    m_type = RHIPipelineType::Compute;
}

D3D12ComputePipelineState::~D3D12ComputePipelineState()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    pDevice->Delete(m_pPipelineState);
}

bool D3D12ComputePipelineState::Create()
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = ((D3D12Device*) m_pDevice)->GetRootSignature();
    desc.CS = ((D3D12Shader*) m_desc.m_pCS)->GetByteCode();

    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12Device* pDevice = (ID3D12Device*) m_pDevice->GetHandle();
    if (FAILED(pDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipelineState))))
    {
        MY_ERROR("[D3D12ComputePipelineState] failed to create {}", m_name);
        return false;
    }

    pipelineState->SetName(string_to_wstring(m_name).c_str());

    if (m_pPipelineState)
    {
        ((D3D12Device*) m_pDevice)->Delete(m_pPipelineState);
    }

    m_pPipelineState = pipelineState;
    return true;
}

D3D12MeshShaderPipelineState::D3D12MeshShaderPipelineState(D3D12Device* pDevice, const RHIMeshShaderPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc= desc;
    m_name = name;
    m_type = RHIPipelineType::MeshShading;
}

D3D12MeshShaderPipelineState::~D3D12MeshShaderPipelineState()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    pDevice->Delete(m_pPipelineState);
}

bool D3D12MeshShaderPipelineState::Create()
{
    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = ((D3D12Device*) m_pDevice)->GetRootSignature();
    if (m_desc.m_pAS)
    {
        psoDesc.AS = ((D3D12Shader*) m_desc.m_pAS)->GetByteCode();
    }

    psoDesc.MS = ((D3D12Shader*) m_desc.m_pMS)->GetByteCode();

    if (m_desc.m_pPS)
    {
        psoDesc.PS = ((D3D12Shader*) m_desc.m_pPS)->GetByteCode();
    }

    psoDesc.RasterizerState = RHIToD3D12RasterizerDesc(m_desc.m_rasterizerState);
    psoDesc.DepthStencilState = RHIToD3D12DepthStencilDesc(m_desc.m_depthStencilState);
    psoDesc.BlendState = RHIToD3DBlendDesc(m_desc.m_blendState);

    psoDesc.NumRenderTargets = HasRTBinding(m_desc) ? RHI_MAX_RENDER_TARGET_ACCOUNT : 0;
    for (int i = 0; i < RHI_MAX_RENDER_TARGET_ACCOUNT; ++i)
    {
        psoDesc.RTVFormats[i] = DXGIFormat(m_desc.m_rtFormat[i]);
    }
    psoDesc.DSVFormat = DXGIFormat(m_desc.m_depthStencilFromat);
    psoDesc.SampleMask = 0xFFFFFFFF;
    psoDesc.SampleDesc.Count = 1;

    auto psoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(psoDesc);

    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc;
    streamDesc.pPipelineStateSubobjectStream = &psoStream;
    streamDesc.SizeInBytes = sizeof(psoStream);

    ID3D12PipelineState* pipelineState = nullptr;
    ID3D12Device2* pDevice = (ID3D12Device2*) m_pDevice->GetHandle();
    HRESULT hResult = pDevice->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hResult))
    {
        MY_ERROR("[D3D12MeshShaderPipelineState] failed to create {}", m_name);
        return false;
    }

    pipelineState->SetName(string_to_wstring(m_name).c_str());

    if (m_pPipelineState)
    {
        ((D3D12Device*) m_pDevice)->Delete(m_pPipelineState);
    }
    m_pPipelineState = pipelineState;
    
    return true;
}