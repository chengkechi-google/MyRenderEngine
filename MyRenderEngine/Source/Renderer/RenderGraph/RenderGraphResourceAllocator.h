#pragma once
#include "RHI/RHI.h"

class RenderGraphResourceAllocator
{
    struct LifeTimeRange
    {
        uint32_t m_firstPass = UINT32_MAX;
        uint32_t m_lastPass = 0;

        void Reset() { m_firstPass = UINT32_MAX; m_lastPass = 0; }
        bool IsUsed() const { return m_firstPass != UINT32_MAX; }
        bool IsOverlapping(const LifeTimeRange& other) const
        {
            if (IsUsed())
            {
                return m_firstPass <= other.m_lastPass && m_lastPass >= other.m_firstPass;
            }
            else
            {
                return false;
            }
        }
    };

    struct AliasedResource
    {
        IRHIResource* m_pResource;
        LifeTimeRange m_lifeTime;
        uint64_t m_lastUsedFrame = 0;
        RHIAccessFlags m_lastUsedState = RHIAccessDiscard;
    };

    struct Heap
    {
        IRHIHeap* m_heap;
        eastl::vector<AliasedResource> m_resources;
        bool IsOverlapping(const LifeTimeRange& lifeTime)
        {
            for (size_t i = 0; i < m_resources.size(); ++i)
            {
                if (m_resources[i].m_lifeTime.IsOverlapping(lifeTime))
                {
                    return true;
                }
            }
            return false;
        }

        bool Contains(IRHIResource* pResource)
        {
            for (size_t i = 0; i < m_resources.size(); ++i)
            {
                if (m_resources[i].m_pResource == pResource)
                {
                    return true;
                }
            }
            return false;
        }
    };

    struct SRVDescriptor
    {
        IRHIResource* m_pResource;
        IRHIDescriptor* m_pDescriptor;
        RHIShaderResourceViewDesc m_desc;            
    };

    struct UAVDescriptor
    {
        IRHIResource* m_pResource;
        IRHIDescriptor* m_pDescriptor;
        RHIUnorderedAccessViewDesc m_desc;
    };
   
public:
    RenderGraphResourceAllocator(IRHIDevice* pDevice);
    ~RenderGraphResourceAllocator();

    void Reset();
    
    IRHITexture* AllocateNonOverlappingTexture(const RHITextureDesc& desc, const eastl::string& name, RHIAccessFlags& initialState);
    void FreeNonOverlappingTexture(IRHITexture* pTexture, RHIAccessFlags state);

    IRHITexture* AllocateTexture(uint32_t firstPass, uint32_t lastPass, RHIAccessFlags lastState, const RHITextureDesc& desc, const eastl::string& name, RHIAccessFlags& initialState);
    IRHIBuffer* AllocateBuffer(uint32_t firstPass, uint32_t lastPass, RHIAccessFlags lastFlag, const RHIBufferDesc& desc, const eastl::string& name, RHIAccessFlags& initialState);
    void Free(IRHIResource* pResource, RHIAccessFlags state, bool setState);

    IRHIResource* GetAliasedPrevResource(IRHIResource* pResource, uint32_t firstPass, RHIAccessFlags& lastUsedState);
    
    IRHIDescriptor* GetDesciptor(IRHIResource* pResource, const RHIShaderResourceViewDesc& desc);
    IRHIDescriptor* GetDesciptor(IRHIResource* pResource, const RHIUnorderedAccessViewDesc& desc);

private:
    void CheckHeapUsage(Heap& heap);
    void DeleteDescroptor(IRHIResource* resource);
    void AllocateHeap(uint32_t size);

private:
    IRHIDevice* m_pDevice;
    eastl::vector<Heap> m_allocatedHeaps;

    struct NonOverlappingTexture
    {
        IRHITexture* m_pTexture;
        RHIAccessFlags m_lastUsedState;
        uint64_t m_lastUsedFrame;
    };
    eastl::vector<NonOverlappingTexture> m_freeOverlappingTextures;

    eastl::vector<SRVDescriptor> m_allocatedSRVs;
    eastl::vector<UAVDescriptor> m_allocatedUAVs;
    
};