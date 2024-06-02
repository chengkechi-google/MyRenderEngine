#include "StructedBuffer.h"
#include "Core/Engine.h"
#include "../Renderer.h"

StructedBuffer::StructedBuffer(const eastl::string& name)
{
    m_name = name;
}

bool StructedBuffer::Create(uint32_t stride, uint32_t elementCount, RHIMemoryType memoryType, bool uav)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IRHIDevice* pDevice = pRenderer->GetDevice();

    RHIBufferDesc desc;
    desc.m_stride = stride;
    desc.m_size = stride * elementCount;
    desc.m_format = RHIFormat::Unknown;
    desc.m_memoryType = memoryType;
    desc.m_usage = RHIBufferUsageBit::RHIBufferUsageStructedBuffer;

    if (uav)
    {
        desc.m_usage |= RHIBufferUsageBit::RHIBufferUsageUnorderedAccess;
    }

    m_pBuffer.reset(pDevice->CreateBuffer(desc, m_name));
    if (m_pBuffer == nullptr)
    {
        return false;
    }

    RHIShaderResourceViewDesc srvDesc;
    srvDesc.m_type = RHIShaderResourceViewType::StructuredBuffer;
    srvDesc.m_buffer.m_size = stride * elementCount;
    m_pSRV.reset(pDevice->CreateShaderResourceView(m_pBuffer.get(), srvDesc, m_name));
    if (m_pSRV == nullptr)
    {
        return false;
    }

    if (uav)
    {
        RHIUnorderedAccessViewDesc uavDesc;
        uavDesc.m_type = RHIUnorderedAccessViewType::StructuredBuffer;
        uavDesc.m_buffer.m_size = stride * elementCount;
        m_pUAV.reset(pDevice->CreateUnorderedAccessView(m_pBuffer.get(), uavDesc, m_name));
        if (m_pUAV == nullptr)
        {
            return false;
        }
    }
    
    return true;
}