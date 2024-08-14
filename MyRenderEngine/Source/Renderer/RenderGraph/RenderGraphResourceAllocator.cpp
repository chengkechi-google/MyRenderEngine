#include "RenderGraphResourceAllocator.h"
#include "Utils/math.h"
#include "Utils/fmt.h"

RenderGraphResourceAllocator::RenderGraphResourceAllocator(IRHIDevice* pDevice)
{
    m_pDevice = pDevice;
}

RenderGraphResourceAllocator::~RenderGraphResourceAllocator()
{
    for (auto iter = m_allocatedHeaps.begin(); iter != m_allocatedHeaps.end(); ++iter)
    {
        const Heap& heap = *iter;
        
        for (size_t i = 0; i < heap.m_resources.size(); ++i)
        {
            DeleteDescroptor(heap.m_resources[i].m_pResource);
            delete heap.m_resources[i].m_pResource;
        }

        delete heap.m_heap;
    }

    for (auto iter = m_freeOverlappingTextures.begin(); iter != m_freeOverlappingTextures.end(); ++iter)
    {
        DeleteDescroptor(iter->m_pTexture);
        delete iter->m_pTexture;
    }
}

void RenderGraphResourceAllocator::Reset()
{
    // Possible O(n^2)
    for (auto iter = m_allocatedHeaps.begin(); iter != m_allocatedHeaps.end();)
    {
        Heap& heap = *iter;
        CheckHeapUsage(heap);

        if (heap.m_resources.empty())
        {
            delete heap.m_heap;
            iter = m_allocatedHeaps.erase(iter);
        }
        else
        {
            ++ iter;
        }
    }

    // Possible O(n^2)
    uint64_t currentFrame = m_pDevice->GetFrameID();
    for (auto iter = m_freeOverlappingTextures.begin(); iter != m_freeOverlappingTextures.end(); )
    {
        // If over 30 frames, delete the texture
        if (currentFrame - iter->m_lastUsedFrame > 30)
        {
            DeleteDescroptor(iter->m_pTexture);
            delete iter->m_pTexture;
            iter = m_freeOverlappingTextures.erase(iter);
        }
        else
        {
            ++ iter;
        }
    }
}

IRHITexture* RenderGraphResourceAllocator::AllocateNonOverlappingTexture(const RHITextureDesc& desc, const eastl::string& name, RHIAccessFlags& initialState)
{
    for (auto iter = m_freeOverlappingTextures.begin(); iter != m_freeOverlappingTextures.end(); ++iter)
    {
        IRHITexture* texture = iter->m_pTexture;
        if (texture->GetDesc() == desc)
        {
            initialState = iter->m_lastUsedState;
            m_freeOverlappingTextures.erase(iter);
            return texture;
        }
    }

    if (IsDepthFormat(desc.m_format))
    {
        initialState = RHIAccessBit::RHIAccessDSV;
    }
    else if (desc.m_usage & RHITextureUsageBit::RHITextureUsageRenderTarget)
    {
        initialState = RHIAccessBit::RHIAccessRTV;
    }
    else if (desc.m_usage & RHITextureUsageBit::RHITextureUsageUnorderedAccess)
    {
        initialState = RHIAccessBit::RHIAccessMaskUAV;
    }

    return m_pDevice->CreateTexture(desc, "RGTexture " + name);
}

void RenderGraphResourceAllocator::FreeNonOverlappingTexture(IRHITexture* texture, RHIAccessFlags state)
{
    if (texture != nullptr)
    {
        m_freeOverlappingTextures.push_back({ texture, state, m_pDevice->GetFrameID() });
    }
}

