#include "D3D12CommandList.h"
#include "D3D12Device.h"
#include "D3D12Fence.h"
#include "D3D12Texture.h"
#include "D3D12Buffer.h"
#include "D3D12PipelineState.h"
#include "D3D12Heap.h"
#include "D3D12Descriptor.h"
#include "D3D12RayTracingBLAS.h"
#include "D3D12RayTracingTLAS.h"
#include "D3D12SwapChain.h"
#include "PixRuntime.h"
#include "ags.h"
#include "../RHI.h"
#include "Utils/assert.h"
#include "Utils/profiler.h"
#include "Utils/log.h"

D3D12CommandList::D3D12CommandList(IRHIDevice* pDevice, RHICommandQueue queueType, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_queueType = queueType;
    m_name = name;
}

D3D12CommandList::~D3D12CommandList()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    pDevice->Delete(m_pCommandAllocator);
    pDevice->Delete(m_pCommandList);
}
bool D3D12CommandList::Create()
{
    D3D12Device* pDevice = (D3D12Device*) m_pDevice;
    D3D12_COMMAND_LIST_TYPE type;

    switch (m_queueType)
    {
        case RHICommandQueue::Graphics:
        {
            type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            m_pCommandQueue = pDevice->GetGraphicsQueue();
#if MICROPROFILE_GPU_TIMERS_D3D12
            m_profileQueue = pDevice->GetProfileGraphicsQueue();
#endif
            break;
        }

        case RHICommandQueue::Compute:
        {
            type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            m_pCommandQueue = pDevice->GetComputeQueue();
#if MICROPROFILE_GPU_TIMERS_D3D12
            m_profileQueue = pDevice->GetProfileComputeQueue();
#endif
            break;
        }

        case RHICommandQueue::Copy:
        {
            type = D3D12_COMMAND_LIST_TYPE_COPY;
            m_pCommandQueue = pDevice->GetCopyQueue();
#if MICROPROFILE_GPU_TIMERS_D3D12
            m_profileQueue = pDevice->GetProfileCopyQueue();
#endif
            break;
        }
        
        default:
        {
            MY_ERROR("[Command List] use unknown queuse type {}", (int) m_queueType);
            break;
        }
    }

    ID3D12Device* pD3D12Device = (ID3D12Device*) pDevice->GetHandle();
    HRESULT hResult = pD3D12Device->CreateCommandAllocator(type, IID_PPV_ARGS(&m_pCommandAllocator));
    if (FAILED(hResult))
    {
        MY_ERROR("[Command List] failed to create command allocator {}", m_name);
        return false;
    }
    m_pCommandAllocator->SetName(string_to_wstring(m_name + "allocator").c_str());

    hResult = pD3D12Device->CreateCommandList(0, type, m_pCommandAllocator, nullptr, IID_PPV_ARGS(&m_pCommandList));     //< Low cost without initial state
    if (FAILED(hResult))
    {
        MY_ERROR("[Command List] failed to create command list {}", m_name);
        return false;
    }

    m_pCommandList->Close();
    return true;
}

void D3D12CommandList::ResetAllocator()
{
    m_pCommandAllocator->Reset();
}

void D3D12CommandList::Begin()
{
    m_pCommandList->Reset(m_pCommandAllocator, nullptr);
    m_pCommandList->SetName(string_to_wstring(m_name).c_str()); //< Need resize string, maybe store wsting in the class
    
    ClearState();
}

void D3D12CommandList::End()
{
    FlushBarriers();
    m_pCommandList->Close();
}

void D3D12CommandList::Wait(IRHIFence* pFence, uint64_t value)
{
    m_pendingWaits.emplace_back(pFence, value);
}

void D3D12CommandList::Signal(IRHIFence* pFence, uint64_t value)
{
    m_pendingSignals.emplace_back(pFence, value);
}

void D3D12CommandList::Present(IRHISwapChain* pSwapChain)
{
    m_pendingSwapChain.push_back(pSwapChain);
}

