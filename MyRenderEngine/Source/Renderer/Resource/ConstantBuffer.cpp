#include "ConstantBuffer.h"
#include "Core/Engine.h"
#include "../Renderer.h"

ConstantBuffer::ConstantBuffer(const eastl::string& name)
{
    m_name = name;
}

bool ConstantBuffer::Create(uint32_t size, uint32_t stride, RHIMemoryType memoryType)
{
    // todo : should check is 64kb and 256 byte alignment
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IRHIDevice* pDevice = pRenderer->GetDevice();

    RHIBufferDesc desc;
    desc.m_stride = stride;
    desc.m_size = size;
    desc.m_format = RHIFormat::Unknown;
    desc.m_memoryType = memoryType;
    desc.m_usage = RHIBufferUsageBit::RHIBufferUsageConstantBuffer;

    m_pBuffer.reset(pDevice->CreateBuffer(desc, m_name));
    if (m_pBuffer == nullptr)
    {
        return false;
    }

    RHIConstantBufferViewDesc cbvDesc;
    cbvDesc.m_size = size;
    m_pCBV.reset(pDevice->CreateConstBufferView(m_pBuffer.get(), cbvDesc, m_name));
    if (m_pCBV == nullptr)
    {
        return false;
    }

    return true;
}