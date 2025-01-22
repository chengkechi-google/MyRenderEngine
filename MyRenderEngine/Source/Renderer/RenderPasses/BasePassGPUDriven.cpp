#include "BasePassGPUDriven.h"
#include "Renderer/Renderer.h"
#include "HierarchicalDepthBufferPass.h"
#include "Utils/profiler.h"
#include "EASTL/map.h"

struct FirstPhaseInstanceCullingData
{
    RGHandle m_objectListBuffer;
    RGHandle m_visibleObjectListBuffer;
    RGHandle m_culledObjectListBuffer;
};

struct SecondPhaseInstanceCullingData
{
    RGHandle m_objectListBuffer;
    RGHandle m_visibleObjectListBuffer;
};

struct BasePassData
{
    RGHandle m_indirectCommandBuffer;
    
    RGHandle m_inHZB;
    RGHandle m_meshletListBuffer;
    RGHandle m_meshletListCounterBuffer;
    RGHandle m_occlusionCulledMeshletsBuffer;           //< Only use in first phase
    RGHandle m_occlusionCulledMeshletsCounterBuffer;    //< Only ise in first phase

    RGHandle m_outDiffuseRT;    //< SRGB : diffuse(xyz) + ao(a)
    RGHandle m_outSpecularRT;   //< SRGB : specular(xyz) + shading model(a)
    RGHandle m_outNormalRT;     //< rgba8norm : normal(xyz) + roughness(a)
    RGHandle m_outEmissiveRT;   //< r11g11b10 : emissive
    RGHandle m_outCustomRT;     //< rgba8norm : custom data
    RGHandle m_outDepthRT;  
};

static inline uint32_t roundup(uint32_t a, uint32_t b)
{
    return (a / b + 1) * b;
}

BasePassGPUDriven::BasePassGPUDriven(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    RHIComputePipelineDesc desc;
    desc.m_pCS = pRenderer->GetShader("InstanceCulling.hlsl", "InstanceCulling", RHIShaderType::CS, {"FIRST_PHASE=1"});
    m_p1stPhaseInstanceCullingPSO = pRenderer->GetPipelineState(desc, "1st phase instance culling PSO");

    desc.m_pCS = pRenderer->GetShader("InstanceCulling.hlsl", "InstanceCulling", RHIShaderType::CS);    //< Second phase
    m_p2ndPhaseInstanceCullingPSO = pRenderer->GetPipelineState(desc, "2nd phase instance culling PSO");

    desc.m_pCS = pRenderer->GetShader("InstanceCulling.hlsl", "BuildMeshletList", RHIShaderType::CS);
    m_pBuildMeshletListPSO = pRenderer->GetPipelineState(desc, "Build meshlet list PSO");

    desc.m_pCS = pRenderer->GetShader("InstanceCulling.hlsl", "BuildInstanceCullingCommand", RHIShaderType::CS);
    m_pBuildInstanceCullingCommandPSO = pRenderer->GetPipelineState(desc, "Build instance culling command PSO");

    desc.m_pCS = pRenderer->GetShader("InstanceCulling.hlsl", "BuildIndirectCommand", RHIShaderType::CS);
    m_pBuildIndirectCommandPSO = pRenderer->GetPipelineState(desc, "Build indirect command PSO");
}

RenderBatch& BasePassGPUDriven::AddBatch()
{
    LinearAllocator* pAllocator = m_pRenderer->GetConstantAllocator();
    return m_instances.emplace_back(*pAllocator);
}

