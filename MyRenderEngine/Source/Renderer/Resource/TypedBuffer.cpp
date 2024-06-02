#include "TypedBuffer.h"
#include "Core/Engine.h"
#include "../Renderer.h"

TypedBuffer::TypedBuffer(const eastl::string& name)
{
    m_name = name;
}

bool TypedBuffer::Create(RHIFormat format, uint32_t elementCount, RHIMemoryType memoryType, bool uav)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IRHIDevice* pDevice = pRenderer->GetDevice();

    uint32_t stride = GetFormatRowPitch(format, 1);
    RHIBufferDesc desc;
    desc.m_stride = stride;
    desc.m_size = stride * elementCount;
    desc.m_format = format;
    desc.m_memoryType = memoryType;
    desc.m_usage = RHIBufferUsageBit::RHIBufferUsageTypedBuffer;
    
    if (uav)
    {
        desc.m_usage |= RHIBufferUsageBit::RHIBufferUsageUnorderedAccess;
    }

    RHIShaderResourceViewDesc srvDesc;
    srvDesc.m_type = RHIShaderResourceViewType::TypedBuffer;
    srvDesc.m_format = format;
    srvDesc.m_buffer.m_size = stride * elementCount;
    m_pSRV.reset(pDevice->CreateShaderResourceView(m_pBuffer.get(), srvDesc, m_name));
    if (m_pSRV == nullptr)
    {
        return false;
    }

    if (uav)
    {
        RHIUnorderedAccessViewDesc uavDesc;
        uavDesc.m_type = RHIUnorderedAccessViewType::TypedBuffer;
        uavDesc.m_format = format;
        uavDesc.m_buffer.m_size = stride * elementCount;
        m_pUAV.reset(pDevice->CreateUnorderedAccessView(m_pBuffer.get(), uavDesc, m_name));
        if (m_pUAV == nullptr)
        {
            return false;
        }
    }

    return true;
}