void D3D12CommandList::Submit()
{
    for (size_t i = 0; i < m_pendingWaits.size(); ++i)
    {
        m_pCommandQueue->Wait((ID3D12Fence*) m_pendingWaits[i].first->GetHandle(), m_pendingWaits[i].second);
    }
    m_pendingWaits.clear();

    if (m_commandCount > 0)
    {
        ID3D12CommandList* ppCommandLists[] = { m_pCommandList };
        m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
    }

    for (size_t i = 0; i < m_pendingSwapChain.size(); ++i)
    {
        ((D3D12SwapChain*) m_pendingSwapChain[i])->Present();
    }
    m_pendingSwapChain.clear();

    for (size_t i = 0; i < m_pendingSignals.size(); ++i)
    {
        m_pCommandQueue->Signal((ID3D12Fence*) m_pendingSignals[i].first->GetHandle(), m_pendingSignals[i].second);
    }
    m_pendingSignals.clear();
}

void D3D12CommandList::ClearState()
{
    m_commandCount = 0;
    m_pCurrentPSO = nullptr;

    if (m_queueType == RHICommandQueue::Graphics || m_queueType == RHICommandQueue::Compute)
    {
        D3D12Device* pDevice = (D3D12Device*) m_pDevice;
        ID3D12DescriptorHeap* heaps[2] = { pDevice->GetResourceDescriptorHeap(), pDevice->GetSamplerDescriptorHeap() };
        m_pCommandList->SetDescriptorHeaps(2, heaps);

        ID3D12RootSignature* pRootSignature = pDevice->GetRootSignature();
        m_pCommandList->SetComputeRootSignature(pRootSignature);
        
        if (m_queueType == RHICommandQueue::Graphics)
        {
            m_pCommandList->SetGraphicsRootSignature(pRootSignature);
        }
    }    
}

void D3D12CommandList::BeginProfiling()
{
#if MICROPROFILE_GPU_TIMERS_D3D12
    if (m_profileQueue != -1)
    {
        m_pProfileLog = MicroProfileThreadLogGpuAlloc();
        MicroProfileGpuBegin(m_pCommandList, m_pProfileLog);
    }
#endif
}

void D3D12CommandList::EndProfiling()
{
#if MICROPROFILE_GPU_TIMERS_D3D12
    if (m_profileQueue != -1)
    {
        MicroProfileGpuSubmit(m_profileQueue, MicroProfileGpuEnd(m_pProfileLog));
        MicroProfileThreadLogGpuFree(m_pProfileLog);
    }
#endif
}

void D3D12CommandList::BeginEvent(const eastl::string& eventName)
{
    PIX::BeginEvent(m_pCommandList, eventName.c_str());
    ags::BeginEvent(m_pCommandList, eventName.c_str());
}

void D3D12CommandList::EndEvent()
{
    PIX::EndEvent(m_pCommandList);
    ags::EndEvent(m_pCommandList);
}

void D3D12CommandList::CopyBufferToTexture(IRHITexture* pDstTexture, uint32_t mipLevel, uint32_t arraySlice, IRHIBuffer* pSrcBuffer, uint32_t offset)
{
    FlushBarriers();
    
    const RHITextureDesc& desc = pDstTexture->GetDesc();

    uint32_t minWidth = GetFormatBlockWidth(desc.m_format);
    uint32_t minHeight = GetFormatBlockHeight(desc.m_format);
    uint32_t width = eastl::max(desc.m_width >> mipLevel, minWidth);
    uint32_t height = eastl::max(desc.m_height >> mipLevel, minHeight);
    uint32_t depth = eastl::max(desc.m_depth >> mipLevel, 1u);

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = (ID3D12Resource*) pDstTexture->GetHandle();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = CalcSubresource(desc, mipLevel, arraySlice);

    CD3DX12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = (ID3D12Resource*) pSrcBuffer->GetHandle();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = offset;
    src.PlacedFootprint.Footprint.Format = DXGIFormat(desc.m_format);
    src.PlacedFootprint.Footprint.Width = width;
    src.PlacedFootprint.Footprint.Height = height;
    src.PlacedFootprint.Footprint.Depth = depth;
    src.PlacedFootprint.Footprint.RowPitch = pDstTexture->GetRowPitch(mipLevel);

    m_pCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);        //< For now only copy from origin point
    ++ m_commandCount;
}

