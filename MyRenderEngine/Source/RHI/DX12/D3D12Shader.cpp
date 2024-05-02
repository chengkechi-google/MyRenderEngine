#include "D3D12Shader.h"
#include "D3D12Device.h"
#include "xxHash/xxhash.h"

D3D12Shader::D3D12Shader(D3D12Device* pDevice, const RHIShaderDesc& desc, eastl::span<uint8_t> data, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
    
    SetShaderData(data.data(), data.size());
}

bool D3D12Shader::SetShaderData(const uint8_t* data, uint32_t dataSize)
{
    m_data.resize(dataSize);
    memcpy(m_data.data(), data, dataSize);

    m_hash = XXH3_64bits(data, dataSize);
    return true;
}