#include "D3D12Texture.h"
#include "D3D12Device.h"
#include "D3D12Heap.h"
#include "d3d12ma/D3D12MemAlloc.h"
#include "Utils/log.h"
#include "../RHI.h"


D3D12Texture::D3D12Texture(D3D12Device* device, const RHITextureDesc& desc, const eastl::string& name)
{
    m_pDevice = device;
    m_desc = desc;
    m_name = name;
}

D3D12Texture::~D3D12Texture()
{
    D3D12Device* device = (D3D12Device*) m_pDevice;
    device->Delete(m_pTexture);
    device->Delete(m_pAllocation);

    for (size_t i = 0; i < m_RTV.size(); ++i)
    {
        device->DeleteRTV(m_RTV[i]);
    }

    for (size_t i = 0; i < m_DSV.size(); ++i)
    {
        device->DeleteDSV(m_DSV[i]);
    }   
}

uint32_t D3D12Texture::GetRequiredStagingBufferSize() const
{
    ID3D12Device *pDevice = (ID3D12Device*) m_pDevice->GetHandle();
    D3D12_RESOURCE_DESC desc = m_pTexture->GetDesc();
    uint32_t subresourcesCount = m_desc.m_mipLevels * m_desc.m_arraySize;

    uint64_t size = 0;
    pDevice->GetCopyableFootprints(&desc, 0, subresourcesCount, 0, nullptr, nullptr, nullptr, &size);
    return (uint32_t) size;
}

uint32_t D3D12Texture::GetRowPitch(uint32_t mipLevel) const
{
    MY_ASSERT(mipLevel < m_desc.m_mipLevels);

    ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();
    D3D12_RESOURCE_DESC desc = m_pTexture->GetDesc();
    
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footPrint;
    pDevice->GetCopyableFootprints(&desc, mipLevel, 1, 0, &footPrint, nullptr, nullptr, nullptr);

    return footPrint.Footprint.RowPitch;
}

RHITilingDesc D3D12Texture::GetTilingDesc() const
{
    MY_ASSERT(m_desc.m_allocationType == RHIAllocationType::Sparse);

    ID3D12Device *pDevice = (ID3D12Device*) m_pDevice->GetHandle();

    uint32_t tileCount;
    D3D12_PACKED_MIP_INFO packedMipInfo;
    D3D12_TILE_SHAPE tileShape;
    pDevice->GetResourceTiling(m_pTexture, &tileCount, &packedMipInfo, &tileShape, nullptr, 0, nullptr);

    RHITilingDesc info = {};
    info.m_tileCount = tileCount;
    info.m_standardMips = packedMipInfo.NumStandardMips;
    info.m_tileWidth = tileShape.WidthInTexels;
    info.m_tileHeight = tileShape.HeightInTexels;
    info.m_tileDepth = tileShape.DepthInTexels;
    info.m_packedMips = packedMipInfo.NumPackedMips;
    info.m_packedMipTiles = packedMipInfo.NumTilesForPackedMips;

    return info;
}

RHISubresourceTilingDesc D3D12Texture::GetSubresourceTilingDesc(uint32_t subresource) const
{
    MY_ASSERT(m_desc.m_allocationType == RHIAllocationType::Sparse);

    ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

    uint32_t subresourceCount = 1;
    D3D12_SUBRESOURCE_TILING tiling;
    pDevice->GetResourceTiling(m_pTexture, nullptr, nullptr, nullptr, &subresourceCount, subresource, &tiling);
    RHISubresourceTilingDesc info;
    info.m_width = tiling.WidthInTiles;
    info.m_height = tiling.HeightInTiles;
    info.m_depth = tiling.DepthInTiles;
    info.m_tileOffset = tiling.StartTileIndexInOverallResource;

    return info;
}


