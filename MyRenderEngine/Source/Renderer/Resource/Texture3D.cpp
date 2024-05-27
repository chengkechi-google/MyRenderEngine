#include "Texture3D.h"
#include "Core/Engine.h"
#include "../Renderer.h"

Texture3D::Texture3D(const eastl::string& name)
{
    m_name = name;
}

bool Texture3D::Create(uint32_t width, uint32_t height, uint32_t depth, uint32_t levels, RHIFormat format, RHITextureUsageFlags flags)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IRHIDevice* pDevice = pRenderer->GetDevice();

    RHITextureDesc desc;
    desc.m_width = width;
    desc.m_height = height;
    desc.m_depth = depth;
    desc.m_mipLevels = levels;
    desc.m_type = RHITextureType::Texture3D;
    desc.m_format = format;
    desc.m_usage = flags;

    if ((flags & RHITextureUsageBit::RHITextureUsageRenderTarget) || (flags & RHITextureUsageBit::RHITextureUsageUnorderedAccess))
    {
        desc.m_allocationType = RHIAllocationType::Comitted;
    }

    m_pTexture.reset(pDevice->CreateTexture(desc, m_name));
    if (m_pTexture == nullptr)
    {
        return false;
    }

    RHIShaderResourceViewDesc srvDesc;
    srvDesc.m_type = RHIShaderResourceViewType::Texture3D;
    srvDesc.m_format = format;
    m_pSRV.reset(pDevice->CreateShaderResourceView(m_pTexture.get(), srvDesc, m_name));
    if (m_pSRV == nullptr)
    {
        return false;
    }

    if (flags & RHITextureUsageBit::RHITextureUsageUnorderedAccess)
    {
        RHIUnorderedAccessViewDesc uavDesc;
        uavDesc.m_type = RHIUnorderedAccessViewType::Texture3D;
        uavDesc.m_format = format;
        m_pUAV.reset(pDevice->CreateUnorderedAccessView(m_pTexture.get(), uavDesc, m_name));
        if(m_pUAV == nullptr)
        {
            return false;
        }
    }

    return true;
}