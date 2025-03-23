#include "GTAO.h"
#include "Renderer/Renderer.h"
#include "Core/Engine.h"
#include "GTAO/XeGTAO.h"


GTAO::GTAO(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    eastl::vector<eastl::string> defines;
    defines.push_back("XE_GTAO_USE_HALF_FLOAT_PRECISION=0");

    RHIComputePipelineDesc desc;
    // eastl::string can not include 23 characters, I am not sure, but maybe it is because of small string optimization
    desc.m_pCS = pRenderer->GetShader("GTAO/GTAO.hlsl", "GTAOPreFilterDepth16x16CS", RHIShaderType::CS, defines);
    m_pPrefilterDepthPSO = pRenderer->GetPipelineState(desc, "GTAO Prefilter depth PSO");

    desc.m_pCS = pRenderer->GetShader("GTAO/GTAO.hlsl", "GTAODenoise", RHIShaderType::CS, defines);
    m_pDenoisePSO = pRenderer->GetPipelineState(desc, "GTAO denoise PSO");

    defines.push_back("QUALITY_LEVEL=0");
    desc.m_pCS = pRenderer->GetShader("GTAO/GTAO.hlsl", "GTAOMain", RHIShaderType::CS, defines);
    m_pGTAOLowPSO = pRenderer->GetPipelineState(desc, "GTAO low PSO");

    defines.back() = "QUALITY_LEVEL=1";
    desc.m_pCS = pRenderer->GetShader("GTAO/GTAO.hlsl", "GTAOMain", RHIShaderType::CS, defines);
    m_pGTAOMediumPSO = pRenderer->GetPipelineState(desc, "GTAO medium PSO");

    defines.back() = "QUALITY_LEVEL=2";
    desc.m_pCS = pRenderer->GetShader("GTAO/GTAO.hlsl", "GTAOMain", RHIShaderType::CS, defines);
    m_pGTAOHighPSO = pRenderer->GetPipelineState(desc, "GTAO high PSO");

    defines.back() = "QUALITY_LEVEL=3";
    desc.m_pCS = pRenderer->GetShader("GTAO/GTAO.hlsl", "GTAOMain", RHIShaderType::CS, defines);
    m_pGTAOUltraPSO = pRenderer->GetPipelineState(desc, "GTAO ultra PSO");

    // PSOs with GTSO
    {
        defines.pop_back();
        defines.push_back("XE_GTAO_COMPUTE_BENT_NORMALS=1");

        desc.m_pCS = pRenderer->GetShader("GTAO/GTAO.hlsl", "GTAODenoise", RHIShaderType::CS, defines);
        m_pSODenoisePSO = pRenderer->GetPipelineState(desc, "GTSO denoise PSO");

        defines.push_back("QUALITY_LEVEL=0");
        desc.m_pCS = pRenderer->GetShader("GTAO/GTAO.hlsl", "GTAOMain", RHIShaderType::CS, defines);
        m_pGTAOSOLowPSO = pRenderer->GetPipelineState(desc, "GTAOSO Low PSO");

        defines.back() = "QUALITY_LEVEL=1";
        desc.m_pCS = pRenderer->GetShader("GTAO/GTAO.hlsl", "GTAOMain", RHIShaderType::CS, defines);
        m_pGTAOSOMediumPSO = pRenderer->GetPipelineState(desc, "GTAOSO Medium PSO");

        defines.back() = "QUALITT_LEVEL=2";
        desc.m_pCS = pRenderer->GetShader("GTAO/GTAO.hlsl", "GTAOMain", RHIShaderType::CS, defines);
        m_pGTAOSOHighPSO = pRenderer->GetPipelineState(desc, "GTAOSO High PSO");

        defines.back() = "QUALITY_LEVEL=3";
        desc.m_pCS = pRenderer->GetShader("GTAO/GTAO.hlsl", "GTAOMain", RHIShaderType::CS, defines);
        m_pGTAOSOUltraPSO = pRenderer->GetPipelineState(desc, "GTAOSO Ultra PSO");
    }

    CreateHilbertLUT();
}

