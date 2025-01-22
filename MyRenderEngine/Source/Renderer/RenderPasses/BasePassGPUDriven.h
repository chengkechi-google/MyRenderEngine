#pragma once

#include "../RenderBatch.h"
#include "../RenderGraph/RenderGraph.h"

class Renderer;

class BasePassGPUDriven
{
public:
    BasePassGPUDriven(Renderer* pRenderer);

    RenderBatch& AddBatch();
    void Render1stPhase(RenderGraph* pRenderGraph);
    void Render2ndPhase(RenderGraph* pRenderGraph);

    RGHandle GetDiffuseRT() const { return m_diffuseRT; }
    RGHandle GetSoecularRT() const { return m_specularRT; }
    RGHandle GetNormalRT() const { return m_normalRT; }
    RGHandle GetEmissiveRT() const { return m_emissiveRT; }
    RGHandle GetCustomDataRT() const { return m_customDataRT; }
    RGHandle GetDepthRT() const { return m_depthRT; }
    RGHandle GetCulledObjectsDiffuseRT() const { return m_culledObjectsDiffuseRT; }

    RGHandle GetSecondPhaseMeshletListBuffer() const { return m_2ndPhaseMeshletListBuffer; }
    RGHandle GetSecondPhaseMeshletListCounterBuffer() const { return m_2ndPhaseMeshletListCounterBuffer; }
    

private:
    void MergeBatch();

    void ResetCounter(IRHICommandList* pCommandList, RGBuffer* p1stPhaseMeshletCounter, RGBuffer* p2ndPhaseObjectCounter, RGBuffer* p2ndPhaseMeshletCounter);
    void InstanceCulling1stPhase(IRHICommandList* pCommandList, RGBuffer* pCullingResultUAV, RGBuffer* p2ndPhaseObjectListUAV, RGBuffer* p2ndPhaseObjectListCounterUAV);
    void InstanceCulling2ndPhase(IRHICommandList* pCommandList, RGBuffer* pIndirectCommandBuffer, RGBuffer* pCullingResultUAV, RGBuffer* pObjectListBufferSRV, RGBuffer* pObjectListCounterBufferUAV);

    void Flush1stPhaseBatches(IRHICommandList* pCommandList, RGBuffer* pIndirectCommandBuffer, RGBuffer* pMeshletListSRV, RGBuffer* pMeshletListCounterSRV);
    void Flush2ndPhaseBatches(IRHICommandList* pCommandList, RGBuffer* pIndirectCommandBuffer, RGBuffer* pMeshletListSRV, RGBuffer* pMeshletListCounterSRV);

    void BuildMeshletList(IRHICommandList* pCommandList, RGBuffer* pCullingResultSRV, RGBuffer* pMeshletBufferUAV, RGBuffer* pMeshListCounterBufferUAV);
    void BuildIndirectCommand(IRHICommandList* pCommandList, RGBuffer* pCounterBufferSRV, RGBuffer* pCommandBufferUAV);
private:
    Renderer* m_pRenderer;

    IRHIPipelineState* m_p1stPhaseInstanceCullingPSO = nullptr;
    IRHIPipelineState* m_p2ndPhaseInstanceCullingPSO = nullptr;

    IRHIPipelineState* m_pBuildMeshletListPSO = nullptr;
    IRHIPipelineState* m_pBuildInstanceCullingCommandPSO = nullptr;
    IRHIPipelineState* m_pBuildIndirectCommandPSO = nullptr;

    eastl::vector<RenderBatch> m_instances;

    struct IndirectBatch
    {
        IRHIPipelineState* m_pPSO;
        uint32_t m_originMeshletListAddress;
        uint32_t m_originMeshletCount;
        uint32_t m_meshletListBufferOffset;
    };
    eastl::vector<IndirectBatch> m_indirectBatches;

    eastl::vector<RenderBatch> m_nonGPUDrivenBatches;

    uint32_t m_totalInstanceCount = 0;
    uint32_t m_totalMeshletCount = 0;

    uint32_t m_instanceIndexAddress = 0;

    RGHandle m_diffuseRT;
    RGHandle m_specularRT;
    RGHandle m_normalRT;
    RGHandle m_emissiveRT;
    RGHandle m_customDataRT;
    RGHandle m_depthRT;

    RGHandle m_culledObjectsDiffuseRT;
    RGHandle m_culledObjectsDepthRT;

    RGHandle m_2ndPhaseObjectListBuffer;
    RGHandle m_2ndPhaseObjectListCounterBuffer;

    RGHandle m_2ndPhaseMeshletListBuffer;
    RGHandle m_2ndPhaseMeshletListCounterBuffer;
};