#pragma once

#include "directx/d3d12.h"
#include "directx/d3dx12.h"
#include <dxgi1_6.h>
#include "../RHIDefines.h"
#include "Utils/assert.h"
#include "Utils/string.h"


#define SAFE_RELEASE(p) if(p != nullptr) { p->Release(); p = nullptr; };

struct D3D12Descriptor
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
    uint32_t index = RHI_INVALID_RESOURCE;
};

inline bool IsNullDescriptor(const D3D12Descriptor& descriptor)
{
    return descriptor.cpuHandle.ptr == 0 && descriptor.gpuHandle.ptr == 0;
}

inline D3D12_HEAP_TYPE RHIToD3D12HeapType(RHIMemoryType memoryType)
{
    switch (memoryType)
    {
        case RHIMemoryType::GPUOnly:
            return D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT;
        case RHIMemoryType::CPUOnly:
        case RHIMemoryType::CPUToGPU:
            return D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_UPLOAD;
        case RHIMemoryType::GPUToCPU:
            return D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_READBACK;
        default:
            return D3D12_HEAP_TYPE::D3D12_HEAP_TYPE_DEFAULT;
    }
}

inline D3D12_BARRIER_SYNC RHIToD3D12BarrierSync(RHIAccessFlags flags)
{
    D3D12_BARRIER_SYNC sync = D3D12_BARRIER_SYNC_NONE;
    bool discard = flags & RHIAccessBit::RHIAccessDiscard;
    if (!discard)
    {
        // D3D Validation : "SyncAfter bits D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW are incompatible with AccessAfter bits D3D12_BARRIER_ACCESS_NO_ACCESS"
        if(flags & RHIAccessBit::RHIAccessClearUAV)     sync |= D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;
    }

    // Now no resource transition in compute queue, it is much easier
    /*if ((flags & RHIAccessBit::RHIAccessMaskVS) && (flags & RHIAccessBit::RHIAccessMaskPS) && (flags & RHIAccessBit::RHIAccessMaskCS))
    {
        sync |= D3D12_BARRIER_SYNC_ALL_SHADING;
        flags &= ~RHIAccessBit::RHIAccessMaskVS;
        flags &= ~RHIAccessBit::RHIAccessMaskPS;
        flags &= ~RHIAccessBit::RHIAccessMaskCS;
    }*/
    
    if(flags & RHIAccessBit::RHIAccessPresent)          sync |= D3D12_BARRIER_SYNC_ALL;
    if(flags & RHIAccessBit::RHIAccessRTV)              sync |= D3D12_BARRIER_SYNC_RENDER_TARGET;
    if(flags & RHIAccessBit::RHIAccessMaskDSV)          sync |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
    if(flags & RHIAccessBit::RHIAccessMaskVS)           sync |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
    if(flags & RHIAccessBit::RHIAccessMaskPS)           sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
    if(flags & RHIAccessBit::RHIAccessMaskCS)           sync |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
    if(flags & RHIAccessBit::RHIAccessMaskCopy)         sync |= D3D12_BARRIER_SYNC_COPY;
    if(flags & RHIAccessBit::RHIAccessShadingRate)      sync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
    if(flags & RHIAccessBit::RHIAccessIndexBuffer)      sync |= D3D12_BARRIER_SYNC_INDEX_INPUT;
    if(flags & RHIAccessBit::RHIAccessIndirectArgs)     sync |= D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
    if(flags & RHIAccessBit::RHIAccessMaskAS)           sync |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;

    return sync;
}

