#include "RenderGraphResource.h"
#include "RenderGraph.h"

void RenderGraphResource::Resolve(RenderGraphEdge* pEdge, RenderGraphPassBase* pPass)
{
    if (pPass->GetID() >= m_lastPass)
    {
        m_lastState = pEdge->GetUsage();        
    }

    m_firstPass = eastl::min(m_firstPass, pPass->GetID());
    m_lastPass = eastl::max(m_lastPass, pPass->GetID());

    // For resources used in async compute, we should extent its life time range
    if(pPass->GetType() == RenderPassType::AsyncCompute)
    {
        m_firstPass = eastl::min(m_firstPass, pPass->GetWaitGraphicsPassID());
        m_lastPass = eastl::min(m_lastPass, pPass->GetSignalGraphicsPassID());
    }
}

RGTexture::RGTexture(RenderGraphResourceAllocator& allocator, const eastl::string& name, const Desc& desc) :
    RenderGraphResource(name),
    m_allocator(allocator)
{
    m_desc = desc;
}

RGTexture::RGTexture(RenderGraphResourceAllocator& allocator, IRHITexture* pTexture, RHIAccessFlags state) :
    RenderGraphResource(pTexture->GetName()),
    m_allocator(allocator)
{
    m_desc = pTexture->GetDesc();
    m_pTexture = pTexture;
    m_initialState = state;
    m_imported = true;
}

RGTexture::~RGTexture()
{
    if (!m_imported)
    {
        if (m_output)
        {
            m_allocator.FreeNonOverlappingTexture(m_pTexture, m_lastState);
        }
        else
        {
            m_allocator.Free(m_pTexture, m_lastState, m_output);
        }
    }
}

IRHIDescriptor* RGTexture::GetSRV()
{
    MY_ASSERT(!IsImported());
    
    RHIShaderResourceViewDesc desc;
    desc.m_format = m_pTexture->GetDesc().m_format;

    return m_allocator.GetDesciptor(m_pTexture, desc);
}

IRHIDescriptor* RGTexture::GetUAV()
{
    MY_ASSERT(!IsImported());

    RHIUnorderedAccessViewDesc desc;
    desc.m_format = m_pTexture->GetDesc().m_format;
    
    return m_allocator.GetDesciptor(m_pTexture, desc);
}

IRHIDescriptor* RGTexture::GetUAV(uint32_t mip, uint32_t arraySlice)
{
    MY_ASSERT(!IsImported());
    
    RHIUnorderedAccessViewDesc desc;
    desc.m_format = m_pTexture->GetDesc().m_format;
    desc.m_texture.m_mipSlice = mip;
    desc.m_texture.m_arraySlice = arraySlice;
    
    return m_allocator.GetDesciptor(m_pTexture, desc);
}

void RGTexture::Resolve(RenderGraphEdge* pEdge, RenderGraphPassBase* pPass)
{
    RenderGraphResource::Resolve(pEdge, pPass);

    RHIAccessFlags usage = pEdge->GetUsage();
    if (usage & RHIAccessBit::RHIAccessRTV)
    {
        m_desc.m_usage |= RHITextureUsageBit::RHITextureUsageRenderTarget;
    }

    if (usage & RHIAccessBit::RHIAccessMaskUAV)
    {
        m_desc.m_usage |= RHITextureUsageBit::RHITextureUsageUnorderedAccess;
    }

    if (usage & (RHIAccessBit::RHIAccessDSV | RHIAccessBit::RHIAccessDSVReadOnly))
    {
        m_desc.m_usage |= RHITextureUsageBit::RHITextureUsageDepthStencil;
    }
}

void RGTexture::Realize()
{
    if (!m_imported)
    {
        if (m_output)
        {
            m_pTexture = m_allocator.AllocateNonOverlappingTexture(m_desc, m_name, m_initialState);
        }
        else
        {
            m_pTexture = m_allocator.AllocateTexture(m_firstPass, m_lastPass, m_lastState, m_desc, m_name, m_initialState);
        }
    }
}

void RGTexture::Barrier(IRHICommandList* pCommandList, uint32_t subresource, RHIAccessFlags accessBefore, RHIAccessFlags accessAfter)
{
    pCommandList->TextureBarrier(m_pTexture, subresource, accessBefore, accessAfter);
}