void BasePassGPUDriven::Render1stPhase(RenderGraph* pRenderGraph)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "Base Pass 1st phase");

    // Merge is depending on the PSO
    MergeBatch();

    uint32_t maxDispatchNum = roundup((uint32_t) m_indirectBatches.size(), 65536 / sizeof(uint32_t));
    uint32_t maxInstanceNum = roundup(m_pRenderer->GetInstanceCount(), 65536 / sizeof(uint8_t));
    uint32_t maxMeshletsNum = roundup(m_totalMeshletCount, 65536/ sizeof(uint2));

    HZBPass* pHZBPass = m_pRenderer->GetHZBPass();

    struct ClearCounterPassData
    {
        RGHandle m_firstPhaseMeshletListCounterBuffer;
        RGHandle m_secondPhaseObjectListCounterBuffer;
        RGHandle m_secondPhaseMeshletListCounterBuffer;
    };

    auto clearCounterPass = pRenderGraph->AddPass<ClearCounterPassData>("Clear Counter", RenderPassType::Compute,
        [&](ClearCounterPassData& data, RGBuilder& builder)
        {
            RGBuffer::Desc bufferDesc;
            bufferDesc.m_stride = 4;
            bufferDesc.m_size = bufferDesc.m_stride * maxDispatchNum;
            bufferDesc.m_format = RHIFormat::R32UI;
            bufferDesc.m_usage = RHIBufferUsageBit::RHIBufferUsageTypedBuffer;

            data.m_firstPhaseMeshletListCounterBuffer = builder.Create<RGBuffer>(bufferDesc, "1st phase meshlet list counter");
            data.m_firstPhaseMeshletListCounterBuffer = builder.Write(data.m_firstPhaseMeshletListCounterBuffer);

            // TODO: Only 1 uint size
            data.m_secondPhaseObjectListCounterBuffer = builder.Create<RGBuffer>(bufferDesc, "2nd phae object list counter");
            data.m_secondPhaseObjectListCounterBuffer = builder.Write(data.m_secondPhaseObjectListCounterBuffer);
            
            data.m_secondPhaseMeshletListCounterBuffer = builder.Create<RGBuffer>(bufferDesc, "2nd phase meshlet list counter");
            data.m_secondPhaseMeshletListCounterBuffer = builder.Write(data.m_secondPhaseMeshletListCounterBuffer);
        },
        [=](const ClearCounterPassData& data, IRHICommandList* pCommandList)
        {
            ResetCounter(pCommandList,
                pRenderGraph->GetBuffer(data.m_firstPhaseMeshletListCounterBuffer),
                pRenderGraph->GetBuffer(data.m_secondPhaseObjectListCounterBuffer),
                pRenderGraph->GetBuffer(data.m_secondPhaseMeshletListCounterBuffer));
        });

    struct InstanceCullingData
    {
        RGHandle m_hzbTexture;
        RGHandle m_cullingResultBuffer;
        RGHandle m_secondPhaseObjectListBuffer;
        RGHandle m_secondPhaseObjectListCounterBuffer;
    };
    auto instanceCullingPass = pRenderGraph->AddPass<InstanceCullingData>("Instance Culling", RenderPassType::Compute,
        [&](InstanceCullingData& data, RGBuilder& builder)
        {
            RGBuffer::Desc bufferDesc;
            bufferDesc.m_stride = 1;
            bufferDesc.m_size = bufferDesc.m_stride * maxInstanceNum;
            bufferDesc.m_format = RHIFormat::R8UI;
            bufferDesc.m_usage = RHIBufferUsageBit::RHIBufferUsageTypedBuffer;
            data.m_cullingResultBuffer = builder.Create<RGBuffer>(bufferDesc, "1st phase culling result");
            data.m_cullingResultBuffer = builder.Write(data.m_cullingResultBuffer);

            bufferDesc.m_stride = 4;
            bufferDesc.m_size = bufferDesc.m_stride * maxInstanceNum;
            bufferDesc.m_format = RHIFormat::R32UI;
            data.m_secondPhaseObjectListBuffer = builder.Create<RGBuffer>(bufferDesc, "2nd phase object list");
            data.m_secondPhaseObjectListBuffer = builder.Write(data.m_secondPhaseObjectListBuffer);

            data.m_secondPhaseObjectListCounterBuffer = builder.Write(clearCounterPass->m_secondPhaseObjectListCounterBuffer);

            for (uint32_t i = 0; i < pHZBPass->GetHZBMipCount(); ++i)
            {
                data.m_hzbTexture = builder.Read(pHZBPass->Get1stPhaseCullingHZBMip(i), i);
            }
        },
        [=](const InstanceCullingData& data, IRHICommandList* pCommandList)
        {
            InstanceCulling1stPhase(pCommandList,
                pRenderGraph->GetBuffer(data.m_cullingResultBuffer),
                pRenderGraph->GetBuffer(data.m_secondPhaseObjectListBuffer),
                pRenderGraph->GetBuffer(data.m_secondPhaseObjectListCounterBuffer));
        });
    
    struct BuildMeshletListData
    {
        RGHandle m_cullingResultBuffer;
        RGHandle m_meshletListBuffer;
        RGHandle m_meshletListCounterBuffer;
    };

    auto buildMeshletListPass = pRenderGraph->AddPass<BuildMeshletListData>("Build Meshlet List", RenderPassType::Compute,
        [&](BuildMeshletListData& data, RGBuilder& builder)
        {
            RGBuffer::Desc bufferDesc;
            bufferDesc.m_stride = sizeof(uint2);
            bufferDesc.m_size = bufferDesc.m_stride * maxMeshletsNum;
            bufferDesc.m_usage = RHIBufferUsageBit::RHIBufferUsageStructedBuffer;
            data.m_meshletListBuffer = builder.Create<RGBuffer>(bufferDesc, "1st phase meshlet list");

            data.m_cullingResultBuffer = builder.Read(instanceCullingPass->m_cullingResultBuffer);
            data.m_meshletListBuffer = builder.Write(data.m_meshletListBuffer);
            data.m_meshletListCounterBuffer = builder.Write(clearCounterPass->m_firstPhaseMeshletListCounterBuffer);
        },
        [=](const BuildMeshletListData& data, IRHICommandList* pCommandList)
        {
            BuildMeshletList(pCommandList,
                pRenderGraph->GetBuffer(data.m_cullingResultBuffer),
                pRenderGraph->GetBuffer(data.m_meshletListBuffer),
                pRenderGraph->GetBuffer(data.m_meshletListCounterBuffer));
        });

    struct BuildIndirectCommandPassData
    {
        RGHandle m_meshletListCounterBuffer;
        RGHandle m_indirectCommandBuffer;
    };

    auto buildIndirectCommandPass = pRenderGraph->AddPass<BuildIndirectCommandPassData>("Build Indirect Command", RenderPassType::Compute,
        [&](BuildIndirectCommandPassData& data, RGBuilder& builder)
        {
            RGBuffer::Desc bufferDesc;
            bufferDesc.m_stride = sizeof(uint3);
            bufferDesc.m_size = bufferDesc.m_stride * maxDispatchNum;
            bufferDesc.m_usage = RHIBufferUsageBit::RHIBufferUsageStructedBuffer;
            data.m_indirectCommandBuffer = builder.Create<RGBuffer>(bufferDesc, "1st Phase Indirect Command Buffer");
            data.m_indirectCommandBuffer = builder.Write(data.m_indirectCommandBuffer);
            
            data.m_meshletListCounterBuffer = builder.Read(buildMeshletListPass->m_meshletListCounterBuffer);
        },
        [=](const BuildIndirectCommandPassData& data, IRHICommandList* pCommandList)
        {
            BuildIndirectCommand(pCommandList, 
                pRenderGraph->GetBuffer(data.m_meshletListCounterBuffer),
                pRenderGraph->GetBuffer(data.m_indirectCommandBuffer));
        });

    auto gBufferPass = pRenderGraph->AddPass<BasePassData>("Base Pass", RenderPassType::Graphics,
        [&](BasePassData& data, RGBuilder& builder)
        {
            RGTexture::Desc desc;
            desc.m_width = m_pRenderer->GetRenderWidth();
            desc.m_height = m_pRenderer->GetRenderHeight();
            desc.m_format = RHIFormat::RGBA8SRGB;
            data.m_outDiffuseRT = builder.Create<RGTexture>(desc, "Diffuse RT");
            data.m_outSpecularRT = builder.Create<RGTexture>(desc, "Specular RT");
            
            desc.m_format = RHIFormat::RGBA8UNORM;
            data.m_outNormalRT = builder.Create<RGTexture>(desc, "Normal RT");
            data.m_outCustomRT = builder.Create<RGTexture>(desc, "CustomData RT");
            
            desc.m_format = RHIFormat::R11G11B10F;
            data.m_outEmissiveRT = builder.Create<RGTexture>(desc, "Emissive RT");

            desc.m_format = RHIFormat::D32F;
            data.m_outDepthRT = builder.Create<RGTexture>(desc, "Scene Depth RT");

            data.m_outDiffuseRT = builder.WriteColor(0, data.m_outDiffuseRT, 0, RHIRenderPassLoadOp::Clear, float4(0.0f));
            data.m_outSpecularRT = builder.WriteColor(1, data.m_outSpecularRT, 0, RHIRenderPassLoadOp::Clear, float4(0.0f));
            data.m_outNormalRT = builder.WriteColor(2, data.m_outNormalRT, 0, RHIRenderPassLoadOp::Clear, float4(0.0f));
            data.m_outEmissiveRT = builder.WriteColor(3, data.m_outEmissiveRT, 0, RHIRenderPassLoadOp::Clear, float4(0.0f));
            data.m_outCustomRT = builder.WriteColor(4, data.m_outCustomRT, 0, RHIRenderPassLoadOp::Clear, float4(0.0));
            data.m_outDepthRT = builder.WriteDepth(data.m_outDepthRT, 0, RHIRenderPassLoadOp::Clear, RHIRenderPassLoadOp::Clear);

            
            for (uint32_t i = 0; i < pHZBPass->GetHZBMipCount(); ++i)
            {
                data.m_inHZB = builder.Read(pHZBPass->Get1stPhaseCullingHZBMip(i), i, RGBuilderFlag::ShaderStageNonPS);
            }

            data.m_indirectCommandBuffer = builder.ReadIndirectArg(buildIndirectCommandPass->m_indirectCommandBuffer);
            data.m_meshletListBuffer = builder.Read(buildMeshletListPass->m_meshletListBuffer, 0, RGBuilderFlag::ShaderStageNonPS);
            data.m_meshletListCounterBuffer = builder.Read(buildMeshletListPass->m_meshletListCounterBuffer, 0, RGBuilderFlag::ShaderStageNonPS);

            RGBuffer::Desc bufferDesc;
            bufferDesc.m_stride = sizeof(uint2);
            bufferDesc.m_size = bufferDesc.m_stride * maxMeshletsNum;
            bufferDesc.m_usage = RHIBufferUsageBit::RHIBufferUsageStructedBuffer;
            data.m_occlusionCulledMeshletsBuffer = builder.Create<RGBuffer>(bufferDesc, "2nd phase meshlet list");
            data.m_occlusionCulledMeshletsBuffer = builder.Write(data.m_occlusionCulledMeshletsBuffer);
            data.m_occlusionCulledMeshletsCounterBuffer = builder.Write(clearCounterPass->m_secondPhaseMeshletListCounterBuffer);
        },
        [=](const BasePassData& data, IRHICommandList* pCommandList)
        {
            Flush1stPhaseBatches(pCommandList,
                pRenderGraph->GetBuffer(data.m_indirectCommandBuffer),
                pRenderGraph->GetBuffer(data.m_meshletListBuffer),
                pRenderGraph->GetBuffer(data.m_meshletListCounterBuffer));
        });

    struct ShowCulledInstancePassData
    {
        RGHandle m_outCulledDiffuseRT;
        RGHandle m_outCulledDepthRT;
        RGHandle m_cullingResultBuffer;
    };

    auto ShowCulledInstancePass = pRenderGraph->AddPass<ShowCulledInstancePassData>("ShowCulledInstancePass", RenderPassType::Graphics,
        [&](ShowCulledInstancePassData& data, RGBuilder& builder)
        {
            RGTexture::Desc desc;
            desc.m_width = m_pRenderer->GetRenderWidth();
            desc.m_height = m_pRenderer->GetRenderHeight();
            desc.m_format = RHIFormat::RGBA8SRGB;
            data.m_outCulledDiffuseRT = builder.Create<RGTexture>(desc, "Culled Object Diffuse RT");

            desc.m_format = RHIFormat::D32F;
            data.m_outCulledDepthRT = builder.Create<RGTexture>(desc, "Culled Object Depth RT");

            data.m_cullingResultBuffer = builder.Read(instanceCullingPass->m_cullingResultBuffer);
            data.m_outCulledDiffuseRT = builder.WriteColor(0, data.m_outCulledDiffuseRT, 0, RHIRenderPassLoadOp::Clear, float4(0.0f));
            data.m_outCulledDepthRT = builder.WriteDepth(data.m_outCulledDepthRT,0, RHIRenderPassLoadOp::Clear, RHIRenderPassLoadOp::Clear);
            builder.SkipCulling();
        },
        [&instances = m_instances, pRenderGraph](const ShowCulledInstancePassData& data, IRHICommandList* pCommandList)
        {
            for (size_t i = 0; i < instances.size(); ++ i)
            {
                pCommandList->SetPipelineState(instances[i].m_pCustomPSO);
                uint32_t rootConstants[3] = {instances[i].m_instaceIndex, (uint32_t) instances.size(), pRenderGraph->GetBuffer(data.m_cullingResultBuffer)->GetSRV()->GetHeapIndex()};
                pCommandList->SetGraphicsConstants(0, rootConstants, sizeof(rootConstants));
                pCommandList->DispatchMesh(1, 1, 1);
            }
            
            instances.clear();
        });

    //auto showCulledMeshletPass = pRenderGraph->AddPass<ShowCulledMeshletPassData>("Show Culled Meshlet Pass", RenderPassType::Graphics, )
    //m_instances.clear();
    
    m_diffuseRT = gBufferPass->m_outDiffuseRT;
    m_specularRT = gBufferPass->m_outSpecularRT;
    m_normalRT = gBufferPass->m_outNormalRT;
    m_emissiveRT = gBufferPass->m_outEmissiveRT;
    m_customDataRT = gBufferPass->m_outCustomRT;
    m_depthRT = gBufferPass->m_outDepthRT;

    m_2ndPhaseObjectListBuffer = instanceCullingPass->m_secondPhaseObjectListBuffer;
    m_2ndPhaseObjectListCounterBuffer = instanceCullingPass->m_secondPhaseObjectListCounterBuffer;

    m_2ndPhaseMeshletListBuffer = gBufferPass->m_occlusionCulledMeshletsBuffer;
    m_2ndPhaseMeshletListCounterBuffer = gBufferPass->m_occlusionCulledMeshletsCounterBuffer;

    m_culledObjectsDiffuseRT = ShowCulledInstancePass->m_outCulledDiffuseRT;
    m_culledObjectsDepthRT = ShowCulledInstancePass->m_outCulledDepthRT;
}

