#include "HierarchicalDepthBufferPass.h"
#include "../Renderer.h"

#define A_CPU
#include "FFX/ffx_a.h"
#include "FFX/ffx_spd.h"

HZBPass::HZBPass(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
    RHIComputePipelineDesc desc;
    desc.m_pCS = pRenderer->GetShader("HZB/HZBReprojection.hlsl", "DepthReprojection", RHIShaderType::CS);
    m_pDepthReprojectionPSO = pRenderer->GetPipelineState(desc, "HZB depth reprojection PSO");

    desc.m_pCS = pRenderer->GetShader("HZB/HZBReprojection.hlsl", "DepthDilation", RHIShaderType::CS);
    m_pDepthDilationPSO = pRenderer->GetPipelineState(desc, "HZB depth dilation PSO");

    desc.m_pCS = pRenderer->GetShader("HZB/HZBReprojection.hlsl", "InitHZB", RHIShaderType::CS);
    m_pInitHZBPSO = pRenderer->GetPipelineState(desc, "HZB init PSO");

    desc.m_pCS = pRenderer->GetShader("HZB/HZBReprojection.hlsl", "InitSceneHZB", RHIShaderType::CS);
    m_pInitSceneHZBPSO = pRenderer->GetPipelineState(desc, "HZB init scene PSO");

    desc.m_pCS = pRenderer->GetShader("HZB/HZB.hlsl", "BuildHZB", RHIShaderType::CS);
    m_pDepthMipFilterPSO = pRenderer->GetPipelineState(desc, "HZB generate mips PSO");

    desc.m_pCS = pRenderer->GetShader("HZB/HZB.hlsl", "BuildHZB", RHIShaderType::CS, {"MAX_MIN_FILTER=1"});
    m_pDepthMipFilterMinMaxPSO = pRenderer->GetPipelineState(desc, "HZB generate min max mips PSO");
}

void HZBPass::Generate1stPhaseCullingHZB(RenderGraph* pGraph)
{
    RENDER_GRAPH_EVENT(pGraph, "HZB Pass");

    CalcHZBSize();

    struct DepthReprojectionData
    {
        RGHandle m_prevDepth;
        RGHandle m_reprojectedDepth;
    };

    auto reprojectionPass = pGraph->AddPass<DepthReprojectionData>("Depth Reprojection", RenderPassType::Compute,
        [&](DepthReprojectionData& data, RGBuilder& builder)
        {
            data.m_prevDepth = builder.Read(m_pRenderer->GetPrevSceneDepthHandle());
            RGTexture::Desc desc;
            desc.m_width = m_hzbSize.x;
            desc.m_height = m_hzbSize.y;
            desc.m_format = RHIFormat::R16F;
            data.m_reprojectedDepth = builder.Create<RGTexture>(desc, "Reprojected Depth RT");

            data.m_reprojectedDepth = builder.Write(data.m_reprojectedDepth);
        },
        [=](const DepthReprojectionData& data, IRHICommandList* pCommandList)
        {
            ReprojectDepth(pCommandList, pGraph->GetTexture(data.m_reprojectedDepth));
        });

    struct DepthDilationData
    {
        RGHandle m_reprojectedDepth;
        RGHandle m_dilatedDepth;
    };

    RGHandle hzb;   //< Hold all mipmaps
    
    auto dilationPass = pGraph->AddPass<DepthDilationData>("Depth Dilation Pass", RenderPassType::Compute,
        [&](DepthDilationData& data, RGBuilder& builder)
        {
            data.m_reprojectedDepth = builder.Read(reprojectionPass->m_reprojectedDepth);

            RGTexture::Desc desc;
            desc.m_width = m_hzbSize.x;
            desc.m_height = m_hzbSize.y;
            desc.m_mipLevels = m_hzbMipCount;
            desc.m_format = RHIFormat::R16F;
            hzb = builder.Create<RGTexture>(desc, "1st phase HZB");
            data.m_dilatedDepth = builder.Write(data.m_dilatedDepth);   //< Create node for 0 mip level
        },
        [=](const DepthDilationData& data, IRHICommandList* pCommandList)
        {
            DilateDepth(pCommandList, pGraph->GetTexture(data.m_reprojectedDepth), pGraph->GetTexture(data.m_dilatedDepth));
        });

    struct BuildHZBData
    {
        RGHandle m_hzb;
    };

    auto hzbPass = pGraph->AddPass<BuildHZBData>("Build HZB", RenderPassType::Compute,
        [&](BuildHZBData& data, RGBuilder& builder)
        {
            data.m_hzb = builder.Read(dilationPass->m_dilatedDepth);
            
            m_1stPhaseCullingHZBMips[0] = data.m_hzb;
            for (uint32_t i = 1; i < m_hzbMipCount; ++i)
            {
                m_1stPhaseCullingHZBMips[i] = builder.Write(hzb, i);
            }
        },
        [=](const BuildHZBData& data, IRHICommandList* pCommandList)
        {
            RGTexture* hzb = pGraph->GetTexture(data.m_hzb);
            BuildHZB(pCommandList, hzb);
        });
}

