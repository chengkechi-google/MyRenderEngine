#include "D3D12Descriptor.h"
#include "D3D12Device.h"
#include "D3D12Buffer.h"
#include "D3D12Texture.h"
#include "D3D12RayTracingBLAS.h"
#include "D3D12RayTracingTLAS.h"
#include "Utils/assert.h"

D3D12ShaderResourceView::D3D12ShaderResourceView(D3D12Device* pDevice, IRHIResource* pResource, const RHIShaderResourceViewDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_pResource = pResource;
    m_name = name;
}

D3D12ShaderResourceView::~D3D12ShaderResourceView()
{
    ((D3D12Device*) m_pDevice)->DeleteResoruce(m_descriptor);
}

bool D3D12ShaderResourceView::Create()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    switch (m_desc.m_type)
    {
        case RHIShaderResourceViewType::Texture2D:
        {
            const RHITextureDesc& textureDesc = ((IRHITexture*) m_pResource)->GetDesc();
            bool depth = textureDesc.m_usage & RHITextureUsageBit::RHITextureUsageDepthStencil;

            srvDesc.Format = DXGIFormat(m_desc.m_format, depth);
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = m_desc.m_texture.m_mipSlice;
            srvDesc.Texture2D.MipLevels = m_desc.m_texture.m_mipLevels;
            srvDesc.Texture2D.PlaneSlice = m_desc.m_texture.m_planeSlice;
            break;
        }
        
        case RHIShaderResourceViewType::Texture2DArray:
        {
            const RHITextureDesc& textureDesc = ((IRHITexture*)m_pResource)->GetDesc();

            srvDesc.Format = DXGIFormat(m_desc.m_format);
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

            srvDesc.Texture2DArray.MostDetailedMip = m_desc.m_texture.m_mipSlice;
            srvDesc.Texture2DArray.MipLevels = m_desc.m_texture.m_mipLevels;
            srvDesc.Texture2DArray.FirstArraySlice = m_desc.m_texture.m_arraySlice;
            srvDesc.Texture2DArray.ArraySize = m_desc.m_texture.m_arraySize;
            srvDesc.Texture2DArray.PlaneSlice = m_desc.m_texture.m_planeSlice;
            break;
        }

        case RHIShaderResourceViewType::Texture3D:
        {
            const RHITextureDesc& textureDesc = ((IRHITexture*)m_pResource)->GetDesc();

            srvDesc.Format = DXGIFormat(m_desc.m_format);
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;

            srvDesc.Texture2DArray.MostDetailedMip = m_desc.m_texture.m_mipSlice;
            srvDesc.Texture2DArray.MipLevels = m_desc.m_texture.m_mipLevels;
            break;
        }

        case RHIShaderResourceViewType::TextureCube:
        {
            const RHITextureDesc& textureDesc = ((IRHITexture*)m_pResource)->GetDesc();

            srvDesc.Format = DXGIFormat(m_desc.m_format);
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;

            srvDesc.TextureCube.MostDetailedMip = m_desc.m_texture.m_mipSlice;
            srvDesc.TextureCube.MipLevels = m_desc.m_texture.m_mipLevels;
            break;
        }

        case RHIShaderResourceViewType::TextureCubeArray:
        {
            const RHITextureDesc& textureDesc = ((IRHITexture*)m_pResource)->GetDesc();

            srvDesc.Format = DXGIFormat(m_desc.m_format);
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;

            srvDesc.TextureCubeArray.MostDetailedMip = m_desc.m_texture.m_mipSlice;
            srvDesc.TextureCubeArray.MipLevels = m_desc.m_texture.m_mipLevels;
            srvDesc.TextureCubeArray.First2DArrayFace = 0;
            srvDesc.TextureCubeArray.NumCubes = m_desc.m_texture.m_arraySize / 6;
            break;
        }

        case RHIShaderResourceViewType::StructuredBuffer:
        {
            const RHIBufferDesc& bufferDesc = ((IRHIBuffer*) m_pResource)->GetDesc();
            MY_ASSERT(bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageStructedBuffer);
            MY_ASSERT(m_desc.m_format == RHIFormat::Unknown);
            MY_ASSERT(m_desc.m_buffer.m_offset % bufferDesc.m_stride == 0);
            MY_ASSERT(m_desc.m_buffer.m_size % bufferDesc.m_stride == 0);

            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.FirstElement = m_desc.m_buffer.m_offset / bufferDesc.m_stride;
            srvDesc.Buffer.NumElements = m_desc.m_buffer.m_size / bufferDesc.m_stride;
            srvDesc.Buffer.StructureByteStride = bufferDesc.m_stride;
            break;
        }

        case RHIShaderResourceViewType::TypedBuffer:
        {
            const RHIBufferDesc& bufferDesc = ((IRHIBuffer*)m_pResource)->GetDesc();
            MY_ASSERT(bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageTypedBuffer);
            MY_ASSERT(m_desc.m_buffer.m_offset % bufferDesc.m_stride == 0);
            MY_ASSERT(m_desc.m_buffer.m_size % bufferDesc.m_stride == 0);

            srvDesc.Format = DXGIFormat(m_desc.m_format);
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.FirstElement = m_desc.m_buffer.m_offset / bufferDesc.m_stride;
            srvDesc.Buffer.NumElements = m_desc.m_buffer.m_size / bufferDesc.m_stride;
            break;
        }

        case RHIShaderResourceViewType::RawBuffer:
        {
            const RHIBufferDesc& bufferDesc = ((IRHIBuffer*)m_pResource)->GetDesc();
            MY_ASSERT(bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageRawBuffer);
            MY_ASSERT(bufferDesc.m_stride % 4 == 0);
            MY_ASSERT(m_desc.m_buffer.m_offset % 4 == 0);
            MY_ASSERT(m_desc.m_buffer.m_size % 4 == 0);

            srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.FirstElement = m_desc.m_buffer.m_offset / 4;
            srvDesc.Buffer.NumElements = m_desc.m_buffer.m_size / 4;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            break;
        }
        
        case RHIShaderResourceViewType::RayTracingTLAS:
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
            srvDesc.RaytracingAccelerationStructure.Location = ((D3D12RayTracingTLAS*) m_pResource)->GetGPUAddress();
            break;
        }
        
        default:
        {
            break;
        }        
    }

    m_descriptor = ((D3D12Device*) m_pDevice)->AllocateResourceDescriptor();
    
    ID3D12Device10* pDevice = (ID3D12Device10*) m_pDevice->GetHandle();
    ID3D12Resource* pResource = m_desc.m_type == RHIShaderResourceViewType::RayTracingTLAS ? nullptr : (ID3D12Resource*) m_pResource->GetHandle();
    pDevice->CreateShaderResourceView(pResource, &srvDesc, m_descriptor.cpuHandle);

    return true;
}

