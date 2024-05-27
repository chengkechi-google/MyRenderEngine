#include "RHI.h"
#include "DX12/D3D12Device.h"
#include "xxHash/xxhash.h"
#include "microprofile/microprofile.h"

IRHIDevice* CreateRHIDevice(const RHIDeviceDesc& desc)
{
    IRHIDevice *pDevice = nullptr;
    switch (desc.m_backEnd)
    {
        case RHIRenderBackEnd::D3D12:
        {
            pDevice = new D3D12Device(desc);
            if (!((D3D12Device*)pDevice)->Init())
            {
                delete pDevice;
                pDevice = nullptr;
            }
            break;
        }

        default:
            break;
    }

    return pDevice;
}

void BeginMicroProfileGPUEvent(IRHICommandList* pCommandList, const eastl::string& eventName)
{
#if MICROPROFILE_GPU_TIMERS
    static const uint32_t EVENT_COLOR[] = 
    {
        MP_LIGHTCYAN4,
        MP_SKYBLUE2,
        MP_SEAGREEN4,
        MP_LIGHTGOLDENROD4,
        MP_BROWN3,
        MP_MEDIUMPURPLE2,
        MP_SIENNA,
        MP_LIMEGREEN,
        MP_MISTYROSE,
        MP_LIGHTYELLOW,
        
    };

    uint32_t colorCount = sizeof(EVENT_COLOR) / sizeof(EVENT_COLOR[0]);
    uint32_t color = EVENT_COLOR[XXH32(eventName.c_str(), strlen(eventName.c_str()), 0) % colorCount];
    MicroProfileToken token = MicroProfileGetToken("GPU", eventName.c_str(), color, MicroProfileTokenTypeGpu);
    MicroProfileEnterGpu(token, pCommandList->GetProfileLog());
#endif
}

void EndMicroProfileGPUEvent(IRHICommandList* pCommandList)
{
#if MICROPROFILE_GPU_TIMERS
    MicroProfileLeaveGpu(pCommandList->GetProfileLog());
#endif
}

uint32_t GetFormatRowPitch(RHIFormat format, uint32_t width)
{
    switch (format)
    {
        case RHIFormat::RGBA32F:
        case RHIFormat::RGBA32UI:
        case RHIFormat::RGBA32SI:
            return width * 16;
        case RHIFormat::RGB32F:
        case RHIFormat::RGB32UI:
        case RHIFormat::RGB32SI:
            return width * 12;
        case RHIFormat::RGBA16F:
        case RHIFormat::RGBA16UI:
        case RHIFormat::RGBA16SI:
        case RHIFormat::RGBA16UNORM:
        case RHIFormat::RGBA16SNORM:
            return width * 8;
        case RHIFormat::RGBA8UI:
        case RHIFormat::RGBA8SI:
        case RHIFormat::RGBA8UNORM:
        case RHIFormat::RGBA8SNORM:
        case RHIFormat::RGBA8SRGB:
        case RHIFormat::BGRA8UNORM:
        case RHIFormat::BGRA8SRGB:
        case RHIFormat::RGB10A2UI:
        case RHIFormat::RGB10A2UNORM:
        case RHIFormat::R11G11B10F:
        case RHIFormat::RGB9E5:
            return width * 4;
        case RHIFormat::RG32F:
        case RHIFormat::RG32UI:
        case RHIFormat::RG32SI:
            return width * 8;
        case RHIFormat::RG16F:
        case RHIFormat::RG16UI:
        case RHIFormat::RG16SI:
        case RHIFormat::RG16UNORM:
        case RHIFormat::RG16SNORM:
            return width * 4;
        case RHIFormat::RG8UI:
        case RHIFormat::RG8SI:
        case RHIFormat::RG8UNORM:
        case RHIFormat::RG8SNORM:
            return width * 2;
        case RHIFormat::R32F:
        case RHIFormat::R32UI:
        case RHIFormat::R32SI:
            return width * 4;
        case RHIFormat::R16F:
        case RHIFormat::R16UI:
        case RHIFormat::R16SI:
        case RHIFormat::R16UNORM:
        case RHIFormat::R16SNORM:
            return width * 2;
        case RHIFormat::R8UI:
        case RHIFormat::R8SI:
        case RHIFormat::R8UNORM:
        case RHIFormat::R8SNORM:
            return width;
        case RHIFormat::BC1UNORM:
        case RHIFormat::BC1SRGB:
        case RHIFormat::BC4UNORM:
        case RHIFormat::BC4SNORM:
            return width / 2;
        case RHIFormat::BC2UNORM:
        case RHIFormat::BC2SRGB:
        case RHIFormat::BC3UNORM:
        case RHIFormat::BC3SRGB:
        case RHIFormat::BC5UNORM:
        case RHIFormat::BC5SNORM:
        case RHIFormat::BC6U16F:
        case RHIFormat::BC6S16F:
        case RHIFormat::BC7UNORM:
        case RHIFormat::BC7SRGB:
            return width;
        default:
            MY_ASSERT(false);
            return 0;
    }
}

