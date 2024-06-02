#include "IndexBuffer.h"
#include "Core/Engine.h"
#include "../Renderer.h"

IndexBuffer::IndexBuffer(const eastl::string& name)
{
    m_name = name;
}

bool IndexBuffer::Create(uint32_t stride, uint32_t indexCount, RHIMemoryType memoryType)
{
    MY_ASSERT(stride == 2 || stride == 4);
    m_indexCount = indexCount;

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IRHIDevice* pDevice = pRenderer->GetDevice();

    RHIBufferDesc desc;
    desc.m_stride = stride;
    desc.m_size = stride * indexCount;
    desc.m_format = stride == 2 ? RHIFormat::R16UI : RHIFormat::R32UI;
    desc.m_memoryType = memoryType;

    m_pBuffer.reset(pDevice->CreateBuffer(desc, m_name));
    if (m_pBuffer == nullptr)
    {
        return false;
    }

    return true;
}