RGHandle GTAO::AddPasse(RenderGraph* pRenderGraph, RGHandle depthRT, RGHandle normalRT, uint32_t width, uint32_t height)
{
    if (!m_bEnable)
    {
        return RGHandle();
    }

    RENDER_GRAPH_EVENT(pRenderGraph, "GTAO");
    // Depth prefilter
    
    struct PrefilterDepthPassData
    {
        RGHandle m_inputDepth;
        RGHandle m_outputDepthMip0;
        RGHandle m_outputDepthMip1;
        RGHandle m_outputDepthMip2;
        RGHandle m_outputDepthMip3;
        RGHandle m_outputDepthMip4;
    };

    auto gtaoPrefilterDepthDepthPass = pRenderGraph->AddPass<PrefilterDepthPassData>("GTAO prefilter depth pass", RenderPassType::Compute,
        [&](PrefilterDepthPassData& data, RGBuilder& builder)
        {
            data.m_inputDepth = builder.Read(depthRT);

            RGTexture::Desc desc;
            desc.m_width = width;
            desc.m_height = height;
            desc.m_mipLevels = 5; //< mip 0 ~ 4
            desc.m_format = RHIFormat::R16F;
            RGHandle gtaoDepth = builder.Create<RGTexture>(desc, "GTAO depth");

            data.m_outputDepthMip0 = builder.Write(gtaoDepth, 0);
            data.m_outputDepthMip1 = builder.Write(gtaoDepth, 1);
            data.m_outputDepthMip2 = builder.Write(gtaoDepth, 2);
            data.m_outputDepthMip3 = builder.Write(gtaoDepth, 3);
            data.m_outputDepthMip4 = builder.Write(gtaoDepth, 4);
        },
        [=](const PrefilterDepthPassData& data, IRHICommandList* pCommandList)
        {
            FilterDepth(pCommandList, pRenderGraph->GetTexture(data.m_inputDepth), pRenderGraph->GetTexture(data.m_outputDepthMip0),
                width, height);
        });
    

    // GTAO main
    struct GTAOPassData
    {
        RGHandle m_inputPrefilteredDepth;
        RGHandle m_inputNormal;
        RGHandle m_outputAO;
        RGHandle m_outputEdge;
    };

    auto gtaoPass = pRenderGraph->AddPass<GTAOPassData>("GTAO Main", RenderPassType::Compute,
        [&](GTAOPassData& data, RGBuilder& builder)
        {
            // Have to tell graph which mip need transition
            data.m_inputPrefilteredDepth = builder.Read(gtaoPrefilterDepthDepthPass->m_outputDepthMip0, 0);
            data.m_inputPrefilteredDepth = builder.Read(gtaoPrefilterDepthDepthPass->m_outputDepthMip1, 1);
            data.m_inputPrefilteredDepth = builder.Read(gtaoPrefilterDepthDepthPass->m_outputDepthMip2, 2);
            data.m_inputPrefilteredDepth = builder.Read(gtaoPrefilterDepthDepthPass->m_outputDepthMip3, 3);
            data.m_inputPrefilteredDepth = builder.Read(gtaoPrefilterDepthDepthPass->m_outputDepthMip4, 4);

            data.m_inputNormal = builder.Read(normalRT);

            RGTexture::Desc desc;
            desc.m_width = width;
            desc.m_height = height;
            desc.m_format = m_bEnableGTSO ? RHIFormat::R32UI : RHIFormat::R8UI;
            data.m_outputAO = builder.Create<RGTexture>(desc, "GTAO AO Term");
            data.m_outputAO = builder.Write(data.m_outputAO);

            desc.m_format = RHIFormat::R8UNORM;
            data.m_outputEdge = builder.Create<RGTexture>(desc, "GTAO Edge");
            data.m_outputEdge = builder.Write(data.m_outputEdge);
        },
        [=](const GTAOPassData& data, IRHICommandList* pCommandList)
        {
            RenderAO(pCommandList,
                pRenderGraph->GetTexture(data.m_inputPrefilteredDepth),
                pRenderGraph->GetTexture(data.m_inputNormal),
                pRenderGraph->GetTexture(data.m_outputAO),
                pRenderGraph->GetTexture(data.m_outputEdge),
                width, height);
        });

    struct DenoisePassData
    {
        RGHandle m_inputAO;
        RGHandle m_inputEdge;
        RGHandle m_outputAO;
    };

    auto gtaoDenoisePass = pRenderGraph->AddPass<DenoisePassData>("GTAO Denoise Pass", RenderPassType::Compute,
        [&](DenoisePassData& data, RGBuilder& builder)
        {
            data.m_inputAO = builder.Read(gtaoPass->m_outputAO);
            data.m_inputEdge = builder.Read(gtaoPass->m_outputEdge);

            RGTexture::Desc desc;
            desc.m_width = width;
            desc.m_height = height;
            desc.m_format = m_bEnableGTSO ? RHIFormat::R32UI : RHIFormat::R8UI;
            data.m_outputAO = builder.Create<RGTexture>(desc, "GTAO Denoised AO");
            data.m_outputAO = builder.Write(data.m_outputAO);
        },
        [=](const DenoisePassData& data, IRHICommandList* pCommandList)
        {
            Denoise(pCommandList,
                pRenderGraph->GetTexture(data.m_inputAO),
                pRenderGraph->GetTexture(data.m_inputEdge),
                pRenderGraph->GetTexture(data.m_outputAO),
                width, height);
        });
    return gtaoDenoisePass->m_outputAO;
}


void GTAO::CreateHilbertLUT()
{
    uint16_t data[64 * 64];

    for (int y = 0; y < 64; ++ y)
    {
        for (int x = 0; x < 64; ++ x)
        {
            uint32_t r2Index = XeGTAO::HilbertIndex(x, y);
            MY_ASSERT(r2Index < 65536);
            data[x + 64 * y] = (uint16_t) r2Index;
        }
    }

    m_pHilbertLUT.reset(m_pRenderer->CreateTexture2D(64, 64, 1, RHIFormat::R16UI, 0, "GTAO::m_pHilbertLUT"));
    m_pRenderer->UploadTexture(m_pHilbertLUT->GetTexture(), data);
}