void D3D12CommandList::CopyTextureToBuffer(IRHIBuffer* pDstBuffer, IRHITexture* pSrcTexture, uint32_t mipLevel, uint32_t arraySlice)
{
    FlushBarriers();
    
    const RHITextureDesc& desc = pSrcTexture->GetDesc();
    uint32_t minWidth = GetFormatBlockWidth(desc.m_format);
    uint32_t minHeight = GetFormatBlockHeight(desc.m_format);
    uint32_t width = eastl::max(desc.m_width >> mipLevel, minWidth);
    uint32_t height = eastl::max(desc.m_height >> mipLevel, minHeight);
    uint32_t depth = eastl::max(desc.m_depth, 1u);
    
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = (ID3D12Resource*) pDstBuffer->GetHandle();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Footprint.Format = DXGIFormat(desc.m_format);
    dst.PlacedFootprint.Footprint.Width = width;
    dst.PlacedFootprint.Footprint.Height = height;
    dst.PlacedFootprint.Footprint.Depth = depth;
    dst.PlacedFootprint.Footprint.RowPitch = pSrcTexture->GetRowPitch(mipLevel);

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = (ID3D12Resource*) pSrcTexture->GetHandle();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = CalcSubresource(desc, mipLevel, arraySlice);
    
    m_pCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    ++ m_commandCount;
}

void D3D12CommandList::CopyBuffer(IRHIBuffer* pDstBuffer, uint32_t dstOffset, IRHIBuffer* pSrcBuffer, uint32_t srcOffset, uint32_t size)
{
    FlushBarriers();
    m_pCommandList->CopyBufferRegion((ID3D12Resource*)pDstBuffer->GetHandle(), dstOffset, (ID3D12Resource*)pSrcBuffer->GetHandle(), srcOffset, size);
    ++ m_commandCount;
}

void D3D12CommandList::CopyTexture(IRHITexture* pDstTexture, uint32_t dstMip, uint32_t dstArray, IRHITexture* pSrcTexture, uint32_t srcMip, uint32_t srcArray)
{
    FlushBarriers();

    D3D12_TEXTURE_COPY_LOCATION dstTexture = {};
    dstTexture.pResource = (ID3D12Resource*) pDstTexture->GetHandle();
    dstTexture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstTexture.SubresourceIndex = CalcSubresource(pDstTexture->GetDesc(), dstMip, dstArray);
    
    D3D12_TEXTURE_COPY_LOCATION srcTexture = {};
    srcTexture.pResource = (ID3D12Resource*) pSrcTexture->GetHandle();
    srcTexture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcTexture.SubresourceIndex = CalcSubresource(pSrcTexture->GetDesc(), srcMip, srcArray);

    m_pCommandList->CopyTextureRegion(&dstTexture, 0, 0, 0, &srcTexture, nullptr);
    ++ m_commandCount;
}

void D3D12CommandList::ClearUAV(IRHIResource* pResource, IRHIDescriptor* pUAV, const float* clearValue)
{
    MY_ASSERT(pResource->IsTexture() || pResource->IsBuffer());

    FlushBarriers();
    
    D3D12Descriptor shaderVisibleDescriptor = ((D3D12UnorderedAccessView*) pUAV)->GetShaderVisibleDescriptor();
    D3D12Descriptor nonShaderVisibleDescriptor = ((D3D12UnorderedAccessView*) pUAV)->GetNonShaderVisiableDescriptor();

    m_pCommandList->ClearUnorderedAccessViewFloat(shaderVisibleDescriptor.gpuHandle, nonShaderVisibleDescriptor.cpuHandle, 
        (ID3D12Resource*) pResource->GetHandle(), clearValue, 0, nullptr); 
    ++ m_commandCount;
}

void D3D12CommandList::ClearUAV(IRHIResource* pResource, IRHIDescriptor* pUAV, const uint32_t* clearValue)
{
    MY_ASSERT(pResource->IsTexture() || pResource->IsBuffer());

    FlushBarriers();
    
    D3D12Descriptor shaderVisibleDescriptor = ((D3D12UnorderedAccessView*) pUAV)->GetShaderVisibleDescriptor();
    D3D12Descriptor nonShaderVisibleDescriptor = ((D3D12UnorderedAccessView*) pUAV)->GetNonShaderVisiableDescriptor();

    m_pCommandList->ClearUnorderedAccessViewUint(shaderVisibleDescriptor.gpuHandle, nonShaderVisibleDescriptor.cpuHandle, 
        (ID3D12Resource*) pResource->GetHandle(), clearValue, 0, nullptr); 
    ++ m_commandCount;
}