void BasePassGPUDriven::Render2ndPhase(RenderGraph* pRenderGraph)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "Base Pass 2nd phase");

    HZBPass* pHZBPass = m_pRenderer->GetHZBPass();

    uint32_t maxDispatchNum = roundup((uint32_t)m_indirectBatches.size(), 65536 / sizeof(uint32_t));
    uint32_t maxInstanceNum = roundup(m_pRenderer->GetInstanceCount(), 65536 / sizeof(uint8_t));
}

void BasePassGPUDriven::MergeBatch()
{
    m_totalInstanceCount = (uint32_t) m_instances.size();

    eastl::vector<uint32_t> instanceIndices(m_totalInstanceCount);
    for (uint32_t i = 0; i < m_totalInstanceCount; ++i)
    {
        instanceIndices[i] = m_instances[i].m_instaceIndex;
    }
    m_instanceIndexAddress = m_pRenderer->AllocateSceneConstant(instanceIndices.data(), sizeof(uint32_t) * m_totalInstanceCount);

    m_totalMeshletCount = 0;
    m_indirectBatches.clear();
    m_nonGPUDrivenBatches.clear();

    struct MergedBatch
    {
        eastl::vector<RenderBatch> m_batches;
        uint32_t m_meshletCount;
    };
    eastl::map<IRHIPipelineState*, MergedBatch> mergedBatches;

    for (size_t i = 0; i < m_instances.size(); ++i)
    {
        const RenderBatch& batch = m_instances[i];
        if (batch.m_pPSO->GetType() == RHIPipelineType::MeshShading)
        {
            m_totalMeshletCount += batch.m_meshletCount;

            auto iter = mergedBatches.find(batch.m_pPSO);
            if (iter != mergedBatches.end())
            {
                iter->second.m_meshletCount += batch.m_meshletCount;
                iter->second.m_batches.push_back(batch);                //< Copy happen
            }
            else
            {
                MergedBatch mergedBatch;
                mergedBatch.m_batches.push_back(batch);
                mergedBatch.m_meshletCount = batch.m_meshletCount;
                mergedBatches.insert(eastl::make_pair(batch.m_pPSO, mergedBatch));
            }
        }
        else
        {
            m_nonGPUDrivenBatches.push_back(batch);
        }
    }

    uint32_t meshletListOffset = 0;
    for (auto iter = mergedBatches.begin(); iter != mergedBatches.end(); ++iter) //< iterate efvery different PSO
    {
        const MergedBatch& batch = iter->second;
        
        eastl::vector<uint2> meshletList;
        meshletList.reserve(batch.m_meshletCount);

        for (size_t i = 0; i < batch.m_batches.size(); ++i)
        {
            uint32_t instanceIndex = batch.m_batches[i].m_instaceIndex;
            for (size_t m = 0; m < batch.m_batches[i].m_meshletCount; ++m)
            {
                meshletList.emplace_back(instanceIndex, (uint32_t) m);
            }
        }

        uint32_t meshletListAddress = m_pRenderer->AllocateSceneConstant(meshletList.data(), sizeof(uint2) * (uint32_t) meshletList.size());
        m_indirectBatches.push_back({iter->first, meshletListAddress, batch.m_meshletCount, meshletListOffset});

        meshletListOffset += batch.m_meshletCount;
    }

    //m_instances.clear();
}