inline D3D12_BARRIER_ACCESS RHIToD3D12BarrierAccess(RHIAccessFlags flags)
{
    if (flags & RHIAccessDiscard)
    {
        return D3D12_BARRIER_ACCESS_NO_ACCESS;
    }

    D3D12_BARRIER_ACCESS access = D3D12_BARRIER_ACCESS_COMMON;

    if(flags & RHIAccessBit::RHIAccessRTV)          access |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
    if(flags & RHIAccessBit::RHIAccessDSV)          access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
    if(flags & RHIAccessBit::RHIAccessDSVReadOnly)  access |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
    if(flags & RHIAccessBit::RHIAccessMaskSRV)      access |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
    if(flags & RHIAccessBit::RHIAccessMaskUAV)      access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
    if(flags & RHIAccessBit::RHIAccessClearUAV)     access |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
    if(flags & RHIAccessBit::RHIAccessCopyDst)      access |= D3D12_BARRIER_ACCESS_COPY_DEST;
    if(flags & RHIAccessBit::RHIAccessCopySrc)      access |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
    if(flags & RHIAccessBit::RHIAccessShadingRate)  access |= D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
    if(flags & RHIAccessBit::RHIAccessIndexBuffer)  access |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
    if(flags & RHIAccessBit::RHIAccessIndirectArgs) access |= D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
    if(flags & RHIAccessBit::RHIAccessASRead)       access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
    if(flags & RHIAccessBit::RHIAccessASWrite)      access |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;

    return access;
}

inline D3D12_BARRIER_LAYOUT RHIToD3D12BarrierLayout(RHIAccessFlags flags)
{
    if(flags & RHIAccessBit::RHIAccessDiscard)      return D3D12_BARRIER_LAYOUT_UNDEFINED;
    if(flags & RHIAccessBit::RHIAccessPresent)      return D3D12_BARRIER_LAYOUT_PRESENT;
    if(flags & RHIAccessBit::RHIAccessRTV)          return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
    if(flags & RHIAccessBit::RHIAccessDSV)          return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
    if(flags & RHIAccessBit::RHIAccessDSVReadOnly)  return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
    if(flags & RHIAccessBit::RHIAccessMaskSRV)      return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
    if(flags & RHIAccessBit::RHIAccessMaskUAV)      return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
    if(flags & RHIAccessBit::RHIAccessClearUAV)     return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
    if(flags & RHIAccessBit::RHIAccessCopyDst)      return D3D12_BARRIER_LAYOUT_COPY_DEST;
    if(flags & RHIAccessBit::RHIAccessCopySrc)      return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
    if(flags & RHIAccessBit::RHIAccessShadingRate)  return D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE;

    MY_ASSERT(false);
    return D3D12_BARRIER_LAYOUT_UNDEFINED;
}