D3D12UnorderedAccessView::D3D12UnorderedAccessView(D3D12Device* pDevice, IRHIResource* pResource, const RHIUnorderedAccessViewDesc& desc, const eastl::string name)
{
    m_pDevice = pDevice;
    m_pResource = pResource;
    m_desc = desc;
    m_name = name;
}

D3D12UnorderedAccessView::~D3D12UnorderedAccessView()
{
    ((D3D12Device*) m_pDevice)->DeleteResoruce(m_descriptor);
    ((D3D12Device*) m_pDevice)->DeleteNonShaderVisiableUAV(m_nonShaderVisibleDescriptor);
}

bool D3D12UnorderedAccessView::Create()
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    
    switch(m_desc.m_type)
    {
        case RHIUnorderedAccessViewType::Texture2D:
        {
            const RHITextureDesc& textureDesc = ((IRHITexture*) m_pResource)->GetDesc();
            MY_ASSERT(textureDesc.m_usage & RHITextureUsageBit::RHITextureUsageUnorderedAccess);
            
            uavDesc.Format = DXGIFormat(m_desc.m_format, false, true);
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = m_desc.m_texture.m_mipSlice;
            uavDesc.Texture2D.PlaneSlice = m_desc.m_texture.m_planeSlice;
            break;
        }

        case RHIUnorderedAccessViewType::Texture2DArray:
        {
            const RHITextureDesc& textureDesc = ((IRHITexture*) m_pResource)->GetDesc();
            MY_ASSERT(textureDesc.m_usage & RHITextureUsageBit::RHITextureUsageUnorderedAccess);

            uavDesc.Format = DXGIFormat(m_desc.m_format, false, true);
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.MipSlice = m_desc.m_texture.m_mipSlice;
            uavDesc.Texture2DArray.PlaneSlice = m_desc.m_texture.m_planeSlice;
            uavDesc.Texture2DArray.FirstArraySlice = m_desc.m_texture.m_arraySize;
            uavDesc.Texture2DArray.FirstArraySlice = m_desc.m_texture.m_arraySlice;
            break;
        }

        case RHIUnorderedAccessViewType::Texture3D:
        {
            const RHITextureDesc& textureDesc = ((IRHITexture*) m_pResource)->GetDesc();
            MY_ASSERT(textureDesc.m_usage & RHITextureUsageBit::RHITextureUsageUnorderedAccess);

            uavDesc.Format = DXGIFormat(m_desc.m_format, false, true);
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            uavDesc.Texture3D.MipSlice = m_desc.m_texture.m_mipSlice;
            uavDesc.Texture3D.FirstWSlice = 0;
            uavDesc.Texture3D.WSize = textureDesc.m_depth;
            break;
        }

        case RHIUnorderedAccessViewType::StructuredBuffer:
        {
            const RHIBufferDesc& bufferDesc = ((IRHIBuffer*) m_pResource)->GetDesc();
            MY_ASSERT(bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageStructedBuffer);
            MY_ASSERT(bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageUnorderedAccess);
            MY_ASSERT(m_desc.m_format == RHIFormat::Unknown);
            MY_ASSERT(m_desc.m_buffer.m_offset % bufferDesc.m_stride == 0);
            MY_ASSERT(m_desc.m_buffer.m_size % bufferDesc.m_stride == 0);

            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = m_desc.m_buffer.m_offset / bufferDesc.m_stride;
            uavDesc.Buffer.NumElements = m_desc.m_buffer.m_size / bufferDesc.m_stride;
            uavDesc.Buffer.StructureByteStride = bufferDesc.m_stride;
            break;
        }

        case RHIUnorderedAccessViewType::TypedBuffer:
        {
            const RHIBufferDesc& bufferDesc = ((IRHIBuffer*) m_pResource)->GetDesc();
            MY_ASSERT(bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageTypedBuffer);
            MY_ASSERT(bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageUnorderedAccess);
            MY_ASSERT(m_desc.m_buffer.m_offset % bufferDesc.m_stride == 0);
            MY_ASSERT(m_desc.m_buffer.m_size % bufferDesc.m_stride == 0);

            uavDesc.Format = DXGIFormat(m_desc.m_format);
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = m_desc.m_buffer.m_offset / bufferDesc.m_stride;
            uavDesc.Buffer.NumElements = m_desc.m_buffer.m_size / bufferDesc.m_stride;
            break;
        }

        case RHIUnorderedAccessViewType::RawBuffer:
        {
            const RHIBufferDesc& bufferDesc = ((IRHIBuffer*) m_pResource)->GetDesc();
            MY_ASSERT(bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageRawBuffer);
            MY_ASSERT(bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageUnorderedAccess);
            MY_ASSERT(bufferDesc.m_stride % 4 == 0);
            MY_ASSERT(m_desc.m_buffer.m_offset % 4 == 0);
            MY_ASSERT(m_desc.m_buffer.m_size % 4 == 0);

            uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = m_desc.m_buffer.m_offset / 4;
            uavDesc.Buffer.NumElements = m_desc.m_buffer.m_size / 4;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            break;
        }

        default:
        {
            break;
        }
    }

    m_descriptor = ((D3D12Device*) m_pDevice)->AllocateResourceDescriptor();
    m_nonShaderVisibleDescriptor = ((D3D12Device*)m_pDevice)->AllocateNonShaderVisibleUAV();

    ID3D12Device* pDevice = (ID3D12Device*) m_pDevice->GetHandle();
    pDevice->CreateUnorderedAccessView((ID3D12Resource*) m_pResource->GetHandle(), nullptr, &uavDesc, m_descriptor.cpuHandle);
    pDevice->CreateUnorderedAccessView((ID3D12Resource*) m_pResource->GetHandle(), nullptr, &uavDesc, m_nonShaderVisibleDescriptor.cpuHandle);
    return true;
}

