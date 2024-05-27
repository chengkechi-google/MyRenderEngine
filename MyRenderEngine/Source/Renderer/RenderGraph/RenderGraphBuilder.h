#pragma once

#include "RenderGraph.h"
#include "RHI/RHIDefines.h"

enum class RGBuilderFlag
{
    None,
    ShaderStagePS,
    ShaderStageNonPS
};

class RGBuilder
{
public:
    RGBuilder(RenderGraph* pGraph, RenderGraphPassBase* pPass)
    {
        m_pGraph = pGraph;
        m_pPass = pPass;
    }

    void SkipCulling() { m_pPass->MakeTarget(); }
    
    template<typename Resource>
    RGHandle Create(const typename Resource::Desc& desc, const eastl::string& name)
    {
        return m_pGraph->Create<Resource>(desc, name);
    }

    RGHandle Import(IRHITexture* pTexture, RHIAccessFlags state)
    {
        return m_pGraph->Import(pTexture, state);
    }

    RGHandle Import(IRHIBuffer* pBuffer, RHIAccessFlags state)
    {
        return m_pGraph->Import(pBuffer, state);
    }

    RGHandle Read(const RGHandle& input, RHIAccessFlags usage, uint32_t subresource)
    {
        MY_ASSERT(usage & (RHIAccessBit::RHIAccessMaskSRV | RHIAccessBit::RHIAccessIndirectArgs | RHIAccessCopySrc));
        MY_ASSERT(RHI_ALL_SUB_RESOURCE != subresource); //< Render graph doesn't support RHI_ALL_SUB_RESOURCE currently
    
        return m_pGraph->Read(m_pPass, input, usage, subresource);
    }

    RGHandle Read(const RGHandle& input, uint32_t subresource = 0, RGBuilderFlag flag = RGBuilderFlag::None)
    {
        RHIAccessFlags state;
    
        switch (m_pPass->GetType())
        {
            case RenderPassType::Graphics:
            {
                if (flag == RGBuilderFlag::ShaderStagePS)
                {
                    state = RHIAccessBit::RHIAccessPixelShaderSRV;
                }
                else if (flag == RGBuilderFlag::ShaderStageNonPS)
                {
                    state = RHIAccessBit::RHIAccessVertexShaderSRV;
                }
                else
                {
                    state = RHIAccessBit::RHIAccessVertexShaderSRV | RHIAccessBit::RHIAccessPixelShaderSRV;
                }
            }

            case RenderPassType::Compute:
            case RenderPassType::AsyncCompute:
            {
                state = RHIAccessBit::RHIAccessComputeShaderSRV;
            }

            case RenderPassType::Copy:
            {
                state = RHIAccessBit::RHIAccessCopySrc;
            }
        
            default:
            {
                MY_ASSERT(false);
                break;
            }
            
        }
        return Read(input, state, subresource);
    }

    RGHandle ReadIndirectArg(const RGHandle& input, uint32_t subresource = 0)
    {
        return Read(input, RHIAccessBit::RHIAccessIndirectArgs, subresource);
    }

    RGHandle Write(const RGHandle& input, RHIAccessFlags usage, uint32_t subresource)
    {
        MY_ASSERT(usage & (RHIAccessBit::RHIAccessMaskUAV | RHIAccessBit::RHIAccessCopyDst));
        MY_ASSERT(RHI_ALL_SUB_RESOURCE != subresource); // todo: Render graph doesn't support RHI_ALL_SUB_RESOURCE currently
        return m_pGraph->Write(m_pPass, input, usage, subresource);
    }
    
    RGHandle Write(const RGHandle& input, uint32_t subresource = 0, RGBuilderFlag flag = RGBuilderFlag::None)
    {
        RHIAccessFlags state;
        
        switch (m_pPass->GetType())
        {
            case RenderPassType::Graphics:
            {
                if (flag == RGBuilderFlag::ShaderStagePS)
                {
                    state = RHIAccessBit::RHIAccessPixelShaderUAV;
                }
                else if (flag == RGBuilderFlag::ShaderStageNonPS)
                {
                    state = RHIAccessBit::RHIAccessVertexShaderUAV;
                }
                else
                {
                    state = RHIAccessBit::RHIAccessPixelShaderUAV | RHIAccessBit::RHIAccessVertexShaderUAV;
                }
                break;
            }

            case RenderPassType::Compute:
            case RenderPassType::AsyncCompute:
            {
                state = RHIAccessBit::RHIAccessComputeShaderUAV | RHIAccessBit::RHIAccessClearUAV;
                break;
            }

            case RenderPassType::Copy:
            {
                state = RHIAccessBit::RHIAccessCopyDst;
                break;
            }
            
            default:
            {
                MY_ASSERT(false);
                break;
            }
        }
        Write(input, state, subresource);
    }

    RGHandle WriteColor(uint32_t colorIndex, const RGHandle& input, uint32_t subresource, RHIRenderPassLoadOp loadOp, float4 clearColor = float4(0.0f, 0.0f, 0.0f, 1.0f))
    {
        MY_ASSERT(m_pPass->GetType() == RenderPassType::Graphics);
        return m_pGraph->WriteColor(m_pPass, colorIndex, input, subresource, loadOp, clearColor);
    }

    RGHandle WriteDepth(const RGHandle& input, uint32_t subresource, RHIRenderPassLoadOp loadOp, float clearDepth = 0.0f)
    {
        MY_ASSERT(m_pPass->GetType() == RenderPassType::Graphics);
        return m_pGraph->WriteDepth(m_pPass, input, subresource, loadOp, RHIRenderPassLoadOp::DontCare, clearDepth, 0);
    }

    RGHandle WriteDepth(const RGHandle& input, uint32_t subresource, RHIRenderPassLoadOp depthLoadOp, RHIRenderPassLoadOp stencilLoadOp, float clearDepth = 0.0f, uint32_t clearStencil = 0)
    {
        MY_ASSERT(m_pPass->GetType() == RenderPassType::Graphics);
        return m_pGraph->WriteDepth(m_pPass, input, subresource, depthLoadOp, stencilLoadOp, clearDepth, clearStencil);
    }

    RGHandle ReadDepth(const RGHandle& input, uint32_t subresource)
    {
        MY_ASSERT(m_pPass->GetType() == RenderPassType::Graphics);
        return m_pGraph->ReadDepth(m_pPass, input, subresource);
    }
private:
    RGBuilder(RGBuilder const&) = delete;
    RGBuilder& operator=(RGBuilder& const) = delete;

private:
    RenderGraph* m_pGraph = nullptr;
    RenderGraphPassBase* m_pPass = nullptr;
};