inline DXGI_FORMAT DXGIFormat(RHIFormat format, bool depthSRV = false, bool uav = false)
{
    switch (format)
    {
        case RHIFormat::Unknown:
            return DXGI_FORMAT_UNKNOWN;
        case RHIFormat::RGBA32F:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case RHIFormat::RGBA32UI:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case RHIFormat::RGBA32SI:
            return DXGI_FORMAT_R32G32B32A32_SINT;
        case RHIFormat::RGBA16F:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case RHIFormat::RGBA16UI:
            return DXGI_FORMAT_R16G16B16A16_UINT;
        case RHIFormat::RGBA16SI:
            return DXGI_FORMAT_R16G16B16A16_SINT;
        case RHIFormat::RGBA16UNORM:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case RHIFormat::RGBA16SNORM:
            return DXGI_FORMAT_R16G16B16A16_SNORM;
        case RHIFormat::RGBA8UI:
            return DXGI_FORMAT_R8G8B8A8_UINT;
        case RHIFormat::RGBA8SI:
            return DXGI_FORMAT_R8G8B8A8_SINT;
        case RHIFormat::RGBA8UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case  RHIFormat::RGBA8SNORM:
            return DXGI_FORMAT_R8G8B8A8_SNORM;
        case RHIFormat::RGBA8SRGB:
            return uav ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case RHIFormat::BGRA8UNORM:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case RHIFormat::BGRA8SRGB:
            return uav ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case RHIFormat::RGB10A2UI:
            return DXGI_FORMAT_R10G10B10A2_UINT;
        case RHIFormat::RGB10A2UNORM:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        case RHIFormat::RGB32F:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case RHIFormat::RGB32UI:
            return DXGI_FORMAT_R32G32B32_UINT;
        case RHIFormat::RGB32SI:
            return DXGI_FORMAT_R32G32B32_SINT;
        case RHIFormat::R11G11B10F:
            return DXGI_FORMAT_R11G11B10_FLOAT;
        case RHIFormat::RGB9E5:
            return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        case RHIFormat::RG32F:
            return DXGI_FORMAT_R32G32_FLOAT;
        case RHIFormat::RG32UI:
            return DXGI_FORMAT_R32G32_UINT;
        case RHIFormat::RG32SI:
            return DXGI_FORMAT_R32G32_SINT;
        case RHIFormat::RG16F:
            return DXGI_FORMAT_R16G16_FLOAT;
        case RHIFormat::RG16UI:
            return DXGI_FORMAT_R16G16_UINT;
        case RHIFormat::RG16SI:
            return DXGI_FORMAT_R16G16_SINT;
        case RHIFormat::RG16UNORM:
            return DXGI_FORMAT_R16G16_UNORM;
        case RHIFormat::RG16SNORM:
            return DXGI_FORMAT_R16G16_SNORM;
        case RHIFormat::RG8UI:
            return DXGI_FORMAT_R8G8_UINT;
        case RHIFormat::RG8SI:
            return DXGI_FORMAT_R8G8_SINT;
        case RHIFormat::RG8UNORM:
            return DXGI_FORMAT_R8G8_UNORM;
        case RHIFormat::RG8SNORM:
            return DXGI_FORMAT_R8G8_SNORM;
        case RHIFormat::R32F:
            return DXGI_FORMAT_R32_FLOAT;
        case RHIFormat::R32UI:
            return DXGI_FORMAT_R32_UINT;
        case RHIFormat::R32SI:
            return DXGI_FORMAT_R32_SINT;
        case RHIFormat::R16F:
            return DXGI_FORMAT_R16_FLOAT;
        case RHIFormat::R16UI:
            return DXGI_FORMAT_R16_UINT;
        case RHIFormat::R16SI:
            return DXGI_FORMAT_R16_SINT;
        case RHIFormat::R16UNORM:
            return DXGI_FORMAT_R16_UNORM;
        case RHIFormat::R16SNORM:
            return DXGI_FORMAT_R16_SNORM;
        case RHIFormat::R8UI:
            return DXGI_FORMAT_R8_UINT;
        case RHIFormat::R8SI:
            return DXGI_FORMAT_R8_SINT;
        case RHIFormat::R8UNORM:
            return DXGI_FORMAT_R8_UNORM;
        case RHIFormat::R8SNORM:
            return DXGI_FORMAT_R8_SNORM;
        case RHIFormat::D32F:
            return depthSRV ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_D32_FLOAT;
        case RHIFormat::D32FS8:
            return depthSRV ? DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS : DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case RHIFormat::D16:
            return depthSRV ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_D16_UNORM;
        case RHIFormat::BC1UNORM:
            return DXGI_FORMAT_BC1_UNORM;
        case RHIFormat::BC1SRGB:
            return DXGI_FORMAT_BC1_UNORM_SRGB;
        case RHIFormat::BC2UNORM:
            return DXGI_FORMAT_BC2_UNORM;
        case RHIFormat::BC2SRGB:
            return DXGI_FORMAT_BC2_UNORM_SRGB;
        case RHIFormat::BC3UNORM:
            return DXGI_FORMAT_BC3_UNORM;
        case RHIFormat::BC3SRGB:
            return DXGI_FORMAT_BC3_UNORM_SRGB;
        case RHIFormat::BC4UNORM:
            return DXGI_FORMAT_BC4_UNORM;
        case RHIFormat::BC4SNORM:
            return DXGI_FORMAT_BC4_SNORM;
        case RHIFormat::BC5UNORM:
            return DXGI_FORMAT_BC5_UNORM;
        case RHIFormat::BC5SNORM:
            return DXGI_FORMAT_BC5_SNORM;
        case RHIFormat::BC6U16F:
            return DXGI_FORMAT_BC6H_UF16;
        case RHIFormat::BC6S16F:
            return DXGI_FORMAT_BC6H_SF16;
        case RHIFormat::BC7UNORM:
            return DXGI_FORMAT_BC7_UNORM;
        case RHIFormat::BC7SRGB:
            return DXGI_FORMAT_BC7_UNORM_SRGB;

        default:
            MY_ASSERT(false);
            return DXGI_FORMAT_UNKNOWN;
    }      
}