D3D12ConstantBufferView::D3D12ConstantBufferView(D3D12Device* pDevice, IRHIBuffer* pBuffer, const RHIConstantBufferViewDesc& desc, const eastl::string name)
{
    m_pDevice = pDevice;
    m_pBuffer = pBuffer;
    m_desc = desc;
    m_name = name;
}

D3D12ConstantBufferView::~D3D12ConstantBufferView()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    pDevice->DeleteResoruce(m_descriptor);
}

bool D3D12ConstantBufferView::Create()
{
    MY_ASSERT(m_pBuffer->GetDesc().m_usage & RHIBufferUsageBit::RHIBufferUsageConstantBuffer);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_pBuffer->GetGPUAddress() + m_desc.m_offset;
    cbvDesc.SizeInBytes = m_desc.m_size;

    m_descriptor = ((D3D12Device*) m_pDevice)->AllocateResourceDescriptor();
    
    ID3D12Device *pDevice = (ID3D12Device*) m_pDevice->GetHandle();
    pDevice->CreateConstantBufferView(&cbvDesc, m_descriptor.cpuHandle);
    return true;
}

D3D12Sampler::D3D12Sampler(D3D12Device* pDevice, const RHISamplerDesc& desc, const eastl::string name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

D3D12Sampler::~D3D12Sampler()
{
    ((D3D12Device*) m_pDevice)->DeleteSampler(m_descriptor);
}

bool D3D12Sampler::Create()
{
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = RHIToD3D12Filter(m_desc);
    samplerDesc.AddressU = RHIToD3D12AddressMode(m_desc.m_addressU);
    samplerDesc.AddressV = RHIToD3D12AddressMode(m_desc.m_addressV);
    samplerDesc.AddressW = RHIToD3D12AddressMode(m_desc.m_addressW);
    samplerDesc.MipLODBias = m_desc.m_mipBias;
    samplerDesc.MaxAnisotropy = (UINT) m_desc.m_maxAnisotropy;
    samplerDesc.ComparisonFunc = m_desc.m_reductionMode == RHISamplerReductionMode::Compare ? RHIToD3D12CompareFunc(m_desc.m_compareFunc) : D3D12_COMPARISON_FUNC_NONE;
    samplerDesc.MinLOD = m_desc.m_minLOD;
    samplerDesc.MaxLOD = m_desc.m_maxLOD;
    memcpy(samplerDesc.BorderColor, m_desc.m_borderColor, sizeof(float) * 4);
    
    m_descriptor = ((D3D12Device*) m_pDevice)->AllocateSampler();
    
    ID3D12Device* pDevice = (ID3D12Device*) m_pDevice->GetHandle();
    pDevice->CreateSampler(&samplerDesc, m_descriptor.cpuHandle);
    return true;
}
