#pragma once

#include "D3D12Headers.h"
#include "../RHIDescriptor.h"
#include "../RHIBuffer.h"

class D3D12Device;

class D3D12ShaderResourceView : public IRHIDescriptor
{
public:
    D3D12ShaderResourceView(D3D12Device* pDevice, IRHIResource* pResource, const RHIShaderResourceViewDesc& desc, const eastl::string& name);
    ~D3D12ShaderResourceView();

    virtual void* GetHandle() const override { return m_pResource->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_descriptor.index; }
    
    bool Create();
private:
    IRHIResource* m_pResource = nullptr;
    RHIShaderResourceViewDesc m_desc = {};
    D3D12Descriptor m_descriptor;  
};

class D3D12UnorderedAccessView : public IRHIDescriptor
{
public:
    D3D12UnorderedAccessView(D3D12Device* pDevice, IRHIResource* pResource, const RHIUnorderedAccessViewDesc& desc, const eastl::string name);
    ~D3D12UnorderedAccessView();

    virtual void* GetHandle() const override { return m_pResource->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_descriptor.index; }
    
    bool Create();
    D3D12Descriptor GetShaderVisibleDescriptor() const { return m_descriptor; }
    D3D12Descriptor GetNonShaderVisiableDescriptor() const { return m_nonShaderVisibleDescriptor; }

private:
    IRHIResource* m_pResource;
    RHIUnorderedAccessViewDesc m_desc = {};
    D3D12Descriptor m_descriptor;
    D3D12Descriptor m_nonShaderVisibleDescriptor;   // For UAV clear
};

class D3D12ConstantBufferView : public IRHIDescriptor
{
public:
    D3D12ConstantBufferView(D3D12Device* pDevice, IRHIBuffer* pBuffer, const RHIConstantBufferViewDesc& desc, const eastl::string name);
    ~D3D12ConstantBufferView();
    
    virtual void* GetHandle() const override { return m_pBuffer->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_descriptor.index; }

    bool Create();
private:
    IRHIBuffer* m_pBuffer = nullptr;
    RHIConstantBufferViewDesc m_desc = {};
    D3D12Descriptor m_descriptor;
};

class D3D12Sampler : public IRHIDescriptor
{
public:
    D3D12Sampler(D3D12Device* pDevice, const RHISamplerDesc& desc, const eastl::string name);
    ~D3D12Sampler();

    virtual void* GetHandle() const override { return nullptr; }
    virtual uint32_t GetHeapIndex() const override { return m_descriptor.index; }

    bool Create();
    
private:
    RHISamplerDesc m_desc;
    D3D12Descriptor m_descriptor;
};