IRHIResource* RGTexture::GetAliasedPrevResource(RHIAccessFlags& lastUsedState)
{
    return m_allocator.GetAliasedPrevResource(m_pTexture, m_firstPass, lastUsedState);
}

RGBuffer::RGBuffer(RenderGraphResourceAllocator& allocator, const eastl::string& name, const Desc& desc) :
    RenderGraphResource(name),
    m_allocator(allocator)
{
    m_desc = desc;
}

RGBuffer::RGBuffer(RenderGraphResourceAllocator& allocator, IRHIBuffer* pBuffer, RHIAccessFlags state) :
    RenderGraphResource(pBuffer->GetName()),
    m_allocator(allocator)
{
    m_desc = pBuffer->GetDesc();
    m_pBuffer = pBuffer;
    m_initialState = state;
    m_imported = true;
}

RGBuffer::~RGBuffer()
{
    if (!m_imported)
    {
        m_allocator.Free(m_pBuffer, m_lastState, m_output);
    }
}

IRHIDescriptor* RGBuffer::GetSRV()
{
    MY_ASSERT(!IsImported());

    const RHIBufferDesc& bufferDesc = m_pBuffer->GetDesc();

    RHIShaderResourceViewDesc desc;
    desc.m_format = bufferDesc.m_format;

    if (bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageStructedBuffer)
    {
        desc.m_type = RHIShaderResourceViewType::StructuredBuffer;
    }
    else if (bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageTypedBuffer)
    {
        desc.m_type = RHIShaderResourceViewType::TypedBuffer;
    }
    else if (bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageRawBuffer)
    {
        desc.m_type = RHIShaderResourceViewType::RawBuffer;
    }

    desc.m_buffer.m_offset = 0;
    desc.m_buffer.m_size = bufferDesc.m_size;
    return m_allocator.GetDesciptor(m_pBuffer, desc);
}

IRHIDescriptor* RGBuffer::GetUAV()
{
    MY_ASSERT(!IsImported());

    const RHIBufferDesc& bufferDesc = m_pBuffer->GetDesc();
    MY_ASSERT(bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageUnorderedAccess);

    RHIUnorderedAccessViewDesc desc;
    desc.m_format = bufferDesc.m_format;

    if (bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageStructedBuffer)
    {
        desc.m_type = RHIUnorderedAccessViewType::StructuredBuffer;
    }
    else if (bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageTypedBuffer)
    {
        desc.m_type = RHIUnorderedAccessViewType::TypedBuffer;
    }
    else if (bufferDesc.m_usage & RHIBufferUsageBit::RHIBufferUsageRawBuffer)
    {
        desc.m_type = RHIUnorderedAccessViewType::RawBuffer;
    }

    desc.m_buffer.m_offset = 0;
    desc.m_buffer.m_size = bufferDesc.m_size;
    return m_allocator.GetDesciptor(m_pBuffer, desc);
}

void RGBuffer::Resolve(RenderGraphEdge* pEdge, RenderGraphPassBase* pPass)
{
    RenderGraphResource::Resolve(pEdge, pPass);
    if (pEdge->GetUsage() & RHIAccessBit::RHIAccessMaskUAV)
    {
        m_desc.m_usage |= RHIBufferUsageBit::RHIBufferUsageUnorderedAccess;
    }
}

void RGBuffer::Realize()
{
    if (!m_imported)
    {
        m_pBuffer = m_allocator.AllocateBuffer(m_firstPass, m_lastPass, m_lastState, m_desc, m_name, m_initialState);
    }
}

void RGBuffer::Barrier(IRHICommandList* pCommandList, uint32_t subresource, RHIAccessFlags accessBefore, RHIAccessFlags accessAfter)
{
    pCommandList->BufferBarrier(m_pBuffer, accessBefore, accessAfter);
}

IRHIResource* RGBuffer::GetAliasedPrevResource(RHIAccessFlags& lastUsedState)
{
    return m_allocator.GetAliasedPrevResource(m_pBuffer, m_firstPass, lastUsedState);
}