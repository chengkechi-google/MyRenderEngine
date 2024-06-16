#pragma once
#include "RHI/RHI.h"
#include "EASTL/unique_ptr.h"

class Renderer;

class Texture2D
{
public:
    Texture2D(const eastl::string& name);
    
    bool Create(uint32_t width, uint32_t height, uint32_t levels, RHIFormat format, RHITextureUsageFlags flags);
    
    IRHITexture* GetTexture() const { return m_pTexture.get(); }
    IRHIDescriptor* GetSRV() const { return m_pSRV.get(); }
    IRHIDescriptor* GetUAV(uint32_t mip = 0) const;

    const eastl::string& GetName() const { return m_name; }

protected:
    eastl::string m_name;
    
    eastl::unique_ptr<IRHITexture> m_pTexture;
    eastl::unique_ptr<IRHIDescriptor> m_pSRV;
    eastl::vector<eastl::unique_ptr<IRHIDescriptor>> m_pUAVs;
};