void D3D12CommandList::WriteBuffer(IRHIBuffer* pBuffer, uint32_t offset, uint32_t data)
{
    FlushBarriers();
    
    D3D12_WRITEBUFFERIMMEDIATE_PARAMETER parameter;
    parameter.Dest = pBuffer->GetGPUAddress() + offset;
    parameter.Value = data;

    m_pCommandList->WriteBufferImmediate(1, &parameter, nullptr);
    ++ m_commandCount;
}

void D3D12CommandList::UpdateTileMappings(IRHITexture* pTexture, IRHIHeap* pHeap, uint32_t mappingCount, const RHITileMapping* mappings)
{
    eastl::vector<D3D12_TILED_RESOURCE_COORDINATE> coordinates;
    eastl::vector<D3D12_TILE_REGION_SIZE> sizes;
    eastl::vector<D3D12_TILE_RANGE_FLAGS> flags;
    eastl::vector<UINT> heapTileOffsets;
    eastl::vector<UINT> tileCounts;

    coordinates.reserve(mappingCount);
    sizes.reserve(mappingCount);
    flags.reserve(mappingCount);
    heapTileOffsets.reserve(mappingCount);
    tileCounts.reserve(mappingCount);

    for (uint32_t i = 0; i < mappingCount; ++i)
    {
        CD3DX12_TILED_RESOURCE_COORDINATE coordinate;
        coordinate.Subresource = mappings[i].m_subresource;
        coordinate.X = mappings[i].m_x;
        coordinate.Y = mappings[i].m_y;
        coordinate.Z = mappings[i].m_z;

        if (mappings[i].m_useBox)
        {
            MY_ASSERT(mappings[i].m_tileCount == mappings[i].m_width * mappings[i].m_height * mappings[i].m_depth);
        }

        D3D12_TILE_REGION_SIZE size;
        size.UseBox = mappings[i].m_useBox;
        size.NumTiles = mappings[i].m_tileCount;
        size.Width = mappings[i].m_width;
        size.Height = mappings[i].m_height;
        size.Depth = mappings[i].m_depth;

        D3D12_TILE_RANGE_FLAGS flag = mappings[i].m_type == RHITileMappingType::Map ? D3D12_TILE_RANGE_FLAG_NONE : D3D12_TILE_RANGE_FLAG_NULL;
        UINT tileCount = size.NumTiles;

        coordinates.push_back(coordinate);
        sizes.push_back(size);
        flags.push_back(flag);
        heapTileOffsets.push_back(mappings[i].m_heapOffset);
        tileCounts.push_back(tileCount);
    }

    m_pCommandQueue->UpdateTileMappings(
        (ID3D12Resource*) pTexture->GetHandle(),
        mappingCount,
        coordinates.data(),
        sizes.data(),
        ((D3D12Heap*) pHeap)->GetHeap(),
        mappingCount,
        flags.data(),
        heapTileOffsets.data(),
        tileCounts.data(),
        D3D12_TILE_MAPPING_FLAG_NONE);
}

void D3D12CommandList::TextureBarrier(IRHITexture* pTexture, uint32_t subresource, RHIAccessFlags accessBefore, RHIAccessFlags accessAfter)
{
    D3D12_TEXTURE_BARRIER barrier = {};
    barrier.SyncBefore = RHIToD3D12BarrierSync(accessBefore);
    barrier.SyncAfter = RHIToD3D12BarrierSync(accessAfter);
    barrier.AccessBefore = RHIToD3D12BarrierAccess(accessBefore);
    barrier.AccessAfter = RHIToD3D12BarrierAccess(accessAfter);
    barrier.LayoutBefore = RHIToD3D12BarrierLayout(accessBefore);
    barrier.LayoutAfter = RHIToD3D12BarrierLayout(accessAfter);
    barrier.pResource = (ID3D12Resource*) pTexture->GetHandle();
    barrier.Subresources = CD3DX12_BARRIER_SUBRESOURCE_RANGE(subresource);

    if (accessBefore & RHIAccessBit::RHIAccessDiscard)
    {
        barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_DISCARD;
    }

    m_textureBarriers.push_back(barrier);
}