void BasePassGPUDriven::ResetCounter(IRHICommandList* pCommandList, RGBuffer* pFirstPhaseMeshletCounter, RGBuffer* pSecondPhaseObjectCounter, RGBuffer* pSecondPhaseMeshletCounter)
{
    uint32_t clearValue[4] = {0, 0, 0, 0};
    pCommandList->ClearUAV(pFirstPhaseMeshletCounter->GetBuffer(), pFirstPhaseMeshletCounter->GetUAV(), clearValue);
    pCommandList->ClearUAV(pSecondPhaseObjectCounter->GetBuffer(), pSecondPhaseObjectCounter->GetUAV(), clearValue);
    pCommandList->ClearUAV(pSecondPhaseMeshletCounter->GetBuffer(), pSecondPhaseMeshletCounter->GetUAV(), clearValue);

    pCommandList->BufferBarrier(pFirstPhaseMeshletCounter->GetBuffer(), RHIAccessBit::RHIAccessClearUAV, RHIAccessBit::RHIAccessComputeShaderUAV);
    pCommandList->BufferBarrier(pSecondPhaseObjectCounter->GetBuffer(), RHIAccessBit::RHIAccessClearUAV, RHIAccessBit::RHIAccessComputeShaderUAV);
    pCommandList->BufferBarrier(pSecondPhaseMeshletCounter->GetBuffer(), RHIAccessBit::RHIAccessClearUAV, RHIAccessBit::RHIAccessComputeShaderUAV);
}

