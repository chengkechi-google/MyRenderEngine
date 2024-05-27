#pragma once
#include "RHI/RHI.h"
#include "EASTL/unique_ptr.h"

class Texture3D
{
public:
    Texture3D(const eastl::string& name);

    bool Create(uint32_t width, uint32_t height, uint32_t depth, uint32_t levels, RHIFormat format, RHITextureUsageFlags flags);

    IRHITexture* GetTexture() const { return m_pTexture.get(); }
    IRHIDescriptor* GetSRV() const { return m_pSRV.get(); }
    IRHIDescriptor* GetDSV() const { return m_pUAV.get(); }
    
private:
    eastl::string m_name;
    
    eastl::unique_ptr<IRHITexture> m_pTexture;
    eastl::unique_ptr<IRHIDescriptor> m_pSRV;
    eastl::unique_ptr<IRHIDescriptor> m_pUAV;
};