void HZBPass::Generate2ndPhaseCullingHZB(RenderGraph* pGraph, RGHandle depthRT)
{
    RENDER_GRAPH_EVENT(pGraph, "HZB Pass");

    struct InitHZBData
    {
        RGHandle m_inputDepthRT;
        RGHandle m_hzb;
    };
    
    RGHandle hzb;       //< Hold full mips
    
    auto initHZBPass = pGraph->AddPass<InitHZBData>("Init HZB pass", RenderPassType::Compute,
        [&](InitHZBData& data, RGBuilder& builder)
        {
            data.m_inputDepthRT = builder.Read(depthRT);

            RGTexture::Desc desc;
            desc.m_width = m_hzbSize.x;
            desc.m_height = m_hzbSize.y;
            desc.m_mipLevels = m_hzbMipCount;
            desc.m_format = RHIFormat::R16F;
            hzb = builder.Create<RGTexture>(desc, "2nd phase HZB");

            data.m_hzb = builder.Write(hzb);
        },
        [=](const InitHZBData& data, IRHICommandList* pCommandList)
        {
            InitHZB(pCommandList, pGraph->GetTexture(data.m_inputDepthRT), pGraph->GetTexture(data.m_hzb));
        });

    struct BuildHZBData
    {
        RGHandle m_hzb;
    };

    auto hzbPass = pGraph->AddPass<BuildHZBData>("Build HZB Pass", RenderPassType::Compute,
        [&](BuildHZBData& data, RGBuilder& builder)
        {
            data.m_hzb = builder.Read(initHZBPass->m_hzb);

            m_2ndPhaseCullingHZBMips[0] = data.m_hzb;
            for (uint32_t i = 1; i < m_hzbMipCount; ++i)
            {
                m_2ndPhaseCullingHZBMips[i] = builder.Write(hzb, i);
            }
        },
        [=](const BuildHZBData& data, IRHICommandList* pCommandList)
        {
            RGTexture* pHZB = pGraph->GetTexture(data.m_hzb);
            BuildHZB(pCommandList, pHZB);
        });
}

void HZBPass::GenerateSceneHZB(RenderGraph* pGraph, RGHandle depthRT)
{
    RENDER_GRAPH_EVENT(pGraph, "HZB Pass");

    struct InitHZBData
    {
        RGHandle m_inputDepthRT;
        RGHandle m_hzb;
    };

    RGHandle hzb;

    auto initHZBPass = pGraph->AddPass<InitHZBData>("Init HZB pass", RenderPassType::Compute,
        [&](InitHZBData& data, RGBuilder& builder)
        {
            data.m_inputDepthRT = builder.Read(depthRT);
            
            RGTexture::Desc desc;
            desc.m_width = m_hzbSize.x;
            desc.m_height = m_hzbSize.y;
            desc.m_mipLevels = m_hzbMipCount;
            desc.m_format = RHIFormat::RG16F;
            hzb = builder.Create<RGTexture>(desc, "SceneHZB");

            data.m_hzb = builder.Write(hzb);
        },
        [=](const InitHZBData& data, IRHICommandList* pCommandList)
        {
            InitHZB(pCommandList, pGraph->GetTexture(data.m_inputDepthRT), pGraph->GetTexture(data.m_hzb), true);
        });

    struct BuildHZBData
    {
        RGHandle m_hzb;
    };

    auto buildHZBPass = pGraph->AddPass<BuildHZBData>("Build HZB", RenderPassType::Compute,
        [&](BuildHZBData& data, RGBuilder& builder)
        {
            data.m_hzb = builder.Read(initHZBPass->m_hzb);

            m_sceneHZBMips[0] = data.m_hzb;
            for (uint32_t i = 1; i < m_hzbMipCount; ++i)
            {
                m_sceneHZBMips[i] = builder.Write(hzb, i);
            }
        },
        [=](const BuildHZBData& data, IRHICommandList* pCommandList)
        {
            RGTexture* pHZB = pGraph->GetTexture(data.m_hzb);
            BuildHZB(pCommandList, pHZB, true);
        });
}

RGHandle HZBPass::Get1stPhaseCullingHZBMip(uint32_t mip) const
{
    MY_ASSERT(mip < m_hzbMipCount);
    return m_1stPhaseCullingHZBMips[mip];
}

RGHandle HZBPass::Get2ndPhaseCullingHZBMip(uint32_t mip) const
{
    MY_ASSERT(mip < m_hzbMipCount);
    return m_2ndPhaseCullingHZBMips[mip];
}

RGHandle HZBPass::GetSceneHZBMip(uint32_t mip) const
{
    MY_ASSERT(mip < m_hzbMipCount);
    return m_sceneHZBMips[mip];
}

void HZBPass::CalcHZBSize()
{
    uint32_t mipX = (uint32_t)eastl::max(ceilf(log2f((float) m_pRenderer->GetRenderWidth())), 1.0f);
    uint32_t mipY = (uint32_t)eastl::max(ceilf(log2f((float) m_pRenderer->GetRenderHeight())), 1.0f);

    m_hzbMipCount = eastl::max(mipX, mipY);
    MY_ASSERT(m_hzbMipCount <= MAX_HZB_MIP_COUNT);

    m_hzbSize.x = 1 << (mipX - 1);
    m_hzbSize.y = 1 << (mipY - 1);
}

