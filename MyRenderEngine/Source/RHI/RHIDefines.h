#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"
#include "EASTL/span.h"

class IRHIShader;
class IRHIBuffer;
class IRHITexture;
class IRHIRayTracingBLAS;
class IRHIHeap;

static const uint32_t RHI_ALL_SUB_RESOURCE = 0xFFFFFFFF;
static const uint32_t RHI_INVALID_RESOURCE = 0xFFFFFFFF;
static const uint32_t RHI_MAX_INFLIGHT_FRAMES = 3;
static const uint32_t RHI_MAX_ROOT_CONSTATNS = 8;
static const uint32_t RHI_MAX_CBV_BINDINGS = 3;
static const uint32_t RHI_MAX_RENDER_TARGET_ACCOUNT = 8;

enum class RHIRenderBackEnd
{
    D3D12
};

enum class RHICullMode
{
    None,
    Front,
    Back
};

enum class RHICompareFunc
{
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

enum class RHIStencilOp
{
    Keep,
    Zero,
    Replace,
    IncreaseClamp,
    DecreaseClamp,
    Invert,
    IncreaseWrap,
    DecreaseWrap
};

enum class RHIBlendFactor
{
    Zero,
    One,
    SrcColor,
    InvSrcColor,
    SrcAlpha,
    InvSrcAlpha,
    DstColor,
    InvDstColor,
    DstAlpha,
    InvDstAlpha,
    SrcAlphaClamp,
    ConstantFactor,
    InvConstantFactor
};

enum class RHIBlendOp
{
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max
};

enum RHIColorWriteMaskBit
{
    RHIColorWriteMaskR = 1,
    RHIColorWriteMaskG = 2,
    RHIColorWriteMaskB = 4,
    RHIColorWriteMaskA = 8,
    RHIColorWriteMaskAll = (RHIColorWriteMaskR | RHIColorWriteMaskG | RHIColorWriteMaskB | RHIColorWriteMaskA)
};
using RHIColorWriteMask = uint8_t;

enum class RHIFormat
{
    Unknown,
    
    RGBA32F,
    RGBA32UI,
    RGBA32SI,
    RGBA16F,
    RGBA16UI,
    RGBA16SI,
    RGBA16UNORM,
    RGBA16SNORM,
    RGBA8UI,
    RGBA8SI,
    RGBA8UNORM,
    RGBA8SNORM,
    RGBA8SRGB,
    BGRA8UNORM,
    BGRA8SRGB,
    RGB10A2UI,
    RGB10A2UNORM,

    RGB32F,
    RGB32UI,
    RGB32SI,
    R11G11B10F,
    RGB9E5,

    RG32F,
    RG32UI,
    RG32SI,
    RG16F,
    RG16UI,
    RG16SI,
    RG16UNORM,
    RG16SNORM,
    RG8UI,
    RG8SI,
    RG8UNORM,
    RG8SNORM,

    R32F,
    R32UI,
    R32SI,
    R16F,
    R16UI,
    R16SI,
    R16UNORM,
    R16SNORM,
    R8UI,
    R8SI,
    R8UNORM,
    R8SNORM,

    D32F,
    D32FS8,
    D16,

    BC1UNORM,
    BC1SRGB,
    BC2UNORM,
    BC2SRGB,
    BC3UNORM,
    BC3SRGB,
    BC4UNORM,
    BC4SNORM,
    BC5UNORM,
    BC5SNORM,
    BC6U16F,
    BC6S16F,
    BC7UNORM,
    BC7SRGB,
};

enum class RHIMemoryType
{
    GPUOnly,
    CPUOnly,        //< For staging buffer
    CPUToGPU,       //< Frequently updated buffer
    GPUToCPU        //< Read back       
};

enum class RHIAllocationType
{
    Comitted,
    Placed,
    Sparse,
};

enum RHIBufferUsageBit
{
    RHIBufferUsageConstantBuffer = 1 << 0,
    RHIBufferUsageStructedBuffer = 1 << 1,
    RHIBufferUsageTypedBuffer = 1 << 2,
    RHIBufferUsageRawBuffer = 1 << 3,
    RHIBufferUsageUnorderedAccess = 1 << 4,
};
using RHIBufferUsageFlags = uint32_t;

enum RHITextureUsageBit
{
    RHITextureUsageRenderTarget = 1 << 0,
    RHITextureUsageDepthStencil = 1 << 1,
    RHITextureUsageUnorderedAccess = 1 << 2, 
};
using RHITextureUsageFlags = uint32_t;

enum class RHITextureType
{
    Texture2D,
    Texture2DArray,
    Texture3D,
    TextureCube,
    TextureCubeArray
};

enum class RHICommandQueue
{
    Graphics,
    Compute,
    Copy
};

enum RHIAccessBit
{
    RHIAccessPresent            = 1 << 0,
    RHIAccessRTV                = 1 << 1,
    RHIAccessDSV                = 1 << 2,
    RHIAccessDSVReadOnly        = 1 << 3,
    RHIAccessVertexShaderSRV    = 1 << 4,
    RHIAccessPixelShaderSRV     = 1 << 5,
    RHIAccessComputeShaderSRV   = 1 << 6,
    RHIAccessVertexShaderUAV    = 1 << 7,
    RHIAccessPixelShaderUAV     = 1 << 8,
    RHIAccessComputeShaderUAV   = 1 << 9,
    RHIAccessClearUAV           = 1 << 10,
    RHIAccessCopyDst            = 1 << 11,
    RHIAccessCopySrc            = 1 << 12,
    RHIAccessShadingRate        = 1 << 13,
    RHIAccessIndexBuffer        = 1 << 14,
    RHIAccessIndirectArgs       = 1 << 15,
    RHIAccessASRead             = 1 << 16,
    RHIAccessASWrite            = 1 << 17,
    RHIAccessDiscard            = 1 << 18,   // Aliasing barrier

