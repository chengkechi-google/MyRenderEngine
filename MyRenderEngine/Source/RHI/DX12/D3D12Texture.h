#pragma once
#include "D3D12Headers.h"
#include "../RHITexture.h"

class D3D12Device;
class D3D12Heap;

namespace D3D12MA
{
    class Allocation;
}

class D3D12Texture : public IRHITexture
{
public:
    D3D12Texture(D3D12Device* device, const RHITextureDesc& desc, const eastl::string& name);
    ~D3D12Texture();

    virtual void* GetHandle() const override { return m_pTexture; }
    virtual uint32_t GetRequiredStagingBufferSize() const override;
    virtual uint32_t GetRowPitch(uint32_t mipLevel) const override;
    virtual RHITilingDesc GetTilingDesc() const override;
    virtual RHISubresourceTilingDesc GetSubresourceTilingDesc(uint32_t subresource) const override;
    
    bool Create();
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(uint32_t mipSlice, uint32_t arraySlice);
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(uint32_t mipSlice, uint32_t arraySlice);
    D3D12_CPU_DESCRIPTOR_HANDLE GetReadOnlyDSV(uint32_t mipSlice, uint32_t arraySlice);
private:
    ID3D12Resource* m_pTexture = nullptr;
    
    D3D12MA::Allocation* m_pAllocation = nullptr;
    eastl::vector<D3D12Descriptor> m_RTV;
    eastl::vector<D3D12Descriptor> m_DSV;
    eastl::vector<D3D12Descriptor> m_readOnlyDSV;
private:
    friend class D3D12SwapChain;
};