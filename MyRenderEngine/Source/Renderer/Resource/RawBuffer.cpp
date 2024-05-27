#include "RawBuffer.h"
#include "Core/Engine.h"
#include "../Renderer.h"

RawBuffer::RawBuffer(const eastl::string& name)
{
    m_name = name;
}

bool RawBuffer::Create(uint32_t size, RHIMemoryType memoryType, bool uav)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IRHIDevice* pDevice = pRenderer->GetDevice();

    MY_ASSERT(size % 4 == 0);       //< Raw buffer is byte address buffer, 32bits values

    RHIBufferDesc desc;
    desc.m_stride = 4;
    desc.m_size = size;
    desc.m_format = RHIFormat::R32F;
    desc.m_memoryType = memoryType;
    desc.m_usage = RHIBufferUsageBit::RHIBufferUsageRawBuffer;

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
    srvDesc.m_type = RHIShaderResourceViewType::RawBuffer;
    srvDesc.m_buffer.m_size = size;
    m_pSRV.reset(pDevice->CreateShaderResourceView(m_pBuffer.get(), srvDesc, m_name));
    if (m_pSRV == nullptr)
    {
        return false;
    }

    if (uav)
    {
        RHIUnorderedAccessViewDesc uavDesc;
        uavDesc.m_type = RHIUnorderedAccessViewType::RawBuffer;
        uavDesc.m_buffer.m_size = size;
        m_pUAV.reset(pDevice->CreateUnorderedAccessView(m_pBuffer.get(), uavDesc, m_name));
        if (m_pUAV == nullptr)
        {
            return false;
        }
    }

    return true;
     
}