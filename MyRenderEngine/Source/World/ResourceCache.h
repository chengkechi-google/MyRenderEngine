#pragma once

#include "Renderer/Renderer.h"
#include "EASTL/hash_map.h"

class ResourceCache
{
public:
    static ResourceCache* GetInstance();

    Texture2D* GetTexture2D(const eastl::string& file, bool srgb = false);
    void ReleaseTexture2D(Texture2D* pTexture);

    OffsetAllocator::Allocation GetSceneBuffer(const eastl::string& name, const void* pData, uint32_t size);
    void ReleaseSceneBuffer(OffsetAllocator::Allocation allocation);

private:
    struct Resource
    {
        void* m_ptr;
        uint32_t m_refCount;
    };

    struct SceneBuffer
    {
        OffsetAllocator::Allocation m_allocation;
        uint32_t m_refCount;
    };

    eastl::hash_map<eastl::string, Resource> m_cachedTexture2D;
    eastl::hash_map<eastl::string, SceneBuffer> m_cachedSceneBuffer;
};