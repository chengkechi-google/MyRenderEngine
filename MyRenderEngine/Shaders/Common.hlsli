#pragma once
#include "GlobalConstants.hlsli"

static const float M_PI = 3.141592653f;

static const uint INVALID_RESOURCE_INDEX = 0xFFFFFFFF;
static const uint INVALID_ADDRESS = 0xFFFFFFFF;

float max3(float3 v)
{
    return max(max(v.x, v.y), v.z);
}

float min3(float3 v)
{
    return min(min(v.x, v.y), v.z);
}

float square(float x)
{
    return x * x;
}

template<typename T>
T LinearToSRGBChannel(T color)
{
    return color < 0.00313067 ? color * 12.92 : pow(color, (1.0f / 2.4)) * 1.055 - 0.055;
}

template<typename T>
T SRGBToLinearChannel(T color)
{
    return color < 0.04045 ? pow(color * (1.0 / 1.055) + 0.0521327, 2.4) : color * (1.0 / 12.92);
}

template<typename T>
T LinearToSRGB(T color)
{
    return T(LinearToSRGBChannel(color.r), LinearToSRGBChannel(color.g), LinearToSRGBChannel(color.b));
}

template<typename T>
T SRGBToLinear(T color)
{
    return T(SRGBToLinearChannel(color.r), SRGBToLinearChannel(color.g), SRGBToLinearChannel(color.g));
}

float3 RGBToYCbCr(float3 rgb)
{
    const float3x3 m = float3x3(
        0.2126,  0.7152,  0.0722,
       -0.1146, -0.3854,  0.5,
        0.5,    -0.4542, -0.0458
    );
    
    return mul(m, rgb);
}

float3 YCbCrToRGB(float3 ycbcr)
{
    const float3x3 m = float3x3(
        1.0,    0.0,       1.5748,
        1.0,   -0.1873,   -0.4681,
        1.0,    1.8556,    0.0
    );
    
    return max(0.0, mul(m, ycbcr));
}

float3 RGBToYCoCg(float3 rgb)
{
    const float3x3 m = float3x3(
        0.25,   0.5,    0.25,
        0.5,    0.0,   -0.5,
       -0.25,   0.5,   -0.25
    );

    return mul(m, rgb);
}

float3 YCoCgToRGB(float3 ycocg)
{
    const float3x3 m = float3x3(
        1.0,    1.0,   -1.0,
        1.0,    0.0,    1.0,
        1.0,   -1.0,   -1.0
    );

    return mul(m, ycocg);
}

float Luminance(float3 color)
{
    return dot(color, float3(0.2126729, 0.7151522, 0.0721750));
}

float4 UnpackRGBA8Unorm(uint packed)
{
    uint16_t4 unpacked = unpack_u8u16((uint8_t4_packed)packed);
    return unpacked / 255.0f;
}

uint PackRGBA8Unorm(float4 input)
{
    uint16_t4 unpacked = uint16_t4(input * 255.0 + 0.5);
    return (uint)pack_u8(unpacked);
}

// Map normal to octahedron uv plane -1 ~ 1
float2 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));        
    //n.xy = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * (n.xy >= 0.0 ? 1.0 : -1.0);
    n.xy = n.z >= 0 ? n.xy : (1.0 - abs(n.yx)) * lerp(-1.0, 1.0, n.xy >= 0.0);
    
    return n.xy;
}

// https://twitter.com/Stubbesaurus/status/937994790553227264
float3 OctDecode(float2 uv)
{
    float3 n = float3(uv.x, uv.y, 1.0 - abs(uv.x) - abs(uv.y));
    float t = saturate(-n.z);
    //n.xy += n.xy >= 0.0 ? -t : t;
    n.xy += lerp(t, -t, n.xy >= 0.0);
    
    return normalize(n);
}

float3 EncodeNormal(float3 n)
{
    float2 uv = OctEncode(n) * 0.5 + 0.5;
    
    uint2 int12 = (uint2) round(4096);
    uint3 int8 = uint3(int12.x & 0xFF, int12.y & 0xFF, ((int12.x >> 4) & 0xF0) | ((int12.y >> 8) & 0x0F));
    return int8 / 255.0f;
}