    RHIAccessMaskVS = RHIAccessVertexShaderSRV | RHIAccessVertexShaderUAV,
    RHIAccessMaskPS = RHIAccessPixelShaderSRV | RHIAccessPixelShaderUAV,
    RHIAccessMaskCS = RHIAccessComputeShaderSRV | RHIAccessComputeShaderUAV,
    RHIAccessMaskSRV = RHIAccessVertexShaderSRV | RHIAccessPixelShaderSRV | RHIAccessComputeShaderSRV,
    RHIAccessMaskUAV = RHIAccessVertexShaderUAV | RHIAccessPixelShaderUAV | RHIAccessComputeShaderUAV,
    RHIAccessMaskDSV = RHIAccessDSV | RHIAccessDSVReadOnly,
    RHIAccessMaskCopy = RHIAccessCopyDst | RHIAccessCopySrc,
    RHIAccessMaskAS = RHIAccessASRead | RHIAccessASWrite   
};
using RHIAccessFlags = uint32_t;

enum class RHIShaderResourceViewType
{
    Texture2D,
    Texture2DArray,
    Texture3D,
    TextureCube,
    TextureCubeArray,
    StructuredBuffer,
    TypedBuffer,
    RawBuffer,
    RayTracingTLAS,
};

enum class RHIUnorderedAccessViewType
{
    Texture2D,
    Texture2DArray,
    Texture3D,
    StructuredBuffer,
    TypedBuffer,
    RawBuffer,
};

enum class RHIVender
{
    Uknown,
    AMD,
    NVidia,
    Intel
};

struct RHIDeviceDesc
{
    RHIRenderBackEnd m_backEnd = RHIRenderBackEnd::D3D12;
    uint32_t m_maxFrameDelay = 3;
};

struct RHISwapChainDesc
{
    void* m_windowHandle;
    uint32_t m_width = 1;
    uint32_t m_height = 1;
    uint32_t m_backBufferCount = 3;
    RHIFormat m_format = RHIFormat::RGBA8SRGB;
};

struct RHIHeapDesc
{
    uint32_t m_size = 1;
    RHIMemoryType m_memoryType = RHIMemoryType::GPUOnly;
};

struct RHIBufferDesc
{
    uint32_t m_stride = 1;
    uint32_t m_size = 1;
    RHIFormat m_format = RHIFormat::Unknown;
    RHIMemoryType m_memoryType = RHIMemoryType::GPUOnly;
    RHIAllocationType m_allocationType = RHIAllocationType::Placed;
    RHIBufferUsageFlags m_usage = 0;
    IRHIHeap* m_pHeap = nullptr;
    uint32_t m_heapOffset = 0;
};

inline bool operator==(const RHIBufferDesc& lhs, const RHIBufferDesc& rhs)
{
    return lhs.m_stride == rhs.m_stride
        && lhs.m_size == rhs.m_size
        && lhs.m_format == rhs.m_format
        && lhs.m_memoryType == rhs.m_memoryType
        && lhs.m_allocationType == rhs.m_allocationType
        && lhs.m_usage == rhs.m_usage;
}

struct RHITextureDesc
{
    uint32_t m_width = 1;
    uint32_t m_height = 1;
    uint32_t m_depth = 1; 
    uint32_t m_mipLevels = 1;
    uint32_t m_arraySize = 1;
    RHITextureType m_type = RHITextureType::Texture2D;
    RHIFormat m_format = RHIFormat::Unknown;
    RHIMemoryType m_memoryType = RHIMemoryType::GPUOnly;
    RHIAllocationType m_allocationType = RHIAllocationType::Placed;
    RHITextureUsageFlags m_usage = 0;
    IRHIHeap* m_heap = nullptr;
    uint32_t m_heapOffset = 0;
};

inline bool operator==(const RHITextureDesc& lhs, const RHITextureDesc& rhs)
{
    return lhs.m_width == rhs.m_width
        && lhs.m_height == rhs.m_height
        && lhs.m_depth == rhs.m_depth
        && lhs.m_mipLevels == rhs.m_mipLevels
        && lhs.m_arraySize == rhs.m_arraySize
        && lhs.m_type == rhs.m_type
        && lhs.m_format == rhs.m_format
        && lhs.m_memoryType == rhs.m_memoryType
        && lhs.m_allocationType == rhs.m_allocationType
        && lhs.m_usage == rhs.m_usage;
}

struct RHIConstantBufferViewDesc
{
    uint32_t m_size = 0;
    uint32_t m_offset = 0;
};

struct RHIShaderResourceViewDesc
{
    RHIShaderResourceViewType m_type = RHIShaderResourceViewType::Texture2D;
    RHIFormat m_format = RHIFormat::Unknown;

