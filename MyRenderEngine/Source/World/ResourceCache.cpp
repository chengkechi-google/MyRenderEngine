#include "ResourceCache.h"
#include "Renderer/Renderer.h"
#include "Core/Engine.h"

ResourceCache* ResourceCache::GetInstance()
{
    static ResourceCache cache;
    return &cache;
}

Texture2D* ResourceCache::GetTexture2D(const eastl::string& file, bool srgb)
{
    auto iter = m_cachedTexture2D.find(file);
    if (iter != m_cachedTexture2D.end())
    {
        iter->second.m_refCount ++;
        return (Texture2D*) iter->second.m_ptr;
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    
    Resource texture;
    texture.m_refCount = 1;
    texture.m_ptr = pRenderer->CreateTexture2D(file, srgb);     //< File name also the texture name
    m_cachedTexture2D.insert(eastl::make_pair(file, texture));
    return (Texture2D*) texture.m_ptr;
}

void ResourceCache::ReleaseTexture2D(Texture2D* pTexture)
{
    if (pTexture == nullptr)
    {
        MY_ASSERT("Shoould not happen" && false);
        return;
    }

    /*for (auto iter = m_cachedTexture2D.begin(); iter != m_cachedTexture2D.end(); ++iter)
    {
        if (iter->second.m_ptr == pTexture)
        {
            iter->second.m_refCount --;
            if (iter->second.m_refCount == 0)
            {
                delete pTexture;
                m_cachedTexture2D.erase(iter);
            }

            return;
        }
    }*/

    // hash_map look up takes O(1), so maybe faster
    auto iter = m_cachedTexture2D.find(pTexture->GetName());
    if (iter != m_cachedTexture2D.end())
    {
        iter->second.m_refCount --;
        if (iter->second.m_refCount == 0)
        {
            delete pTexture;
            m_cachedTexture2D.erase(iter);
        }

        return;
    }
    
    MY_ASSERT(false);
}

OffsetAllocator::Allocation ResourceCache::GetSceneBuffer(const eastl::string& name, const void* pData, uint32_t size)
{
    auto iter = m_cachedSceneBuffer.find(name);
    if (iter != m_cachedSceneBuffer.end())
    {
        iter->second.m_refCount ++;
        return iter->second.m_allocation;
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    SceneBuffer buffer;
    buffer.m_refCount = 1;
    buffer.m_allocation = pRenderer->AllocateSceneStaticBuffer(pData, size);
    m_cachedSceneBuffer.insert(eastl::make_pair(name, buffer));

    return buffer.m_allocation;
}

void ResourceCache::ReleaseSceneBuffer(OffsetAllocator::Allocation allocation)
{
    if (allocation.metadata == OffsetAllocator::Allocation::NO_SPACE)
    {
        MY_ASSERT(false); //< Over allocated?
        return;
    }

    for (auto iter = m_cachedSceneBuffer.begin(); iter != m_cachedSceneBuffer.end(); ++iter)
    {
        if (iter->second.m_allocation.metadata == allocation.metadata &&
            iter->second.m_allocation.offset == allocation.offset)
        {
            iter->second.m_refCount --;
            
            if (iter->second.m_refCount == 0)
            {
                Engine::GetInstance()->GetRenderer()->FreeSceneStaticBuffer(allocation);
                m_cachedSceneBuffer.erase(iter);
            }
            
            return;
        }
    }

    MY_ASSERT(false);
}