void BasePassGPUDriven::InstanceCulling1stPhase(IRHICommandList* pCommandList, RGBuffer* pCullingResultUAV, RGBuffer* pSecondPhaseObjectListUAV, RGBuffer* pSecondPhaseIbjectListCounterUAV)
{
    uint32_t clearValue[4] = {0, 0, 0, 0};
    pCommandList->ClearUAV(pCullingResultUAV->GetBuffer(), pCullingResultUAV->GetUAV(), clearValue);
    pCommandList->BufferBarrier(pCullingResultUAV->GetBuffer(), RHIAccessBit::RHIAccessClearUAV, RHIAccessComputeShaderUAV);

    pCommandList->SetPipelineState(m_p1stPhaseInstanceCullingPSO);

    uint32_t instanceCount = m_totalInstanceCount;
    uint32_t rootConstants[5] = {
        m_instanceIndexAddress,
        instanceCount,
        pCullingResultUAV->GetUAV()->GetHeapIndex(),
        pSecondPhaseObjectListUAV->GetUAV()->GetHeapIndex(),
        pSecondPhaseIbjectListCounterUAV->GetUAV()->GetHeapIndex()
    };
    pCommandList->SetComputeConstants(0, rootConstants, sizeof(rootConstants));

    uint32_t groupCount = max((instanceCount + 63) / 64, 1u);
    pCommandList->Dispatch(groupCount, 1, 1);
}