    union 
    {   
        struct
        {
            uint32_t m_mipSlice = 0;
            uint32_t m_arraySlice = 0;
            uint32_t m_mipLevels = uint32_t(-1);
            uint32_t m_arraySize = 1;
            uint32_t m_planeSlice = 0;
        } m_texture;

        struct
        {
            uint32_t m_size = 0;
            uint32_t m_offset = 0;
        } m_buffer;
    };
    
    RHIShaderResourceViewDesc() : m_texture() {}
};

inline bool operator==(const RHIShaderResourceViewDesc& lhs, const RHIShaderResourceViewDesc& rhs)
{
    return lhs.m_type == rhs.m_type
        && lhs.m_texture.m_mipSlice == rhs.m_texture.m_mipSlice
        && lhs.m_texture.m_arraySlice == rhs.m_texture.m_arraySlice
        && lhs.m_texture.m_mipLevels == rhs.m_texture.m_mipLevels
        && lhs.m_texture.m_arraySize == rhs.m_texture.m_arraySize
        && lhs.m_texture.m_planeSlice == rhs.m_texture.m_planeSlice;
}

struct RHIUnorderedAccessViewDesc
{
    RHIUnorderedAccessViewType m_type = RHIUnorderedAccessViewType::Texture2D;
    RHIFormat m_format = RHIFormat::Unknown;
    
    union
    {
        struct
        {
            uint32_t m_mipSlice = 0;
            uint32_t m_arraySlice = 0;
            uint32_t m_arraySize = 0;
            uint32_t m_planeSlice = 0;
        } m_texture;

        struct
        {
            uint32_t m_size = 0;
            uint32_t m_offset = 0;

        } m_buffer;
    };

    RHIUnorderedAccessViewDesc() : m_texture() {}
};

inline bool operator==(const RHIUnorderedAccessViewDesc& lhs, const RHIUnorderedAccessViewDesc& rhs)
{
   return lhs.m_type == rhs.m_type
        && lhs.m_texture.m_mipSlice == rhs.m_texture.m_mipSlice
        && lhs.m_texture.m_arraySlice == rhs.m_texture.m_arraySlice
        && lhs.m_texture.m_arraySize == rhs.m_texture.m_arraySize
        && lhs.m_texture.m_planeSlice == rhs.m_texture.m_planeSlice;
};

enum class RHIPrimitiveType
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip
};

enum class RHIPipelineType
{
    Graphics,
    MeshShading,
    Compute
};

#pragma pack(push, 1)
struct RHIRasterizerState
{
    RHICullMode m_cullMode = RHICullMode::None;
    float m_depthBias = 0.0f;
    float m_depthBiasClamp = 0.0;
    float m_depthSlopeScale = 0.0;
    bool m_wireFrame = false;
    bool m_frontCCW = false;
    bool m_depthClip = true;
    bool m_lineAA = false;
    bool m_conservativeRaster = false;
};