inline D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE RHIToD3D12RenderPassLoadOp(RHIRenderPassLoadOp loadOp)
{
    switch (loadOp)
    {
        case RHIRenderPassLoadOp::Load:
            return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
        case RHIRenderPassLoadOp::Clear:
            return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
        case RHIRenderPassLoadOp::DontCare:
            return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
        default:
            return  D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
    }
}

inline D3D12_RENDER_PASS_ENDING_ACCESS_TYPE RHIToD3D12RenderPassStoreOp(RHIRenderPassStoreOp storeOp)
{
    switch (storeOp)
    {
        case RHIRenderPassStoreOp::Store:
            return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
        case RHIRenderPassStoreOp::DontCare:
            return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        default:
            return  D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    }
}
        
inline D3D12_BLEND RHIToD3D12Blend(RHIBlendFactor blendFactor)
{
    switch (blendFactor)
    {
        case RHIBlendFactor::Zero:
            return D3D12_BLEND_ZERO;
        case RHIBlendFactor::One:
            return D3D12_BLEND_ONE;
        case RHIBlendFactor::SrcColor:
            return D3D12_BLEND_SRC_COLOR;
        case RHIBlendFactor::InvSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case RHIBlendFactor::SrcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case RHIBlendFactor::InvSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case RHIBlendFactor::DstColor:
            return D3D12_BLEND_DEST_COLOR;
        case RHIBlendFactor::InvDstColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case RHIBlendFactor::DstAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case RHIBlendFactor::InvDstAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case RHIBlendFactor::SrcAlphaClamp:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case RHIBlendFactor::ConstantFactor:
            return D3D12_BLEND_BLEND_FACTOR;
        case RHIBlendFactor::InvConstantFactor:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        default:
            return D3D12_BLEND_ONE;
    }
}

inline D3D12_BLEND_OP RHIToD3D12BlendOp(RHIBlendOp blendOp)
{
    switch (blendOp)
    {
        case RHIBlendOp::Add:
            return D3D12_BLEND_OP_ADD;
        case RHIBlendOp::Subtract:
            return D3D12_BLEND_OP_SUBTRACT;
        case RHIBlendOp::ReverseSubtract:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case RHIBlendOp::Min:
            return D3D12_BLEND_OP_MIN;
        case RHIBlendOp::Max:
            return D3D12_BLEND_OP_MAX;
        default:
            return D3D12_BLEND_OP_ADD;
    }
}

inline D3D12_RENDER_TARGET_BLEND_DESC RHIToD3D12RTBlendDesc(const RHIBlendState& blendState)
{
    D3D12_RENDER_TARGET_BLEND_DESC desc = {};
    desc.BlendEnable = blendState.m_blendEnable;
    desc.SrcBlend = RHIToD3D12Blend(blendState.m_colorSrc);
    desc.DestBlend = RHIToD3D12Blend(blendState.m_colorDst);
    desc.BlendOp = RHIToD3D12BlendOp(blendState.m_colorOp);
    desc.SrcBlendAlpha = RHIToD3D12Blend(blendState.m_alphaSrc);
    desc.DestBlendAlpha = RHIToD3D12Blend(blendState.m_alphaDst);
    desc.BlendOpAlpha = RHIToD3D12BlendOp(blendState.m_alphaOp);
    desc.RenderTargetWriteMask = blendState.m_writeMask;
    return desc;
}