void HZBPass::ReprojectDepth(IRHICommandList* pCommandList, RGTexture* pReprojectedDepthTexture)
{
    float clearValue[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    pCommandList->ClearUAV(pReprojectedDepthTexture->GetTexture(), pReprojectedDepthTexture->GetUAV(), clearValue);
    pCommandList->TextureBarrier(pReprojectedDepthTexture->GetTexture(), RHI_ALL_SUB_RESOURCE, RHIAccessBit::RHIAccessClearUAV, RHIAccessBit::RHIAccessComputeShaderUAV);

    pCommandList->SetPipelineState(m_pDepthReprojectionPSO);

    uint32_t rootConsts[4] = {
        0,
        pReprojectedDepthTexture->GetUAV()->GetHeapIndex(),
        m_hzbSize.x,
        m_hzbSize.y
    };
    pCommandList->SetComputeConstants(0, rootConsts, sizeof(rootConsts));
    pCommandList->Dispatch(DivideRoundingUp(m_hzbSize.x, 8), DivideRoundingUp(m_hzbSize.y, 8), 1);
}

void HZBPass::DilateDepth(IRHICommandList* pCommandList, RGTexture* pReprojectedDepthSRV, RGTexture* pHZBMip0UAV)
{
    pCommandList->SetPipelineState(m_pDepthDilationPSO);

    uint32_t rootConsts[4] = {pReprojectedDepthSRV->GetSRV()->GetHeapIndex(), pHZBMip0UAV->GetUAV()->GetHeapIndex(), m_hzbSize.x, m_hzbSize.y};
    pCommandList->SetComputeConstants(0, rootConsts, sizeof(rootConsts));
    pCommandList->Dispatch(DivideRoundingUp(m_hzbSize.x, 8), DivideRoundingUp(m_hzbSize.y, 8), 1);
}

void HZBPass::BuildHZB(IRHICommandList* pCommandList, RGTexture* pTexture, bool bMinMax)
{
    pCommandList->SetPipelineState(bMinMax ? m_pDepthMipFilterMinMaxPSO : m_pDepthMipFilterPSO);

    const RHITextureDesc& textureDesc = pTexture->GetTexture()->GetDesc();

    varAU2(dispatchThreadGroupXY);
    varAU2(workGroupOffset);
    varAU2(numWorkGroupsAndMips);
    varAU4(rectInfo) = initAU4(0, 0, textureDesc.m_width, textureDesc.m_height); // Left, top, right, bottom
    SpdSetup(dispatchThreadGroupXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, textureDesc.m_mipLevels - 1);

    struct SPDConstants
    {
        uint m_mips;
        uint m_numWorkGroups;
        uint2 m_workGroupOffset;

        float2 m_invInputSize;
        uint m_imgSrc;
        uint m_spdGlobalAtomicUAV;

        // HLSL packing rule : every element in an array is stored in a four-component vector
        uint4 m_imgDst[12]; // Do not access mip 5, that use for spd
    };

    SPDConstants constants = {};
    constants.m_numWorkGroups = numWorkGroupsAndMips[0];
    constants.m_mips = numWorkGroupsAndMips[1];
    constants.m_workGroupOffset[0] = workGroupOffset[0];
    constants.m_workGroupOffset[1] = workGroupOffset[1];
    constants.m_invInputSize[0] = 1.0f / textureDesc.m_width;
    constants.m_invInputSize[1] = 1.0f / textureDesc.m_height;

    constants.m_imgSrc = pTexture->GetSRV()->GetHeapIndex();
    constants.m_spdGlobalAtomicUAV = m_pRenderer->GetSPDCounterBuffer()->GetUAV()->GetHeapIndex();

    for (uint32_t i = 0; i < textureDesc.m_mipLevels - 1; ++i)
    {
        constants.m_imgDst[i].x = pTexture->GetUAV(i + 1, 0)->GetHeapIndex();
    }

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));

    uint32_t dispatchX = dispatchThreadGroupXY[0];
    uint32_t dispatchY = dispatchThreadGroupXY[1];
    uint32_t dispatchZ = 1;
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);
}

void HZBPass::InitHZB(IRHICommandList* pCommandList, RGTexture* pInputDepthSRV, RGTexture* pHZBMip0UAV, bool bMinMax)
{
    pCommandList->SetPipelineState(bMinMax ? m_pInitSceneHZBPSO : m_pInitHZBPSO);

    uint32_t rootConsts[4] = { pInputDepthSRV->GetSRV()->GetHeapIndex(), pHZBMip0UAV->GetUAV()->GetHeapIndex(), m_hzbSize.x, m_hzbSize.y };
    pCommandList->SetComputeConstants(0, rootConsts, sizeof(rootConsts));

    pCommandList->Dispatch(DivideRoundingUp(m_hzbSize.x, 8), DivideRoundingUp(m_hzbSize.y, 8), 1);
}