struct RHIDepthStencilOp
{
    RHIStencilOp m_stencilFail = RHIStencilOp::Keep;
    RHIStencilOp m_depthFail = RHIStencilOp::Keep;
    RHIStencilOp m_pass = RHIStencilOp::Keep;
    RHICompareFunc m_stencilFunc = RHICompareFunc::Always;    
};

struct RHIDepthStencilState
{
    RHICompareFunc m_depthFunc = RHICompareFunc::Always;
    bool m_depthTest = false;
    bool m_depthWrite = true;
    RHIDepthStencilOp m_front;
    RHIDepthStencilOp m_back;
    bool m_stencilTest = false;
    uint8_t m_stencilReadMask = 0xFF;
    uint8_t m_stencilWriteMask = 0xFF;
};

struct RHIBlendState
{
    bool m_blendEnable = false;
    RHIBlendFactor m_colorSrc = RHIBlendFactor::One;
    RHIBlendFactor m_colorDst = RHIBlendFactor::One;
    RHIBlendOp m_colorOp = RHIBlendOp::Add;
    RHIBlendFactor m_alphaSrc = RHIBlendFactor::One;
    RHIBlendFactor m_alphaDst = RHIBlendFactor::One;
    RHIBlendOp m_alphaOp = RHIBlendOp::Add;
    RHIColorWriteMask m_writeMask = RHIColorWriteMaskBit::RHIColorWriteMaskAll; // uint8_t    
};

struct RHIGraphicsPipelineDesc
{
    IRHIShader* m_pVS = nullptr;
    IRHIShader* m_pPS = nullptr;
    RHIRasterizerState m_rasterizerState;
    RHIDepthStencilState m_depthStencilState;
    RHIBlendState m_blendState[RHI_MAX_RENDER_TARGET_ACCOUNT];
    RHIFormat m_rtFormat[RHI_MAX_RENDER_TARGET_ACCOUNT] = { RHIFormat::Unknown };
    RHIFormat m_depthStencilFromat = RHIFormat::Unknown;
    RHIPrimitiveType m_primitiveType = RHIPrimitiveType::TriangleList;
};

struct RHIMeshShaderPipelineDesc
{
    IRHIShader* m_pAS = nullptr;
    IRHIShader* m_pMS = nullptr;
    IRHIShader* m_pPS = nullptr;
    RHIRasterizerState m_rasterizerState;
    RHIDepthStencilState m_depthStencilState;
    RHIBlendState m_blendState[RHI_MAX_RENDER_TARGET_ACCOUNT];
    RHIFormat m_rtFormat[RHI_MAX_RENDER_TARGET_ACCOUNT] = { RHIFormat::Unknown };
    RHIFormat m_depthStencilFromat = RHIFormat::Unknown;
};

struct RHIComputePipelineDesc
{
    IRHIShader* m_pCS = nullptr;
};
#pragma pack(pop)

enum class RHITileMappingType
{
    Map,
    Unmap
};

// Not 4 byte aligned
struct RHITileMapping
{
    RHITileMappingType m_type;

    uint32_t m_subresource;
    uint32_t m_x;
    uint32_t m_y;
    uint32_t m_z;

    uint32_t m_tileCount;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_depth;
    uint32_t m_heapOffset;
    
    bool m_useBox;
};

struct RHITilingDesc
{
    uint32_t m_tileCount = 0;
    uint32_t m_standardMips = 0;
    uint32_t m_tileWidth = 1;
    uint32_t m_tileHeight = 1;
    uint32_t m_tileDepth = 1;
    uint32_t m_packedMips = 0;
    uint32_t m_packedMipTiles = 0;
};

struct RHISubresourceTilingDesc
{
    uint32_t m_width;   //< In tile
    uint32_t m_height;  
    uint32_t m_depth;   
    uint32_t m_tileOffset;
};

enum class RHIRenderPassLoadOp
{
    Load,
    Clear,
    DontCare
};

enum class RHIRenderPassStoreOp
{
    Store,
    DontCare
};

struct RHIRenderPassColorAttachment
{
    IRHITexture* m_pTexture = nullptr;
    uint32_t m_mipSlice = 0;
    uint32_t m_arraySlice = 0;
    RHIRenderPassLoadOp m_loadOp = RHIRenderPassLoadOp::Load;
    RHIRenderPassStoreOp m_storeOp = RHIRenderPassStoreOp::Store;
    float m_clearColor[4] = {};    
};