inline D3D12_BLEND_DESC RHIToD3DBlendDesc(const RHIBlendState* blendState)
{
    D3D12_BLEND_DESC desc = {};
    desc.AlphaToCoverageEnable = false;
    desc.IndependentBlendEnable = true;
    
    for (int i = 0; i < RHI_MAX_RENDER_TARGET_ACCOUNT; ++i)
    {
        desc.RenderTarget[i] = RHIToD3D12RTBlendDesc(blendState[i]);
    }

    return desc;
}

inline D3D12_CULL_MODE RHIToD3D12CullMode(RHICullMode cullMode)
{
    switch (cullMode)
    {
        case RHICullMode::None:
            return D3D12_CULL_MODE_NONE;
        case RHICullMode::Front:
            return D3D12_CULL_MODE_FRONT;
        case RHICullMode::Back:
            return D3D12_CULL_MODE_BACK;
        default:
            return D3D12_CULL_MODE_NONE;
    }
}

inline D3D12_RASTERIZER_DESC RHIToD3D12RasterizerDesc(const RHIRasterizerState& rasterizerState)
{
    D3D12_RASTERIZER_DESC desc = {};
    desc.FillMode = rasterizerState.m_wireFrame ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    desc.CullMode = RHIToD3D12CullMode(rasterizerState.m_cullMode);
    desc.FrontCounterClockwise = rasterizerState.m_frontCCW;
    desc.DepthBias = (INT) rasterizerState.m_depthBias;
    desc.DepthBiasClamp = rasterizerState.m_depthBiasClamp;
    desc.SlopeScaledDepthBias = rasterizerState.m_depthSlopeScale;
    desc.DepthClipEnable = rasterizerState.m_depthClip;
    desc.AntialiasedLineEnable = rasterizerState.m_lineAA;
    desc.ConservativeRaster = rasterizerState.m_conservativeRaster ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    
    return desc;
}

inline D3D12_COMPARISON_FUNC RHIToD3D12CompareFunc(RHICompareFunc compareFunc)
{
    switch (compareFunc)
    {
        case RHICompareFunc::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case RHICompareFunc::Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case RHICompareFunc::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case RHICompareFunc::LessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case RHICompareFunc::Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case RHICompareFunc::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case RHICompareFunc::GreaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case RHICompareFunc::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            return D3D12_COMPARISON_FUNC_NONE;
    }
}

inline D3D12_STENCIL_OP RHIToD3D12StencilOp(RHIStencilOp stencilOp)
{
    switch (stencilOp)
    {
        case RHIStencilOp::Keep:
            return D3D12_STENCIL_OP_KEEP;
        case RHIStencilOp::Zero:
            return D3D12_STENCIL_OP_ZERO;
        case RHIStencilOp::Replace:
            return D3D12_STENCIL_OP_REPLACE;
        case RHIStencilOp::IncreaseClamp:
            return D3D12_STENCIL_OP_INCR_SAT;
        case RHIStencilOp::DecreaseClamp:
            return D3D12_STENCIL_OP_DECR_SAT;
        case RHIStencilOp::Invert:
            return D3D12_STENCIL_OP_INVERT;
        case RHIStencilOp::IncreaseWrap:
            return D3D12_STENCIL_OP_INCR;
        case RHIStencilOp::DecreaseWrap:
            return D3D12_STENCIL_OP_DECR;
        default:
            return D3D12_STENCIL_OP_KEEP;
    }
}