void BasePassGPUDriven::InstanceCulling2ndPhase(IRHICommandList* pCommandList, RGBuffer* pIndirectDispatchCommandBuffer, 
    RGBuffer* pCullingResultUAV, RGBuffer* pSecondPhaseObjectListSRV, RGBuffer* pSecondPhaseObjectListCounterSRV)
{
    uint32_t clearValue[4] = {0, 0, 0, 0};
    pCommandList->ClearUAV(pCullingResultUAV->GetBuffer(), pCullingResultUAV->GetUAV(), clearValue);
    pCommandList->BufferBarrier(pCullingResultUAV->GetBuffer(), RHIAccessBit::RHIAccessClearUAV, RHIAccessBit::RHIAccessComputeShaderUAV);

    pCommandList->SetPipelineState(m_p2ndPhaseInstanceCullingPSO);

    uint32_t rootConstants[3] = {pSecondPhaseObjectListSRV->GetSRV()->GetHeapIndex(), pSecondPhaseObjectListCounterSRV->GetSRV()->GetHeapIndex(), pCullingResultUAV->GetUAV()->GetHeapIndex()};
    pCommandList->SetComputeConstants(0, rootConstants, sizeof(rootConstants));

    pCommandList->DispatchIndirect(pIndirectDispatchCommandBuffer->GetBuffer(), 0);
}

