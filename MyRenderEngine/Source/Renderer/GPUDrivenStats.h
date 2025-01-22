#pragma once
#include "Resource/TypedBuffer.h"

class Renderer;

class GPUDrivenStats
{
public:
    GPUDrivenStats(Renderer* pRenderer);

    void Clear(IRHICommandList* pCommandList);
    void Draw(IRHICommandList* pCommandList);

    IRHIDescriptor* GetStatsBufferUAV() const { return m_pStatsBuffer->GetUAV(); }
private:
    Renderer* m_pRenderer = nullptr;
    IRHIPipelineState* m_pPSO = nullptr;

    eastl::unique_ptr<TypedBuffer> m_pStatsBuffer;
};