inline D3D12_DEPTH_STENCILOP_DESC RHIToD3D12DepthStencilOp(const RHIDepthStencilOp& depthStencilOp)
{
    D3D12_DEPTH_STENCILOP_DESC desc = {};
    desc.StencilFailOp = RHIToD3D12StencilOp(depthStencilOp.m_stencilFail);
    desc.StencilDepthFailOp = RHIToD3D12StencilOp(depthStencilOp.m_depthFail);
    desc.StencilPassOp = RHIToD3D12StencilOp(depthStencilOp.m_pass);
    desc.StencilFunc = RHIToD3D12CompareFunc(depthStencilOp.m_stencilFunc);

    return desc;
}

inline D3D12_DEPTH_STENCIL_DESC RHIToD3D12DepthStencilDesc(const RHIDepthStencilState& depthStencilState)
{
    D3D12_DEPTH_STENCIL_DESC desc = {};
    desc.DepthEnable = depthStencilState.m_depthTest;
    desc.DepthWriteMask = depthStencilState.m_depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = RHIToD3D12CompareFunc(depthStencilState.m_depthFunc);
    desc.StencilEnable = depthStencilState.m_stencilTest;
    desc.StencilReadMask = depthStencilState.m_stencilReadMask;
    desc.StencilWriteMask = depthStencilState.m_stencilWriteMask;
    desc.FrontFace = RHIToD3D12DepthStencilOp(depthStencilState.m_front);
    desc.BackFace = RHIToD3D12DepthStencilOp(depthStencilState.m_back);
    
    return desc;
}

inline D3D12_PRIMITIVE_TOPOLOGY_TYPE RHIToD3D12TopologyType(RHIPrimitiveType primitiveType)
{
    switch (primitiveType)
    {
        case RHIPrimitiveType::PointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case RHIPrimitiveType::LineList:
        case RHIPrimitiveType::LineStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case RHIPrimitiveType::TriangleList:
        case RHIPrimitiveType::TriangleStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }
}