void D3D12CommandList::BufferBarrier(IRHIBuffer* pBuffer, RHIAccessFlags accessBefore, RHIAccessFlags accessAfter)
{
    D3D12_BUFFER_BARRIER barrier = {};
    barrier.SyncBefore = RHIToD3D12BarrierSync(accessBefore);
    barrier.SyncAfter = RHIToD3D12BarrierSync(accessAfter);
    barrier.AccessBefore = RHIToD3D12BarrierAccess(accessBefore);
    barrier.AccessAfter = RHIToD3D12BarrierAccess(accessAfter);
    barrier.pResource = (ID3D12Resource*) pBuffer->GetHandle();
    barrier.Offset = 0;
    barrier.Size = UINT64_MAX;

    m_bufferBarriers.push_back(barrier);
}

void D3D12CommandList::GlobalBarrier(RHIAccessFlags accessBefore, RHIAccessFlags accessAfter)
{
    D3D12_GLOBAL_BARRIER barrier = {};
    barrier.SyncBefore = RHIToD3D12BarrierSync(accessBefore);
    barrier.SyncAfter = RHIToD3D12BarrierSync(accessAfter);
    barrier.AccessBefore = RHIToD3D12BarrierAccess(accessBefore);
    barrier.AccessAfter = RHIToD3D12BarrierAccess(accessAfter);

    m_globalBarriers.push_back(barrier);
}

void D3D12CommandList::FlushBarriers()
{
    eastl::vector<D3D12_BARRIER_GROUP> barrierGroup;
    if (!m_textureBarriers.empty())
    {
        barrierGroup.push_back(CD3DX12_BARRIER_GROUP((UINT32) m_textureBarriers.size(), m_textureBarriers.data()));
    }

    if (!m_bufferBarriers.empty())
    {
        barrierGroup.push_back(CD3DX12_BARRIER_GROUP((UINT32) m_bufferBarriers.size(), m_bufferBarriers.data()));
    }

    if (!m_globalBarriers.empty())
    {
        barrierGroup.push_back(CD3DX12_BARRIER_GROUP((UINT32) m_globalBarriers.size(), m_globalBarriers.data()));
    }

    if (!barrierGroup.empty())
    {
        ++ m_commandCount;
        m_pCommandList->Barrier((UINT32)barrierGroup.size(), barrierGroup.data());
    }

    m_textureBarriers.clear();
    m_bufferBarriers.clear();
    m_globalBarriers.clear();
}

