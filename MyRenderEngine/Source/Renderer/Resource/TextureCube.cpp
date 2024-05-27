#include "TextureCube.h"
#include "Core/Engine.h"
#include "../Renderer.h"

TextureCube::TextureCube(const eastl::string& name)
{
    m_name = name;
}

bool TextureCube::Create(uint32_t width, uint32_t height, uint32_t levels, RHIFormat format, RHITextureUsageFlags flags)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IRHIDevice* pDevice = pRenderer->GetDevice();

    RHITextureDesc desc;
    desc.m_width = width;
    desc.m_height = height;
    desc.m_mipLevels = levels;
    desc.m_arraySize = 6;
    desc.m_type = RHITextureType::TextureCube;
    desc.m_format = format;
    desc.m_usage = flags;

    if ((flags & RHITextureUsageBit::RHITextureUsageRenderTarget) || (flags & RHITextureUsageBit::RHITextureUsageDepthStencil) || (flags & RHITextureUsageBit::RHITextureUsageUnorderedAccess))
    {
        desc.m_allocationType = RHIAllocationType::Comitted;
    }

    m_pTexture.reset(pDevice->CreateTexture(desc, m_name));
    if (m_pTexture == nullptr)
    {
        return false;
    }

    RHIShaderResourceViewDesc srvDesc;
    srvDesc.m_type = RHIShaderResourceViewType::TextureCube;
    srvDesc.m_format = format;
    srvDesc.m_texture.m_arraySize = 6;
    
    m_pSRV.reset(pDevice->CreateShaderResourceView(m_pTexture.get(), srvDesc, m_name));
    if (m_pSRV == nullptr)
    {
        return false;
    }

    srvDesc.m_type = RHIShaderResourceViewType::Texture2DArray;
    m_pArraySRV.reset(pDevice->CreateShaderResourceView(m_pTexture.get(), srvDesc, m_name));
    if (m_pArraySRV == nullptr)
    {
        return false;
    }

    if (flags & RHITextureUsageBit::RHITextureUsageUnorderedAccess)
    {
        RHIUnorderedAccessViewDesc uavDesc;
        uavDesc.m_type = RHIUnorderedAccessViewType::Texture2DArray;
        uavDesc.m_format = format;
        uavDesc.m_texture.m_arraySize = 6;

        for (uint32_t i = 0; i < levels; ++i)
        {
            uavDesc.m_texture.m_mipSlice = i;
            IRHIDescriptor* pUAV = pDevice->CreateUnorderedAccessView(m_pTexture.get(), uavDesc, m_name);
            if (pUAV == nullptr)
            {
                return false;
            }

            m_pUAV.emplace_back(pUAV);
        }
    }

    return true;
}