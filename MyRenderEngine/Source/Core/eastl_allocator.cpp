#include "EASTL/allocator.h"
#include "Utils/memory.h"
#include "stb/stb_sprintf.h"
#include <stdio.h>

#ifdef EASTL_USER_DEFINED_ALLOCATOR
namespace eastl
{
    allocator gDefaultAllocator;
    allocator* GetDefaultAllocator()
    {
        return &gDefaultAllocator;
    }

    allocator::allocator(const char* EASTL_NAME(pName))
    {
#if EASTL_NAME_ENABLED
        mpName = pName ? pName : EASTL_ALLOCATOR_DEFAULT_NAME;
#endif        
    }
    
    allocator::allocator(const allocator& EASTL_NAME(alloc))
    {
#if EASTL_NAME_ENABLED
        mpName = alloc.mpName;
#endif
    }

    allocator::allocator(const allocator&, const char* EASTL_NAME(pName))
    {
#if EASTL_NAME_ENABLED
        mpName = pName ? pName : EASTL_ALLOCATOR_DEFAULT_NAME;
#endif
    }

    allocator& allocator::operator=(const allocator& EASTL_NAME(alloc))
    {
#if EASTL_NAME_ENABLED
        mpName = alloc.mpName;
#endif
        return *this;
    }

    inline const char* allocator::get_name() const
    {
#if EASTL_NAME_ENABLED
        return mpName;
#else
        return EASTL_ALLOCATOR_DEFAULT_NAME;
#endif
    }

    void* allocator::allocate(size_t n, int flag)
    {
        return MY_ALLOC(n);
    }

    void* allocator::allocate(size_t n, size_t alignment, size_t offset, int flag)
    {
        return MY_ALLOC(n, alignment);
    }

    void allocator::deallocate(void* p, size_t n)
    {
        MY_FREE(p);
    }

    bool operator==(const allocator&, const allocator&)
    {
        return true; // Only use global allocator
    }

#if !defined(EA_COMPILER_HAS_THREE_WAY_COMPARISON)
    bool operator!=(const allocator&, const allocator&)
    {
        return false; // Only use global allocator
    }
#endif
}
#endif // EASTL_USER_DEFINED_ALLOCATOR

int Vsnprintf8(char* p, size_t n, const char* pFormat, va_list args)
{
    return stbsp_vsnprintf(p, (int)n, pFormat, args);
}

int VsnprintfW(wchar_t* p, size_t n, const wchar_t* pFormat, va_list args)
{
#ifdef _MSC_VER
    return _vsnwprintf(p, n, pFormat, args);
#else
    return vsnwprintf(p, n, pFormat, args);
#endif
}
