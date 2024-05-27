#pragma once
#include "RHI/RHI.h"
#include "EASTL/unique_ptr.h"

class TextureCube
{
public:
    TextureCube(const eastl::string& string);
    
    bool Create(uint32_t width, uint32_t height, uint32_t levels, RHIFormat format, RHITextureUsageFlags flags);

    IRHITexture* GetTexture() const { return m_pTexture.get(); }

    // TextureCube
    IRHIDescriptor* GetSRV() const { return m_pSRV.get(); }

    // Texture2DArray
    IRHIDescriptor* GetArraySRV() const { return m_pArraySRV.get(); }

    // RWTexture2DArray
    IRHIDescriptor* GetUAV(uint32_t mip) const { return m_pUAV[mip].get(); }
    
    
protected:
    eastl::string m_name;
    
    eastl::unique_ptr<IRHITexture> m_pTexture;
    eastl::unique_ptr<IRHIDescriptor> m_pSRV;
    eastl::unique_ptr<IRHIDescriptor> m_pArraySRV;
    
    eastl::vector<eastl::unique_ptr<IRHIDescriptor>> m_pUAV;
};