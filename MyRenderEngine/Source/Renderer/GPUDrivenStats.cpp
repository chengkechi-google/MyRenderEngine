#include "GPUDrivenStats.h"
#include "Renderer.h"
#include "Stats.hlsli"

GPUDrivenStats::GPUDrivenStats(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    RHIComputePipelineDesc desc;
    desc.m_pCS = pRenderer->GetShader("Stats.hlsl", "main", RHIShaderType::CS);
    m_pPSO = pRenderer->GetPipelineState(desc, "GPUDrivenStats:m_pPSO");

    m_pStatsBuffer.reset(pRenderer->CreateTypedBuffer(nullptr, RHIFormat::R32UI, STATS_MAX_TYPE_COUNT, "GPUDrivenStats::m_pStatsBuffer", RHIMemoryType::GPUOnly, true));
}

void GPUDrivenStats::Clear(IRHICommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "GPUDrivenStats clear");

    pCommandList->BufferBarrier(m_pStatsBuffer->GetBuffer(), RHIAccessBit::RHIAccessComputeShaderSRV, RHIAccessBit::RHIAccessClearUAV);

    uint32_t clearValue[4] = {0, 0, 0, 0};
    pCommandList->ClearUAV(m_pStatsBuffer->GetBuffer(), m_pStatsBuffer->GetUAV(), clearValue);
    pCommandList->BufferBarrier(m_pStatsBuffer->GetBuffer(), RHIAccessBit::RHIAccessClearUAV, RHIAccessBit::RHIAccessMaskUAV);
}

void GPUDrivenStats::Draw(IRHICommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "GPUDrivenStats");

    pCommandList->BufferBarrier(m_pStatsBuffer->GetBuffer(), RHIAccessBit::RHIAccessMaskUAV, RHIAccessBit::RHIAccessComputeShaderSRV);
    
    uint32_t rootConstants[1] = { m_pStatsBuffer->GetSRV()->GetHeapIndex() };

    pCommandList->SetPipelineState(m_pPSO);
    pCommandList->SetComputeConstants(0, rootConstants, sizeof(rootConstants));
    pCommandList->Dispatch(1, 1, 1);
    
}