uint32_t GetFormatBlockWidth(RHIFormat format)
{
    if (format >= RHIFormat::BC1UNORM && format <= RHIFormat::BC7SRGB)
    {
        return 4;
    }

    return 1;
}

uint32_t GetFormatBlockHeight(RHIFormat format)
{
    if (format >= RHIFormat::BC1UNORM && format <= RHIFormat::BC7SRGB)
    {
        return 4;
    }

    return 1;
}

uint32_t GetFormatComponentNum(RHIFormat format)
{
    switch (format)
    {
        case RHIFormat::RGBA32F:
        case RHIFormat::RGBA32UI:
        case RHIFormat::RGBA32SI:
        case RHIFormat::RGBA16F:
        case RHIFormat::RGBA16UI:
        case RHIFormat::RGBA16SI:
        case RHIFormat::RGBA16UNORM:
        case RHIFormat::RGBA16SNORM:
        case RHIFormat::RGBA8UI:
        case RHIFormat::RGBA8SI:
        case RHIFormat::RGBA8UNORM:
        case RHIFormat::RGBA8SNORM:
        case RHIFormat::RGBA8SRGB:
        case RHIFormat::BGRA8UNORM:
        case RHIFormat::BGRA8SRGB:
        case RHIFormat::RGB10A2UI:
        case RHIFormat::RGB10A2UNORM:
            return 4;
        case RHIFormat::RGB32F:
        case RHIFormat::RGB32SI:
        case RHIFormat::RGB32UI:
        case RHIFormat::R11G11B10F:
        case RHIFormat::RGB9E5:
            return 3;
        case RHIFormat::RG32F:
        case RHIFormat::RG32UI:
        case RHIFormat::RG32SI:
        case RHIFormat::RG16F:
        case RHIFormat::RG16UI:
        case RHIFormat::RG16SI:
        case RHIFormat::RG16UNORM:
        case RHIFormat::RG16SNORM:
        case RHIFormat::RG8UI:
        case RHIFormat::RG8SI:
        case RHIFormat::RG8UNORM:
        case RHIFormat::RG8SNORM:
            return 2;
        case RHIFormat::R32F:
        case RHIFormat::R32UI:
        case RHIFormat::R32SI:
        case RHIFormat::R16F:
        case RHIFormat::R16UI:
        case RHIFormat::R16SI:
        case RHIFormat::R16UNORM:
        case RHIFormat::R16SNORM:
        case RHIFormat::R8UI:
        case RHIFormat::R8SI:
        case RHIFormat::R8UNORM:
        case RHIFormat::R8SNORM:
            return 1;
        default:
            MY_ASSERT(false);
            return 0;           
    }
}

bool IsDepthFormat(RHIFormat format)
{
    return format == RHIFormat::D32FS8 || format == RHIFormat::D32F || format == RHIFormat::D16;
}

bool IsStencilFormat(RHIFormat format)
{
    return format == RHIFormat::D32FS8;
}

uint32_t CalcSubresource(const RHITextureDesc& desc, uint32_t mipLevel, uint32_t arraySlice)
{
    return mipLevel + desc.m_mipLevels * arraySlice;
}

void DecomposeSubresource(const RHITextureDesc& desc, uint32_t subresource, uint32_t& mip, uint32_t& slice)
{
    mip = subresource % desc.m_mipLevels;
    slice = (subresource / desc.m_mipLevels) % desc.m_arraySize;
}