float3 DecodeNormal(float3 f)
{
    uint3 int8 = (uint) round(f * 255.0f);
    uint2 int12 = uint2(int8.x | ((int8.z & 0xF0) << 4), (int8.y | (int8.z & 0x0F) << 8));
    float2 uv = int12 / 4096;
    
    return OctDecode(uv * 2.0 - 1.0);
}

uint EncodeNormal16x2(float3 n)
{
    float2 uv = OctEncode(n) * 0.5 + 0.5;
    uint2 u16 = (uint2) round(uv * 65535.0);
    return (u16.x << 16) | u16.y;
}

float3 DecodeNormal16x2(uint f)
{
    uint2 u16 = uint2(f >> 16, f & 0xFFFF);
    float2 uv = u16 / 65535.0;
    
    return OctDecode(uv * 2.0 - 1.0);
}

float GetLinearDepth(float ndcDepth)
{
    //float C1 = GetCameraCB().linearZParams.x;
    //float C2 = GetCameraCB().linearZParams.y;    
    //return 1.0f / (ndcDepth * C1 - C2);   
    
    return GetCameraCB().m_nearZ / ndcDepth;  //< Using infinite far plane 
}

float GetNDCDepth(float linearDepth)
{
    //float A = GetCameraCB().mtxProjection._33;
    //float B = GetCameraCB().mtxProjection._34;
    //return A + B / linearDepth;
    
    return GetCameraCB().m_nearZ / linearDepth; //< Using infinie far plane
}

float3 GetNDCPosition(float4 clipPos)
{
    return clipPos.xyz / max(clipPos.w, 0.0000001);
}

// [0, width / height] -> [-1, 1]
float2 GetNDCPosition(float2 screenPos, float2 rcpBufferSize)
{
    float2 screenUV = screenPos * rcpBufferSize;
    return (screenUV * 2.0 - 1.0) * float2(1.0, -1.0);  //< scrren v is inversed
}

// [0, width / height] -> [0, 1]
float2 GetScreenUV(uint2 screenPos, float2 rcpBufferSize)
{
    return ((float2) screenPos + 0.5) * rcpBufferSize;
}

// [-1, 1] -> [0, 1]
float2 GetScreenUV(float2 ndcPos)
{
    return ndcPos * float2(0.5, -0.5) + 0.5;
}

// [-1, 1] -> [0, width / height]
float2 GetScreenPosition(float2 ndcPos, uint2 bufferSize)
{
    return GetScreenUV(ndcPos) * bufferSize;
}

float3 GetWorldPosition(float2 screenUV, float depth)
{
    float4 clipPos = float4((screenUV * 2.0 - 1.0) * float2(1.0, -1.0), depth, 1.0);
    float4 worldPos = mul(GetCameraCB().m_mtxViewProjectionInverse, clipPos);
    worldPos.xyz /= worldPos.w;
    
    return worldPos.xyz;
}

float3 GetWorldPosition(uint2 screenPos, float depth)
{
    return GetWorldPosition(GetScreenUV(screenPos, SceneCB.m_rcpRenderSize), depth);

}

// "Efficient Construction of Perpendicular Vectors Without Branching"
float3 GetPerpendicularVector(float3 u)
{
    float3 a = abs(u);
    uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
    uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return cross(u, float3(xm, ym, zm));
}

// "Texture Level-of-Detail Strategies for Real-Time Ray Tracing"
// Need texture resolution and buffer size
float ComputeTriangleLODConstant(float3 p0, float3 p1, float3 p2, float2 uv0, float2 uv1, float2 uv2)
{
    float P_a = length(cross(p1 - p0, p2 - p0)); //Eq. 4 
    float T_a = abs((uv1.x - uv0.x) * (uv2.y - uv0.y) - (uv2.x - uv0.x) * (uv1.y - uv0.y)); //< todo: Not correct, need texture resolution
    float triangleLODConstant = 0.5 * log2(T_a / P_a); // Eq. 3
    
    return triangleLODConstant;
}

float3x3 GetTangentBasis(float3 N)
{
    const float3 B = GetPerpendicularVector(N);
    const float3 T = cross(B, N);
    
    return float3x3(T, B, N);
}