IRHITexture* RenderGraphResourceAllocator::AllocateTexture(uint32_t firstPass, uint32_t lastPass, RHIAccessFlags lastState, const RHITextureDesc& desc,
    const eastl::string& name, RHIAccessFlags& initialState)
{
    LifeTimeRange lifeTime = { firstPass, lastPass };
    uint32_t textureSize = m_pDevice->GetAllocationSize(desc);

    // If we can re use any heap that already allocated and bigger than this texture size
    for (size_t i = 0; i < m_allocatedHeaps.size(); ++i)
    {
        Heap& heap = m_allocatedHeaps[i];
        if (heap.m_heap->GetDesc().m_size < textureSize || heap.IsOverlapping(lifeTime))
        {
            continue;
        }

        for (size_t j = 0; j < heap.m_resources.size(); ++j)
        {
            AliasedResource aliasResource = heap.m_resources[j];
            if (aliasResource.m_pResource->IsTexture() && !aliasResource.m_lifeTime.IsUsed() && ((IRHITexture*)aliasResource.m_pResource)->GetDesc() == desc)
            {
                aliasResource.m_lifeTime = lifeTime;
                initialState = aliasResource.m_lastUsedState;
                aliasResource.m_lastUsedState = lastState;
                return (IRHITexture*) aliasResource.m_pResource;
            }
        }

        RHITextureDesc newDesc = desc;
        newDesc.m_heap = heap.m_heap;

        AliasedResource aliasedTexture;
        aliasedTexture.m_pResource = m_pDevice->CreateTexture(newDesc, "RGTexture" + name);
        aliasedTexture.m_lifeTime = lifeTime;
        aliasedTexture.m_lastUsedState = lastState;
        heap.m_resources.push_back(aliasedTexture);

        if (IsDepthFormat(desc.m_format))
        {
            initialState = RHIAccessBit::RHIAccessDSV;
        }
        else if (desc.m_usage & RHITextureUsageBit::RHITextureUsageRenderTarget)
        {
            initialState = RHIAccessBit::RHIAccessRTV;
        }
        else if (desc.m_usage & RHITextureUsageBit::RHITextureUsageUnorderedAccess)
        {
            initialState = RHIAccessBit::RHIAccessRTV;
        }

        MY_ASSERT(aliasedTexture.m_pResource != nullptr);
        return (IRHITexture*) aliasedTexture.m_pResource;
    }

    // Allocate new heap, not aliased
    AllocateHeap(textureSize);
    return AllocateTexture(firstPass, lastPass, lastState, desc, name, initialState);   
}

IRHIBuffer* RenderGraphResourceAllocator::AllocateBuffer(uint32_t firstPass, uint32_t lastPass, RHIAccessFlags lastState, const RHIBufferDesc& desc,
    const eastl::string& name, RHIAccessFlags& initialState)
{
    LifeTimeRange lifeTime = { firstPass, lastPass };
    uint32_t bufferSize = desc.m_size;
    
    for (size_t i = 0; i < m_allocatedHeaps.size(); ++i)
    {
        Heap& heap = m_allocatedHeaps[i];
        if (heap.m_heap->GetDesc().m_size < bufferSize || heap.IsOverlapping(lifeTime))
        {
            continue;
        }

        for (size_t j = 0; j < heap.m_resources.size(); ++j)
        {
            AliasedResource& aliasedResource = heap.m_resources[j];
            if (aliasedResource.m_pResource->IsBuffer() && !aliasedResource.m_lifeTime.IsUsed() && ((IRHIBuffer*)aliasedResource.m_pResource)->GetDesc() == desc)
            {
                aliasedResource.m_lifeTime = lifeTime;
                initialState = aliasedResource.m_lastUsedState;
                aliasedResource.m_lastUsedFrame = lastState;
                return (IRHIBuffer*)aliasedResource.m_pResource;
            }
        }

        RHIBufferDesc newDesc = desc;
        newDesc.m_pHeap = heap.m_heap;
    
        AliasedResource aliasedBuffer;
        aliasedBuffer.m_pResource = m_pDevice->CreateBuffer(newDesc, "RGBuffer " + name);
        aliasedBuffer.m_lifeTime = lifeTime;
        aliasedBuffer.m_lastUsedState = lastState;
        heap.m_resources.push_back(aliasedBuffer);
        
        initialState = RHIAccessDiscard;

        MY_ASSERT(aliasedBuffer.m_pResource != nullptr);
        return (IRHIBuffer*) aliasedBuffer.m_pResource;        
    }
    
    AllocateHeap(bufferSize);
    return AllocateBuffer(firstPass, lastPass, lastState, desc, name, initialState);
}

void RenderGraphResourceAllocator::Free(IRHIResource* pResource, RHIAccessFlags state, bool setState)
{
    if (pResource != nullptr)
    {
        // O(n^2) should find better to handle this
        for (size_t i = 0; i < m_allocatedHeaps.size(); ++i)
        {
            Heap& heap = m_allocatedHeaps[i];
            
            for (size_t j = 0; j < heap.m_resources.size(); ++j)
            {
                AliasedResource& aliasedResource = heap.m_resources[j];
                if (aliasedResource.m_pResource == pResource)
                {
                    aliasedResource.m_lifeTime.Reset();
                    aliasedResource.m_lastUsedFrame = m_pDevice->GetFrameID();
                    if (setState)
                    {
                        aliasedResource.m_lastUsedState = state;
                    }

                    return;
                }
            }
        }

        MY_ASSERT(false);   //< Doesn't find the resource to delete
    }
}

