#pragma once

struct MaterialTextureInfo
{
    uint m_index;
    uint m_width : 16;
    uint m_height : 16;
    uint m_bTransform;
    float m_rotation;
    
    float2 m_offset;
    float2 m_scale;
    
#ifdef __cplusplus
    MaterialTextureInfo()
    {
        m_index = RHI_INVALID_RESOURCE;
        m_width = m_height = 0;
        m_bTransform = false;
        m_rotation = 0.0f;
    }
#else    
    float2 TransformUV(float2 uv)
    {
        if(m_bTransform)
        {
            float3x3 mtxTranslation = float3x3(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, m_offset.x, m_offset.y, 0.0f);
            float3x3 mtxRotation = float3x3(cos(m_rotation), sin(m_rotation), 0.0f, -sin(m_rotation), cos(m_rotation), 0.0f, 0.0f, 0.0f, 1.0f); //< Rotate ccw
            float3x3 mtxScale = float3x3(m_scale.x, 0.0f, 0.0f, 0.0f, m_scale.y, 0.0f, 0.0f, 0.0f, 1.0f);
            
            return mul(mul(mul(float3(uv, 1.0f), mtxScale), mtxRotation), mtxTranslation).xy;
        }
        
        return uv;
    }
    
    // "Texture Level-of-Detail Strategies for Real-Time Ray Tracing"
    float ComputeTextureLOD(float3 rayDirection, float3 surfaceNormal, float rayConeWidth, float triangleLODConstant)
    {
        return triangleLODConstant + 0.5f * log2(m_width * m_height) + log2(abs(rayConeWidth / dot(rayDirection, surfaceNormal))); //Eq. 34

    }
#endif
};