inline D3D12_PRIMITIVE_TOPOLOGY RHIToD3D12Topology(RHIPrimitiveType primitiveType)
{
    switch (primitiveType)
    {
        case RHIPrimitiveType::PointList:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case RHIPrimitiveType::LineList:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case RHIPrimitiveType::LineStrip:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case RHIPrimitiveType::TriangleList:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case RHIPrimitiveType::TriangleStrip:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default:
            return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}

inline D3D12_FILTER_REDUCTION_TYPE RHIToD3D12FilterReductionType(RHISamplerReductionMode mode)
{
    switch (mode)
    {
        case RHISamplerReductionMode::Standard:
            return D3D12_FILTER_REDUCTION_TYPE_STANDARD;
        case RHISamplerReductionMode::Compare:
            return D3D12_FILTER_REDUCTION_TYPE_COMPARISON;
        case RHISamplerReductionMode::Min:
            return D3D12_FILTER_REDUCTION_TYPE_MINIMUM;
        case RHISamplerReductionMode::Max:
            return D3D12_FILTER_REDUCTION_TYPE_MAXIMUM;
        default:
            return D3D12_FILTER_REDUCTION_TYPE_STANDARD;
    }
}

inline D3D12_FILTER_TYPE RHIToD3D12FilterType(RHIFilter filter)
{
    switch (filter)
    {
        case RHIFilter::Point:
            return D3D12_FILTER_TYPE_POINT;
        case RHIFilter::Linear:
            return D3D12_FILTER_TYPE_LINEAR;
        default:
            return D3D12_FILTER_TYPE_POINT;
    }
}

inline D3D12_FILTER RHIToD3D12Filter(const RHISamplerDesc& desc)
{
    D3D12_FILTER_REDUCTION_TYPE reductionType = RHIToD3D12FilterReductionType(desc.m_reductionMode);
    if (desc.m_enableAnisotropy)
    {
        return D3D12_ENCODE_ANISOTROPIC_FILTER(reductionType);
    }
    else
    {
        D3D12_FILTER_TYPE minFilter = RHIToD3D12FilterType(desc.m_minFilter);
        D3D12_FILTER_TYPE magFilter = RHIToD3D12FilterType(desc.m_magFilter);
        D3D12_FILTER_TYPE mipFilter = RHIToD3D12FilterType(desc.m_mipFilter);        

        return D3D12_ENCODE_BASIC_FILTER(minFilter, magFilter, mipFilter, reductionType);
    }
}

inline D3D12_TEXTURE_ADDRESS_MODE RHIToD3D12AddressMode(RHISamplerAddressMode mode)
{
    switch (mode)
    {
        case RHISamplerAddressMode::Repeat:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case RHISamplerAddressMode::MirroredRepeat:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case RHISamplerAddressMode::ClampToEdge:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case RHISamplerAddressMode::ClampToBorder:
            return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

inline D3D12_RESOURCE_DESC RHIToD3D12ResourceDesc(const RHITextureDesc& desc)
{
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Width = desc.m_width;
    resourceDesc.Height = desc.m_height;
    resourceDesc.MipLevels = desc.m_mipLevels;
    resourceDesc.Format = DXGIFormat(desc.m_format);
    resourceDesc.SampleDesc.Count = 1;
    
    if (desc.m_allocationType == RHIAllocationType::Sparse)
    {
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
    }

    if (desc.m_usage & RHITextureUsageRenderTarget)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }

    if (desc.m_usage & RHITextureUsageDepthStencil)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }

    if (desc.m_usage & RHITextureUsageUnorderedAccess)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    switch (desc.m_type)
    {
        case RHITextureType::Texture2D:
        {
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.DepthOrArraySize = 1;
            break;
        }

        case RHITextureType::Texture2DArray:
        {
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.DepthOrArraySize = desc.m_arraySize;
        }

        case RHITextureType::Texture3D:
        {
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
            resourceDesc.DepthOrArraySize = desc.m_depth;
        }

        case RHITextureType::TextureCube:
        {
            MY_ASSERT(desc.m_arraySize == 6);
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.DepthOrArraySize = desc.m_arraySize;
            break;
        }

        case RHITextureType::TextureCubeArray:
        {
            MY_ASSERT(desc.m_arraySize % 6 == 0);
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.DepthOrArraySize = desc.m_arraySize;
            break;
        }
        
        default:
            break;
    }

    return resourceDesc;
}

inline D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS RHIToD3D12RTASBuildFlags(RHIRayTracingASFlags flags)
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS d3d12Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    
    if (flags & RHIRayTracingASFlagBit::RHIRayTracingASFlagAllowUpdate)
    {
        d3d12Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    }

    if (flags & RHIRayTracingASFlagBit::RHIRayTracingASFlagAllowCompaction)
    {
        d3d12Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
    }

    if (flags & RHIRayTracingASFlagBit::RHIRayTracingASFlagPreferFastTrace)
    {
        d3d12Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    }

    if (flags & RHIRayTracingASFlagBit::RHIRayTracingASFlagPreferFastBuild)
    {
        d3d12Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
    }

    if (flags & RHIRayTracingASFlagBit::RHIRayTracingASFlagLowMemory)
    {
        d3d12Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
    }

    return d3d12Flags;
}

inline D3D12_RAYTRACING_INSTANCE_FLAGS RHIToD3D12RTInstanceFlags(RHIRayTracingInstanceFlags flags)
{
    D3D12_RAYTRACING_INSTANCE_FLAGS d3d12Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

    if (flags & RHIRayTracingInstanceFlagBit::RHIRayTracingInstanceFlagDisableCull)
    {
        d3d12Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
    }

    if (flags & RHIRayTracingInstanceFlagBit::RHIRayTracingInstanceFlagForceNoOpaque)
    {
        d3d12Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
    }

    if (flags & RHIRayTracingInstanceFlagBit::RHIRayTracingInstanceFlagFrontFaceCCW)
    {
        d3d12Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
    }

    if (flags & RHIRayTracingInstanceFlagBit::RHIRayTracingInstanceFlagForceOpaque)
    {
        d3d12Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
    }

    return d3d12Flags;
}