IRHIResource* RenderGraphResourceAllocator::GetAliasedPrevResource(IRHIResource* pResource, uint32_t firstPass, RHIAccessFlags& lastUsedState)
{
    for (size_t i = 0; i < m_allocatedHeaps.size(); ++i)
    {
        Heap& heap = m_allocatedHeaps[i];
        if (!heap.Contains(pResource))
        {
            continue;
        }

        AliasedResource* preAliasedResource = nullptr;
        IRHIResource* pPreResource = nullptr;
        uint32_t preResourceLastPass = 0;

        for (size_t j = 0; j < heap.m_resources.size(); ++j)
        {
            AliasedResource& aliasedResource = heap.m_resources[j];
            if (aliasedResource.m_pResource != pResource
                && aliasedResource.m_lifeTime.m_lastPass < firstPass
                && aliasedResource.m_lifeTime.m_lastPass > preResourceLastPass)
            {
                preAliasedResource = &aliasedResource;
                pPreResource = aliasedResource.m_pResource;
                lastUsedState = aliasedResource.m_lastUsedState;

                preResourceLastPass = aliasedResource.m_lifeTime.m_lastPass;
            }
        }

        if (preAliasedResource)
        {
            preAliasedResource->m_lastUsedState |= RHIAccessBit::RHIAccessDiscard;
        }

        return pPreResource;
    }

    MY_ASSERT(false);   //< Can not find previous reosurce
    return nullptr;
}

IRHIDescriptor* RenderGraphResourceAllocator::GetDesciptor(IRHIResource* pResource, const RHIShaderResourceViewDesc& desc)
{
    for (size_t i = 0; i < m_allocatedSRVs.size(); ++i)
    {
        if (m_allocatedSRVs[i].m_pResource == pResource
            && m_allocatedSRVs[i].m_desc == desc)
        {
            return m_allocatedSRVs[i].m_pDescriptor;
        }
    }

    IRHIDescriptor* srv = m_pDevice->CreateShaderResourceView(pResource, desc, pResource->GetName());
    m_allocatedSRVs.push_back({ pResource, srv, desc });
    return srv;
}

IRHIDescriptor* RenderGraphResourceAllocator::GetDesciptor(IRHIResource* pResource, const RHIUnorderedAccessViewDesc& desc)
{
    for (size_t i = 0; i < m_allocatedUAVs.size(); ++i)
    {
        if (m_allocatedUAVs[i].m_pResource == pResource
            && m_allocatedUAVs[i].m_desc == desc)
        {
            return m_allocatedUAVs[i].m_pDescriptor;
        }
    }

    IRHIDescriptor* uav = m_pDevice->CreateUnorderedAccessView(pResource, desc, pResource->GetName());
    m_allocatedUAVs.push_back({ pResource, uav, desc });
    return uav;
}

void RenderGraphResourceAllocator::CheckHeapUsage(Heap& heap)
{
    uint64_t currentFrame = m_pDevice->GetFrameID();
    for (auto iter = heap.m_resources.begin(); iter != heap.m_resources.end();)
    {
        const AliasedResource aliasedResource = *iter;
        if (currentFrame - aliasedResource.m_lastUsedFrame > 30)
        {
            DeleteDescroptor(aliasedResource.m_pResource);
            delete aliasedResource.m_pResource;
            iter = heap.m_resources.erase(iter);
        }
        else
        {
            ++ iter;
        }
    }
}

void RenderGraphResourceAllocator::DeleteDescroptor(IRHIResource* pResource)
{
    for (auto iter = m_allocatedSRVs.begin(); iter != m_allocatedSRVs.end(); )
    {
        if (iter->m_pResource == pResource)
        {
            delete iter->m_pDescriptor;
            iter = m_allocatedSRVs.erase(iter);
        }
        else
        {
            ++ iter;
        }
    }

    for (auto iter = m_allocatedUAVs.begin(); iter != m_allocatedUAVs.end(); )
    {
        if (iter->m_pResource == pResource)
        {
            delete iter->m_pDescriptor;
            iter = m_allocatedUAVs.erase(iter);
        }
        else
        {
            ++ iter;
        }
    }
}

void RenderGraphResourceAllocator::AllocateHeap(uint32_t size)
{
    RHIHeapDesc heapDesc;
    heapDesc.m_size = RoundUpPow2(size, 64 * 1024);     // Round to 64kb

    eastl::string heapName = fmt::format("RG Heap[ {:.1f} MB", heapDesc.m_size / (1024.0f * 1024.0f)).c_str();

    Heap heap;
    heap.m_heap = m_pDevice->CreatHeap(heapDesc, heapName);
    m_allocatedHeaps.push_back(heap);   
}