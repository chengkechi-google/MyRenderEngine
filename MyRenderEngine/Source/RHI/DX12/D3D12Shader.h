#pragma once

#include "D3D12Headers.h"
#include "../RHIShader.h"

class D3D12Device;

class D3D12Shader : public IRHIShader
{
public:
    D3D12Shader(D3D12Device* pDevice, const RHIShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name);
    
    virtual void* GetHandle() const override { return nullptr; }
    virtual bool SetShaderData(const uint8_t* data, uint32_t dataSize) override;

    D3D12_SHADER_BYTECODE GetByteCode() const { return { m_data.data(), m_data.size() }; }
private:
    eastl::vector<uint8_t> m_data;
};