void D3D12CommandList::BeginRenderPass(const RHIRenderPassDesc& renderPass)
{
    FlushBarriers();

    D3D12_RENDER_PASS_RENDER_TARGET_DESC rtDesc[RHI_MAX_RENDER_TARGET_ACCOUNT] = {};
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dsDesc = {};
    uint32_t flags = D3D12_RENDER_PASS_FLAG_NONE;

    uint32_t width = 0;
    uint32_t height = 0;

    unsigned int rtCount = 0;
    const IRHITexture* pTexture = nullptr;
    for (int i = 0; i < RHI_MAX_RENDER_TARGET_ACCOUNT; ++i)
    {
        pTexture = renderPass.m_color[i].m_pTexture;
        if (pTexture == nullptr)
        {
            continue;
        }

        if (width == 0)
        {
            width = pTexture->GetDesc().m_width;
        }

        if (height == 0)
        {
            height = pTexture->GetDesc().m_height;
        }

        MY_ASSERT(width == pTexture->GetDesc().m_width);
        MY_ASSERT(height == pTexture->GetDesc().m_height);

        rtDesc[i].cpuDescriptor = ((D3D12Texture*) pTexture)->GetRTV(renderPass.m_color[i].m_mipSlice, renderPass.m_color[i].m_arraySlice);
        rtDesc[i].BeginningAccess.Type = RHIToD3D12RenderPassLoadOp(renderPass.m_color[i].m_loadOp);
        rtDesc[i].BeginningAccess.Clear.ClearValue.Format = DXGIFormat(pTexture->GetDesc().m_format);
        memcpy(rtDesc[i].BeginningAccess.Clear.ClearValue.Color, renderPass.m_color[i].m_clearColor, sizeof(float) * 4);  //< RGBA four components
        rtDesc[i].EndingAccess.Type = RHIToD3D12RenderPassStoreOp(renderPass.m_color[i].m_storeOp);
        ++ rtCount;        
    }

    if (renderPass.m_depth.m_pTexture != nullptr)
    {
        pTexture = renderPass.m_depth.m_pTexture;
        if (width == 0)
        {
            width = pTexture->GetDesc().m_width;
        }

        if (height == 0)
        {
            height = pTexture->GetDesc().m_height;
        }

        MY_ASSERT(width == pTexture->GetDesc().m_width);
        MY_ASSERT(height == pTexture->GetDesc().m_height);

        if (renderPass.m_depth.m_readOnly)
        {
            dsDesc.cpuDescriptor = ((D3D12Texture*) pTexture)->GetReadOnlyDSV(renderPass.m_depth.m_mipSlice, renderPass.m_depth.m_arraySlice);

            flags |= D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_DEPTH;
            if (IsStencilFormat(pTexture->GetDesc().m_format))
            {
                flags |= D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_STENCIL;
            }
        }
        else
        {
            dsDesc.cpuDescriptor = ((D3D12Texture*) pTexture)->GetDSV(renderPass.m_depth.m_mipSlice, renderPass.m_depth.m_arraySlice);
        }

        dsDesc.DepthBeginningAccess.Type = RHIToD3D12RenderPassLoadOp(renderPass.m_depth.m_loadOp);
        dsDesc.DepthBeginningAccess.Clear.ClearValue.Format = DXGIFormat(pTexture->GetDesc().m_format);
        dsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = renderPass.m_depth.m_clearDepth;
        dsDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = renderPass.m_depth.m_clearStencil;
        dsDesc.DepthEndingAccess.Type = RHIToD3D12RenderPassStoreOp(renderPass.m_depth.m_storeOp);
        
        if (IsStencilFormat(pTexture->GetDesc().m_format))
        {
            dsDesc.StencilBeginningAccess.Type = RHIToD3D12RenderPassLoadOp(renderPass.m_depth.m_stencilLoadOp);
            dsDesc.StencilBeginningAccess.Clear.ClearValue.Format = DXGIFormat(pTexture->GetDesc().m_format);
            dsDesc.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Depth = renderPass.m_depth.m_clearDepth;
            dsDesc.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = renderPass.m_depth.m_clearStencil;
            dsDesc.StencilEndingAccess.Type = RHIToD3D12RenderPassStoreOp(renderPass.m_depth.m_stencilStoreOp);
        }
        else
        {
            dsDesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
            dsDesc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
        }
    }

    m_pCommandList->BeginRenderPass(rtCount, rtDesc, renderPass.m_depth.m_pTexture != nullptr ? &dsDesc : nullptr, (D3D12_RENDER_PASS_FLAGS) flags);
    ++m_commandCount;

    SetViewport(0, 0, width, height);
}

void D3D12CommandList::EndRenderPass()
{
    m_pCommandList->EndRenderPass();
    ++ m_commandCount;
}

void D3D12CommandList::SetPipelineState(IRHIPipelineState* state)
{
    if (m_pCurrentPSO != state)
    {
        m_pCurrentPSO = state;
        m_pCommandList->SetPipelineState((ID3D12PipelineState*) state->GetHandle());

        if (state->GetType() == RHIPipelineType::Graphics)
        {
            m_pCommandList->IASetPrimitiveTopology(((D3D12GraphicsPipelineState*) state)->GetPrimitiveTopology());
        }
    }
}

void D3D12CommandList::SetStencilReference(uint8_t stencil)
{
    m_pCommandList->OMSetStencilRef(stencil);
}

void D3D12CommandList::SetBlendFactor(const float* blendFactor)
{
    m_pCommandList->OMSetBlendFactor(blendFactor);
}

void D3D12CommandList::SetIndexBuffer(IRHIBuffer* pBuffer, uint32_t offset, RHIFormat format)
{
    MY_ASSERT(format == RHIFormat::R16UI || format == RHIFormat::R32UI);

    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    indexBufferView.BufferLocation = pBuffer->GetGPUAddress() + offset;
    indexBufferView.SizeInBytes = pBuffer->GetDesc().m_size - offset;
    indexBufferView.Format = DXGIFormat(format);

    m_pCommandList->IASetIndexBuffer(&indexBufferView);
}