void BasePassGPUDriven::Flush1stPhaseBatches(IRHICommandList* pCommandList, RGBuffer* pIndirectCommandBuffer, RGBuffer* pMeshletListSRV, RGBuffer* pMeshletListCounterSRV)
{
    for (size_t i = 0; i < m_indirectBatches.size(); ++i)
    {
        const IndirectBatch& batch = m_indirectBatches[i];
        pCommandList->SetPipelineState(batch.m_pPSO);

        uint32_t rootConstants[5] = {
            pMeshletListSRV->GetSRV()->GetHeapIndex(),
            pMeshletListCounterSRV->GetSRV()->GetHeapIndex(),
            batch.m_meshletListBufferOffset,
            (uint32_t) i,
            1
        };
        pCommandList->SetGraphicsConstants(0, rootConstants, sizeof(rootConstants));

        pCommandList->DispatchMeshIndirect(pIndirectCommandBuffer->GetBuffer(), sizeof(uint3) * (uint32_t) i);
    }
}

void BasePassGPUDriven::Flush2ndPhaseBatches(IRHICommandList* pCommandList, RGBuffer* pIndirectCommandBuffer, RGBuffer* pMeshletListSRV, RGBuffer* pMeshletListCounterSRV)
{
    for (size_t i = 0; i < m_indirectBatches.size(); ++i)
    {
        const IndirectBatch& batch = m_indirectBatches[i];
        pCommandList->SetPipelineState(batch.m_pPSO);

        uint32_t rootConstants[5] = {
            pMeshletListSRV->GetSRV()->GetHeapIndex(),
            pMeshletListCounterSRV->GetSRV()->GetHeapIndex(),
            batch.m_meshletListBufferOffset,
            (uint32_t)i,
            1
        };
        pCommandList->SetGraphicsConstants(0, rootConstants, sizeof(rootConstants));

        pCommandList->DispatchMeshIndirect(pIndirectCommandBuffer->GetBuffer(), sizeof(uint3) * (uint32_t) i);
    }

    for (size_t i = 0; i < m_nonGPUDrivenBatches.size(); ++i)
    {
        DrawBatch(pCommandList, m_nonGPUDrivenBatches[i]);
    }
}

void BasePassGPUDriven::BuildMeshletList(IRHICommandList* pCommandList, RGBuffer* pCullingResultSRV, RGBuffer* pMeshletListBufferUAV, RGBuffer* pMeshletListCounterBufferUAV)
{
    pCommandList->SetPipelineState(m_pBuildMeshletListPSO);

    for (size_t i = 0; i < m_indirectBatches.size(); ++i)
    {
        uint32_t rootConstants[7] = {
            (uint32_t) i,
            pCullingResultSRV->GetSRV()->GetHeapIndex(),
            m_indirectBatches[i].m_originMeshletListAddress,
            m_indirectBatches[i].m_originMeshletCount,
            m_indirectBatches[i].m_meshletListBufferOffset,
            pMeshletListBufferUAV->GetUAV()->GetHeapIndex(),
            pMeshletListCounterBufferUAV->GetUAV()->GetHeapIndex()
        };

        pCommandList->SetComputeConstants(0, rootConstants, sizeof(rootConstants));
        pCommandList->Dispatch(DivideRoundingUp(m_indirectBatches[i].m_originMeshletCount, 64), 1, 1);
    }
}

void BasePassGPUDriven::BuildIndirectCommand(IRHICommandList* pCommandList, RGBuffer* pCounterBufferSRV, RGBuffer* pCommandBufferUAV)
{
    pCommandList->SetPipelineState(m_pBuildIndirectCommandPSO);

    uint32_t batchCount = (uint32_t) m_indirectBatches.size();
    uint32_t rootConstants[3] = {
        batchCount,
        pCounterBufferSRV->GetSRV()->GetHeapIndex(),
        pCommandBufferUAV->GetUAV()->GetHeapIndex()
    };

    pCommandList->SetComputeConstants(0, rootConstants, sizeof(rootConstants));
    
    uint32_t groupCount = max((batchCount + 63) / 64, 1u);
    pCommandList->Dispatch(groupCount, 1, 1);
}