void GTAO::UpdateGTAOConstants(IRHICommandList* pCommandList, uint32_t width, uint32_t height)
{
    Camera* pCamera = Engine::GetInstance()->GetWorld()->GetCamera();
    float4x4 projection = pCamera->GetProjectionMatrix();

    XeGTAO::GTAOSettings settings;
    settings.QualityLevel = m_qualityLevel;
    settings.Radius = m_radius;

    XeGTAO::GTAOConstants consts;
    XeGTAO::GTAOUpdateConstants(consts, width, height, settings, &projection.x.x, true, m_pRenderer->GetFrameID() % 64);
    pCommandList->SetComputeConstants(1, &consts, sizeof(consts));
}

void GTAO::FilterDepth(IRHICommandList* pCommandList, RGTexture* pDepthTexture, RGTexture* pHZB, uint32_t width, uint32_t height)
{
    UpdateGTAOConstants(pCommandList, width, height);

    pCommandList->SetPipelineState(m_pPrefilterDepthPSO);

    struct CB
    {
        uint m_srcRawDepth;
        uint m_outWorkingDepthMIP0;
        uint m_outWorkingDepthMIP1;
        uint m_outWorkingDepthMIP2;

        uint m_outWorkingDepthMIP3;
        uint m_outWorkingDepthMIP4;
        uint2 m_padding;
    };

    CB cb;
    cb.m_srcRawDepth = pDepthTexture->GetSRV()->GetHeapIndex();
    cb.m_outWorkingDepthMIP0 = pHZB->GetUAV(0, 0)->GetHeapIndex();
    cb.m_outWorkingDepthMIP1 = pHZB->GetUAV(1, 0)->GetHeapIndex();
    cb.m_outWorkingDepthMIP2 = pHZB->GetUAV(2, 0)->GetHeapIndex();
    cb.m_outWorkingDepthMIP3 = pHZB->GetUAV(3, 0)->GetHeapIndex();
    cb.m_outWorkingDepthMIP4 = pHZB->GetUAV(4, 0)->GetHeapIndex();

    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));
    pCommandList->Dispatch(DivideRoundingUp(width, 16), DivideRoundingUp(height, 16), 1);
}

void GTAO::RenderAO(IRHICommandList* pCommandList, RGTexture* pHZB, RGTexture* pNormal, RGTexture* pOutputAO, RGTexture* pOutputEdge, uint32_t width, uint32_t height)
{
    switch (m_qualityLevel)
    {
    case 0:
        pCommandList->SetPipelineState(m_bEnableGTSO ? m_pGTAOSOLowPSO : m_pGTAOLowPSO);
        break;
    case 1:
        pCommandList->SetPipelineState(m_bEnableGTSO ? m_pGTAOSOMediumPSO : m_pGTAOMediumPSO);
        break;
    case 2:
        pCommandList->SetPipelineState(m_bEnableGTSO ? m_pGTAOSOHighPSO : m_pGTAOHighPSO);
        break;
    case 3:
        pCommandList->SetPipelineState(m_bEnableGTSO ? m_pGTAOSOUltraPSO : m_pGTAOSOUltraPSO);
        break;
    default:
        MY_ASSERT(0);
        break;
    }

    struct CB
    {
        uint m_srcWorkingDepth;
        uint m_normalRT;
        uint m_hilbertLUT;
        uint m_outWorkingAOTerm;
        uint m_outWorkingEdges;
    };

    CB cb;
    cb.m_srcWorkingDepth = pHZB->GetSRV()->GetHeapIndex();
    cb.m_normalRT = pNormal->GetSRV()->GetHeapIndex();
    cb.m_hilbertLUT = m_pHilbertLUT->GetSRV()->GetHeapIndex();
    cb.m_outWorkingAOTerm = pOutputAO->GetUAV()->GetHeapIndex();
    cb.m_outWorkingEdges = pOutputEdge->GetUAV()->GetHeapIndex();

    pCommandList->SetComputeConstants(0, &cb, sizeof(cb));
    pCommandList->Dispatch(DivideRoundingUp(width, XE_GTAO_NUMTHREADS_X), DivideRoundingUp(height, XE_GTAO_NUMTHREADS_Y), 1);
}
void GTAO::Denoise(IRHICommandList* pCommandList, RGTexture* pInputAO, RGTexture* pEdge, RGTexture* pOutputAO, uint32_t width, uint32_t height)
{
    pCommandList->SetPipelineState(m_bEnableGTSO ? m_pSODenoisePSO : m_pDenoisePSO);

    uint32_t cb[3]
    {
        pInputAO->GetSRV()->GetHeapIndex(),
        pEdge->GetSRV()->GetHeapIndex(),
        pOutputAO->GetUAV()->GetHeapIndex()
    };

    pCommandList->SetComputeConstants(0, cb, sizeof(cb));
    pCommandList->Dispatch(DivideRoundingUp(width, XE_GTAO_NUMTHREADS_X * 2), DivideRoundingUp(height, XE_GTAO_NUMTHREADS_Y), 1); //< look at shader, it handle 2 horizon pixel 
}