void D3D12CommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    D3D12_VIEWPORT vp = { (float)x, (float)y, (float)width, (float)height, 0.0f, 1.0f };
    m_pCommandList->RSSetViewports(1, &vp);
    SetScissorRect(x, y, width, height);
}

void D3D12CommandList::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    D3D12_RECT rect = { (LONG)x, (LONG)y, (LONG)x + (LONG)width, (LONG)y + (LONG)height };
    m_pCommandList->RSSetScissorRects(1, &rect);
}

void D3D12CommandList::SetGraphicsConstants(uint32_t slot, const void* data, size_t dataSize)
{
    if (slot == 0)
    {
        uint32_t constsCount = (uint32_t) dataSize / sizeof(uint32_t);
        MY_ASSERT(constsCount <= RHI_MAX_ROOT_CONSTATNS);
        m_pCommandList->SetGraphicsRoot32BitConstants(0, constsCount, data, 0);
    }
    else
    {
        MY_ASSERT(slot < RHI_MAX_CBV_BINDINGS);
        D3D12_GPU_VIRTUAL_ADDRESS address = ((D3D12Device*) m_pDevice)->AllocateConstantBuffer(data, dataSize);
        MY_ASSERT(address);
        m_pCommandList->SetGraphicsRootConstantBufferView(slot, address);
    }
}

void D3D12CommandList::SetComputeConstants(uint32_t slot, const void* data, size_t dataSize)
{
    if (slot == 0)
    {
        uint32_t constsCount = (uint32_t) dataSize / sizeof(uint32_t);
        MY_ASSERT(constsCount <= RHI_MAX_ROOT_CONSTATNS);
        m_pCommandList->SetComputeRoot32BitConstants(0, constsCount, data, 0);
    }
    else
    {
        MY_ASSERT(slot < RHI_MAX_CBV_BINDINGS);
        D3D12_GPU_VIRTUAL_ADDRESS address = ((D3D12Device*) m_pDevice)->AllocateConstantBuffer(data, dataSize);
        MY_ASSERT(address);
        m_pCommandList->SetComputeRootConstantBufferView(slot, address);
    }
}

void D3D12CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount)
{
    m_pCommandList->DrawInstanced(vertexCount, instanceCount, 0, 0);
    ++ m_commandCount;
}

void D3D12CommandList::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t indexOffset)
{
    m_pCommandList->DrawIndexedInstanced(indexCount, instanceCount, indexOffset, 0, 0);
    ++ m_commandCount;
}

void D3D12CommandList::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    FlushBarriers();

    m_pCommandList->Dispatch(groupCountX, groupCountY, groupCountZ);
    ++ m_commandCount;
}

void D3D12CommandList::DispatchMesh(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    m_pCommandList->DispatchMesh(groupCountX, groupCountY, groupCountZ);
    ++ m_commandCount;
}

void D3D12CommandList::DrawIndirect(IRHIBuffer* pBuffer, uint32_t offset)
{
    ID3D12CommandSignature* pSignature = ((D3D12Device*) m_pDevice)->GetDrawSignature();
    m_pCommandList->ExecuteIndirect(pSignature, 1, (ID3D12Resource*) pBuffer->GetHandle(), offset, nullptr, 0);
    ++ m_commandCount;
}

void D3D12CommandList::DrawIndexedIndirect(IRHIBuffer* pBuffer, uint32_t offset)
{
    ID3D12CommandSignature* pSignature = ((D3D12Device*) m_pDevice)->GetDrawIndexedSignature();
    m_pCommandList->ExecuteIndirect(pSignature, 1, (ID3D12Resource*)pBuffer->GetHandle(), offset, nullptr, 0);
    ++ m_commandCount;
}

void D3D12CommandList::DispatchIndirect(IRHIBuffer* pBuffer, uint32_t offset)
{
    FlushBarriers();

    ID3D12CommandSignature* pSignature = ((D3D12Device*) m_pDevice)->GetDispatchSignature();
    m_pCommandList->ExecuteIndirect(pSignature, 1, (ID3D12Resource*) pBuffer->GetHandle(), offset, nullptr, 0);
    ++ m_commandCount;
}

