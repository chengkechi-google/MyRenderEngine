#include "ClusteredLightCulling.h"

#include <fmt/core.h>

static constexpr int gClusterTexelSize = 64;
static constexpr int gClusterZSliceNum = 32;
static constexpr int gClusterMaxLights = 1024;
static_assert(gClusterMaxLights % 32 == 0);

ClusteredLightCulling::ClusteredLightCulling(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    // Create PSOs
    RHIComputePipelineDesc desc;
    desc.m_pCS = pRenderer->GetShader("ClusteredLighting/ClusteredLightCulling.hlsl", "cs_frustum_culling_main", RHIShaderType::CS);
    m_pFrustumLightCullingPSO = pRenderer->GetPipelineState(desc, "Frustum Light Culling PSO");
}

void ClusteredLightCulling::AddPass(RenderGraph* pRenderGraph, ClusteredLightData& clusteredLightData, uint32_t lightCount)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "Clustered Light Culling")
    
    // Frustum culling
    struct FrustumLightCullingData
    {
        RGHandle m_outputLightCountBuffer;
        RGHandle m_outputLightIDBuffer;
    };

    auto frustumLightCullingPass = pRenderGraph->AddPass<FrustumLightCullingData>("Frustum Light Culling Pass", RenderPassType::Compute,
        [&](FrustumLightCullingData& data, RGBuilder& builder)
        {
            RGBuffer::Desc bufferDesc;
            bufferDesc.m_stride = sizeof(uint32_t);
            bufferDesc.m_size = 1;
            bufferDesc.m_format = RHIFormat::R32UI;
            bufferDesc.m_usage = RHIBufferUsageBit::RHIBufferUsageTypedBuffer;

            data.m_outputLightCountBuffer = builder.Create<RGBuffer>(bufferDesc, "frustum light culling count buffer");
            data.m_outputLightCountBuffer = builder.Write(data.m_outputLightCountBuffer);
            
            bufferDesc.m_stride = sizeof(uint16_t);
            bufferDesc.m_format = RHIFormat::R16UI;
            bufferDesc.m_size = lightCount * bufferDesc.m_stride;
            data.m_outputLightIDBuffer = builder.Create<RGBuffer>(bufferDesc, "frustum light culling ID buffer");
            data.m_outputLightIDBuffer = builder.Write(data.m_outputLightIDBuffer);
        },
        [=](const FrustumLightCullingData& data, IRHICommandList* pCommandList)
        {
            FrustumCulling(pCommandList,
                lightCount,
                pRenderGraph->GetBuffer(data.m_outputLightCountBuffer),
                pRenderGraph->GetBuffer(data.m_outputLightIDBuffer));
        });
}

void ClusteredLightCulling::FrustumCulling(IRHICommandList* pCommandList, uint32_t lightCount,
    RGBuffer* pFrustumLightCullingCountBuffer, RGBuffer* pFrustumLightCullingIDBuffer)
{
    pCommandList->SetPipelineState(m_pFrustumLightCullingPSO);
    uint32_t cb[3] =
    {
        lightCount,
        pFrustumLightCullingCountBuffer->GetUAV()->GetHeapIndex(),
        pFrustumLightCullingIDBuffer->GetUAV()->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, cb, sizeof(cb));

    uint32_t groupCount = max(DivideRoundingUp(lightCount, 64), 1);
    pCommandList->Dispatch(groupCount, 1, 1);
}