bool D3D12Texture::Create()
{
    ID3D12Device* device = (ID3D12Device*) m_pDevice->GetHandle();
    
    D3D12MA::Allocator* pAllocator = ((D3D12Device*)m_pDevice)->GetResourceAllocator();
    D3D12_RESOURCE_DESC resourceDesc = RHIToD3D12ResourceDesc(m_desc);
    D3D12_RESOURCE_DESC1 resourceDesc1 = CD3DX12_RESOURCE_DESC1(resourceDesc);

    D3D12_BARRIER_LAYOUT initialLayout;
    if (m_desc.m_usage & RHITextureUsageRenderTarget)
    {
        initialLayout = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
    }else if (m_desc.m_usage & RHITextureUsageDepthStencil)
    {
        initialLayout = D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
    }else if (m_desc.m_usage & RHITextureUsageUnorderedAccess)
    {
        initialLayout = D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
    }
    else
    {
        initialLayout = D3D12_BARRIER_LAYOUT_COMMON;
    }

    HRESULT hResult;
    if (m_desc.m_heap != nullptr)
    {
        MY_ASSERT(m_desc.m_allocationType == RHIAllocationType::Placed);
        MY_ASSERT(m_desc.m_memoryType == m_desc.m_heap->GetDesc().m_memoryType);
        
        hResult = pAllocator->CreateAliasingResource2((D3D12MA::Allocation*) m_desc.m_heap->GetHandle(),
             m_desc.m_heapOffset, &resourceDesc1, initialLayout, nullptr, 0, nullptr, IID_PPV_ARGS(&m_pTexture));
    }
    else if (m_desc.m_allocationType == RHIAllocationType::Sparse)
    {
        ID3D12Device10* device = (ID3D12Device10*)m_pDevice->GetHandle();
        hResult = device->CreateReservedResource2(&resourceDesc, initialLayout, nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&m_pTexture));
    }
    else
    {
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = RHIToD3D12HeapType(m_desc.m_memoryType);
        allocationDesc.Flags = m_desc.m_allocationType == RHIAllocationType::Comitted ? D3D12MA::ALLOCATION_FLAG_COMMITTED : D3D12MA::ALLOCATION_FLAG_NONE;
        
        hResult = pAllocator->CreateResource3(&allocationDesc, &resourceDesc1, initialLayout, nullptr, 0, nullptr, &m_pAllocation, IID_PPV_ARGS(&m_pTexture));
    }

    if (FAILED(hResult))
    {
        MY_ERROR("[D3D12Texture] failed to create{}", m_name);
        return false;
    }

    eastl::wstring nameWStr = string_to_wstring(m_name);
    m_pTexture->SetName(nameWStr.c_str());
    if (m_pAllocation)
    {
        m_pAllocation->SetName(nameWStr.c_str());
    }
    
    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Texture::GetRTV(uint32_t mipSlice, uint32_t arraySlice)
{
    MY_ASSERT(m_desc.m_usage && RHITextureUsageBit::RHITextureUsageRenderTarget);

    if (m_RTV.empty())
    {
        m_RTV.resize(m_desc.m_mipLevels * m_desc.m_arraySize);
    }

    uint32_t index = m_desc.m_mipLevels * arraySlice + mipSlice;
    if (IsNullDescriptor(m_RTV[index]))
    {
        m_RTV[index] = ((D3D12Device*) m_pDevice)->AllocateRTV();

        D3D12_RENDER_TARGET_VIEW_DESC desc = {};
        desc.Format = DXGIFormat(m_desc.m_format);
        
        switch (m_desc.m_type)
        {
            case RHITextureType::Texture2D:
            {
                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = mipSlice;
                break;
            }
            
            case RHITextureType::Texture2DArray:
            case RHITextureType::TextureCube:
            {
                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = mipSlice;
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.FirstArraySlice = arraySlice;
                break;
            }

            default:
            {
                MY_ASSERT(false);
                break;
            }           
        }

        ID3D12Device* pDevice = (ID3D12Device*) m_pDevice->GetHandle();
        pDevice->CreateRenderTargetView(m_pTexture, &desc, m_RTV[index].cpuHandle);
    }
    
    return m_RTV[index].cpuHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Texture::GetDSV(uint32_t mipSlice, uint32_t arraySlice)
{
    MY_ASSERT(m_desc.m_usage && RHITextureUsageBit::RHITextureUsageDepthStencil);
    
    if (m_DSV.empty())
    {
        m_DSV.resize(m_desc.m_mipLevels * m_desc.m_arraySize);
    }

    uint32_t index = m_desc.m_mipLevels * arraySlice + mipSlice;
    if (IsNullDescriptor(m_DSV[index]))
    {
        m_DSV[index] = ((D3D12Device*)m_pDevice)->AllocateRTV();

        D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
        desc.Format = DXGIFormat(m_desc.m_format);

        switch (m_desc.m_type)
        {
            case RHITextureType::Texture2D:
            {
                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = mipSlice;
                break;
            }

            case RHITextureType::Texture2DArray:
            case RHITextureType::TextureCube:
            {
                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = mipSlice;
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.FirstArraySlice = arraySlice;
                break;
            }

            default:
            {
                MY_ASSERT(false);
                break;
            }
        }

        ID3D12Device* pDevice = (ID3D12Device*) m_pDevice->GetHandle();
        pDevice->CreateDepthStencilView(m_pTexture, &desc, m_DSV[index].cpuHandle);
    }

    return  m_DSV[index].cpuHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Texture::GetReadOnlyDSV(uint32_t mipSlice, uint32_t arraySlice)
{
    MY_ASSERT(m_desc.m_usage && RHITextureUsageBit::RHITextureUsageDepthStencil);
    
    if (m_readOnlyDSV.empty())
    {
        m_readOnlyDSV.resize(m_desc.m_mipLevels * m_desc.m_arraySize);
    }

    uint32_t index = m_desc.m_mipLevels * arraySlice + mipSlice;
    if (IsNullDescriptor(m_readOnlyDSV[index]))
    {
        m_readOnlyDSV[index] = ((D3D12Device*) m_pDevice)->AllocateDSV();
        
        D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
        desc.Format = DXGIFormat(m_desc.m_format);
        desc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
        if (IsStencilFormat(m_desc.m_format))
        {
            desc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        }

        switch (m_desc.m_type)
        {
            case RHITextureType::Texture2D:
            {
                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = mipSlice;
                break;
            }

            case RHITextureType::Texture2DArray:
            case RHITextureType::TextureCube:
            {
                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.MipSlice = mipSlice;
                desc.Texture2DArray.ArraySize = 1;
                desc.Texture2DArray.FirstArraySlice = arraySlice;
                break;
            }

            default:
            {
                MY_ASSERT(false);
                break;
            }
        }

        ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();
        pDevice->CreateDepthStencilView(m_pTexture, &desc, m_readOnlyDSV[index].cpuHandle);
    }

    return  m_readOnlyDSV[index].cpuHandle;
}