void D3D12CommandList::DispatchMeshIndirect(IRHIBuffer* pBuffer, uint32_t offset)
{
    ID3D12CommandSignature* pSignature = ((D3D12Device*) m_pDevice)->GetDispatchMeshSignature();
    m_pCommandList->ExecuteIndirect(pSignature, 1, (ID3D12Resource*) pBuffer->GetHandle(), offset, nullptr, 0);
    ++ m_commandCount;
}

void D3D12CommandList::MultiDrawIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset)
{
    ID3D12CommandSignature* pSignature = ((D3D12Device*) m_pDevice)->GetMultiDrawSignature();
    m_pCommandList->ExecuteIndirect(pSignature, maxCount, (ID3D12Resource*) pArgsBuffer->GetHandle(), argsBufferOffset,
        (ID3D12Resource*) pCountBuffer->GetHandle(), countBufferOffset);
    ++ m_commandCount;
}

void D3D12CommandList::MultiDrawIndexedIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset)
{
    ID3D12CommandSignature* pSignature = ((D3D12Device*) m_pDevice)->GetMultiDrawIndexedSignature();
    m_pCommandList->ExecuteIndirect(pSignature, maxCount, (ID3D12Resource*) pArgsBuffer->GetHandle(), argsBufferOffset,
        (ID3D12Resource*) pCountBuffer->GetHandle(), countBufferOffset);
    ++ m_commandCount;
}

void D3D12CommandList::MultiDispatchIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset)
{
    ID3D12CommandSignature* pSignature = ((D3D12Device*) m_pDevice)->GetMultiDispatchSignature();
    m_pCommandList->ExecuteIndirect(pSignature, maxCount, (ID3D12Resource*) pArgsBuffer->GetHandle(), argsBufferOffset,
        (ID3D12Resource*) pCountBuffer->GetHandle(), countBufferOffset);
    ++ m_commandCount;
}

void D3D12CommandList::MultiDispatchMeshIndirect(uint32_t maxCount, IRHIBuffer* pArgsBuffer, uint32_t argsBufferOffset, IRHIBuffer* pCountBuffer, uint32_t countBufferOffset)
{
    ID3D12CommandSignature* pSignature = ((D3D12Device*) m_pDevice)->GetMultiDispatchMeshSignature();
    m_pCommandList->ExecuteIndirect(pSignature, maxCount, (ID3D12Resource*) pArgsBuffer->GetHandle(), argsBufferOffset,
        (ID3D12Resource*) pCountBuffer->GetHandle(), countBufferOffset);
    ++ m_commandCount;
}

void D3D12CommandList::BuildRayTracingBLAS(IRHIRayTracingBLAS* pBLAS)
{
    FlushBarriers();

    m_pCommandList->BuildRaytracingAccelerationStructure(((D3D12RayTracingBLAS*) pBLAS)->GetBuildDesc(), 0, nullptr);
    ++ m_commandCount;
}

void D3D12CommandList::UpdateRayTracingBLAS(IRHIRayTracingBLAS* pBLAS, IRHIBuffer* pVertexBuffer, uint32_t vertexBufferOffset)
{
    FlushBarriers();

    D3D12_RAYTRACING_GEOMETRY_DESC geometry;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc;
    ((D3D12RayTracingBLAS*) pBLAS)->GetUpdateDesc(desc, geometry, pVertexBuffer, vertexBufferOffset);

    m_pCommandList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
    ++ m_commandCount;
}

void D3D12CommandList::BuildRayTracingTLAS(IRHIRayTracingTLAS* pTLAS, const RHIRayTracingInstance* pInstances, uint32_t instanceCount)
{
    FlushBarriers();
    
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    ((D3D12RayTracingTLAS*) pTLAS)->GetBuildDesc(desc, pInstances, instanceCount);
    m_pCommandList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
    ++ m_commandCount;
}

#if MICROPROFILE_GPU_TIMERS
MicroProfileThreadLogGpu* D3D12CommandList::GetProfileLog() const
{
#if MICROPROFILE_GPU_TIMERS_D3D12
    return m_pProfileLog;
#endif
    return nullptr;
}
#endif