// Not 4 btte aligned
struct RHIRenderPassDepthAttachment
{
    IRHITexture* m_pTexture = nullptr;
    uint32_t m_mipSlice = 0;
    uint32_t m_arraySlice = 0;
    RHIRenderPassLoadOp m_loadOp = RHIRenderPassLoadOp::Load;
    RHIRenderPassStoreOp m_storeOp = RHIRenderPassStoreOp::Store;
    RHIRenderPassLoadOp m_stencilLoadOp = RHIRenderPassLoadOp::Load;
    RHIRenderPassStoreOp m_stencilStoreOp = RHIRenderPassStoreOp::Store;
    float m_clearDepth = 0.0f;
    uint32_t m_clearStencil = 0;
    bool m_readOnly = false;
};

struct RHIRenderPassDesc
{
    RHIRenderPassColorAttachment m_color[RHI_MAX_RENDER_TARGET_ACCOUNT];
    RHIRenderPassDepthAttachment m_depth;
};

enum RHIShaderCompilerFlagBit
{
    RHIShaderCompilerFlag0 = 1 << 0,
    RHIShaderCompilerFlag1 = 1 << 1,
    RHIShaderCompilerFlag2 = 1 << 2,
    RHIShaderCompilerFlag3 = 1 << 3
};
using RHIShaderCompilerFlags = uint32_t;

struct RHIShaderDesc
{
    eastl::string m_file;
    eastl::string m_entryPoint;
    eastl::string m_profile;
    eastl::vector<eastl::string> m_defines;
    RHIShaderCompilerFlags m_flags = 0;
};

enum RHIRayTracingASFlagBit
{
    RHIRayTracingASFlagAllowUpdate = 1 << 0,
    RHIRayTracingASFlagAllowCompaction = 1 << 1,
    RHIRayTracingASFlagPreferFastTrace = 1 << 2,
    RHIRayTracingASFlagPreferFastBuild = 1 << 3,
    RHIRayTracingASFlagLowMemory = 1 << 4, 
};
using RHIRayTracingASFlags = uint32_t;

enum RHIRayTracingInstanceFlagBit
{
    RHIRayTracingInstanceFlagDisableCull = 1 << 0,
    RHIRayTracingInstanceFlagFrontFaceCCW = 1 << 1,
    RHIRayTracingInstanceFlagForceOpaque = 1 << 2,
    RHIRayTracingInstanceFlagForceNoOpaque = 1 << 3,
};
using RHIRayTracingInstanceFlags = uint32_t;

// Not 4 btte aligned
struct RHIRayTracingGeometry
{
    IRHIBuffer* m_vertexBuffer;
    uint32_t m_vertexBufferOffset;
    uint32_t m_vertexCount;
    uint32_t m_vertexStride;
    RHIFormat m_vertexFormat;

    IRHIBuffer* m_indexBuffer;
    uint32_t m_indexBufferOffset;
    uint32_t m_indexCount;
    RHIFormat m_indexFormat;

    bool m_opaque;
};

struct RHIRayTracingInstance
{
    IRHIRayTracingBLAS* m_blas;
    float m_transform[12]; // object to world 3 * 4 matrix
    uint32_t m_instanceID;
    uint32_t m_instanceMask;
    RHIRayTracingInstanceFlags m_flags;
};

struct RHIRayTracingBLASDesc
{
    eastl::vector<RHIRayTracingGeometry> m_geometries;
    RHIRayTracingASFlags m_flags;
};

struct RHIRayTracingTLASDesc
{
    uint32_t m_instanceCount;
    RHIRayTracingASFlags m_flags; 
};

// Sampler
enum class RHIFilter
{
    Point,
    Linear
};

enum class RHISamplerAddressMode
{
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder
};

enum class RHISamplerReductionMode
{
    Standard,
    Compare,
    Min,
    Max
};

struct RHISamplerDesc
{
    RHIFilter m_minFilter = RHIFilter::Point;
    RHIFilter m_magFilter = RHIFilter::Point;
    RHIFilter m_mipFilter = RHIFilter::Point;
    RHISamplerReductionMode m_reductionMode = RHISamplerReductionMode::Standard;
    RHISamplerAddressMode m_addressU = RHISamplerAddressMode::Repeat;
    RHISamplerAddressMode m_addressV = RHISamplerAddressMode::Repeat;
    RHISamplerAddressMode m_addressW = RHISamplerAddressMode::Repeat;
    RHICompareFunc m_compareFunc = RHICompareFunc::Always;
    float m_maxAnisotropy = 1.0f;
    float m_mipBias = 0.0f;
    float m_minLOD = 0.0f;
    float m_maxLOD = 0.0f;
    float m_borderColor[4] = {};
    bool m_enableAnisotropy = false;
};