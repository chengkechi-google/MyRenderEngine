#pragma once
#include "RHI/RHI.h"
#include "Utils/math.h"
#include "Utils/linear_allocator.h"

#define MAX_RENDER_BATCH_CB_COUNT RHI_MAX_CBV_BINDINGS

struct RenderBatch
{
public:
    RenderBatch(LinearAllocator& cbAllocator) : m_allocator(cbAllocator)
    {
        m_pIB = nullptr;
    }

    void SetPipelineState(IRHIPipelineState* pPSO)
    {
        m_pPSO = pPSO;
    }

    void SetConstantBuffer(uint32_t slot, const void* pData, size_t dataSize)
    {
        MY_ASSERT(slot < MAX_RENDER_BATCH_CB_COUNT);
        
        if (m_cb[slot].m_pData == nullptr || m_cb[slot].m_dataSize < dataSize)
        {
            m_cb[slot].m_pData = m_allocator.Alloc((uint32_t) dataSize);
        }

        m_cb[slot].m_dataSize = (uint32_t) dataSize;
        memcpy(m_cb[slot].m_pData, pData, dataSize);
    }

    void SetIndexBuffer(IRHIBuffer* pBuffer, uint32_t offset, RHIFormat format)
    {
        m_pIB = pBuffer;
        m_ibOffset = offset;
        m_ibFormat = format;
    }

    void DrawIndexed(uint32_t count)
    {
        m_indexCount = count;
    }

    void Draw(uint32_t count)
    {
        m_vertexCount = count;
    }

    void DispatchMesh(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        m_dispatchX = groupCountX;
        m_dispatchY = groupCountY;
        m_dispatchZ = groupCountZ;
    }

public:
    const char* m_label = "";
    IRHIPipelineState* m_pPSO = nullptr;

    struct
    {
        void* m_pData = nullptr;
        uint32_t m_dataSize = 0;
    } m_cb[MAX_RENDER_BATCH_CB_COUNT];

    union
    {
        struct
        {
            IRHIBuffer* m_pIB;
            RHIFormat m_ibFormat;
            uint32_t m_ibOffset;
            uint32_t m_indexCount;
        };

        struct
        {
            uint32_t m_dispatchX;
            uint32_t m_dispatchY;
            uint32_t m_dispatchZ;
        };
    };

    float3 m_center;    //< world space
    float m_radius = 0.0;
    uint32_t m_meshletCount = 0;
    uint32_t m_instaceIndex = 0;
    uint32_t m_vertexCount = 0;

private:
    LinearAllocator& m_allocator;
};

inline void DrawBatch(IRHICommandList* pCommandList, const RenderBatch& batch)
{
    GPU_EVENT_DEBUG(pCommandList, batch.m_label);

    pCommandList->SetPipelineState(batch.m_pPSO);
    
    for (int i = 0; i < MAX_RENDER_BATCH_CB_COUNT; ++i)
    {
        if (batch.m_cb[i].m_pData != nullptr)
        {
            pCommandList->SetGraphicsConstants(i, batch.m_cb[i].m_pData, batch.m_cb[i].m_dataSize);
        }
    }

    if (batch.m_pPSO->GetType() == RHIPipelineType::MeshShading)
    {
        pCommandList->DispatchMesh(batch.m_dispatchX, batch.m_dispatchY, batch.m_dispatchZ);
    }
    else if (batch.m_pIB != nullptr)
    {
        pCommandList->SetIndexBuffer(batch.m_pIB, batch.m_ibOffset, batch.m_ibFormat);
        pCommandList->DrawIndexed(batch.m_indexCount);
    }
    else
    {
        pCommandList->Draw(batch.m_vertexCount);
    }
}

// todo: maybe just call batch that with compute and graphics information
struct ComputeBatch
{
public:
    ComputeBatch(LinearAllocator& cbAllocator) : m_allocator(cbAllocator)
    {

    }

    void SetPipelineState(IRHIPipelineState* pPSO)
    {
        m_pPSO = pPSO;
    }

    void SetConstantBuffer(uint32_t slot, const void* pData, size_t dataSize)
    {
        MY_ASSERT(slot < MAX_RENDER_BATCH_CB_COUNT);
        
        if (m_cb[slot].m_pData == nullptr || m_cb[slot].m_dataSize < dataSize)
        {
            m_cb[slot].m_pData = m_allocator.Alloc((uint32_t) dataSize);
        }

        m_cb[slot].m_dataSize = (uint32_t) dataSize;
        memcpy(m_cb[slot].m_pData, pData, dataSize);
    }

    void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        m_dispatchX = groupCountX;
        m_dispatchY = groupCountY;
        m_dispatchZ = groupCountZ;
    }
public:
    const char* m_label = "";
    IRHIPipelineState* m_pPSO = nullptr;

    struct
    {
        void* m_pData = nullptr;
        uint32_t m_dataSize = 0;
    } m_cb[MAX_RENDER_BATCH_CB_COUNT];

    uint32_t m_dispatchX = 0;
    uint32_t m_dispatchY = 0;
    uint32_t m_dispatchZ = 0;
private:
    LinearAllocator& m_allocator;
};

inline void DispatchBatch(IRHICommandList* pCommandList, const ComputeBatch& batch)
{
    GPU_EVENT_DEBUG(pCommandList, batch.m_label);

    pCommandList->SetPipelineState(batch.m_pPSO);
    
    for (int i = 0; i < MAX_RENDER_BATCH_CB_COUNT; ++i)
    {
        if (batch.m_cb[i].m_pData != nullptr)
        {
            pCommandList->SetComputeConstants(i, batch.m_cb[i].m_pData, batch.m_cb[i].m_dataSize);
        }
    }

    pCommandList->Dispatch(batch.m_dispatchX, batch.m_dispatchY, batch.m_dispatchZ);
}