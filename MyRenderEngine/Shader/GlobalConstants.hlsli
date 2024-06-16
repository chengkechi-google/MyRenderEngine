#pragma once

struct CullingData
{
    float3 m_viewPos;
    float _padding0;
    
    float4 m_planes[6];
};

struct PhysicalCamera
{
    float m_aperture;
    float m_shutterSpeed;
    int m_iso;
    float m_exposureCompensation;
    
    /* add more physical params when adding dof in the future
    float focalLength;
    float focusDistance;
    float bladeNum;
    */
};

struct CameraConstant
{
    float3 m_cameraPos;
    float m_spreadAngle;
    
    float m_nearZ;
    float _padding0;
    float2 m_linearZParams;
    
    float2 m_jitter;
    float2 m_prevJitter;
    
    float4x4 m_mtxView;
    float4x4 m_mtxViewInverse;
    float4x4 m_mtxProjection;
    float4x4 m_mtxProjectionInverse;
    float4x4 m_mtxViewProjection;
    float4x4 m_mtxViewProjectionInverse;
    
    float4x4 m_mtxViewProjectionNoJitter;
    float4x4 m_mtxPrevViewProjectionNoJitter;
    float4x4 m_mtxClipToPrevClipNoJitter;
    float4x4 m_mtxClipToPrevViewNoJitter;
    
    float4x4 m_mtxPrevView;
    float4x4 m_mtxPrevViewProjection;
    float4x4 m_mtxPrevViewProjectionInverse;
    
    CullingData m_culling;
    PhysicalCamera m_physicalCamera;
};

struct SceneConstant
{
    CameraConstant m_cameraCB;
    
    uint m_sceneConstantBufferSRV;
    uint m_sceneStaticBufferSRV;
    uint m_sceneAnimationBufferSRV;
    uint m_sceneAnimationBufferUAV;
    
    uint m_instanceDataAddress;
    uint m_sceneRayTracingTLAS;
    uint m_secondPhaseMeshletsListUAV;
    uint m_secondPhaseMeshletsCounterUAV;
    
    uint m_showMeshlets;
    float3 m_lightDir;
    
    float3 m_lightColor;
    float m_lightRadius;
    
    uint2 m_renderSize;
    float2 m_rcpRenderSize;
    
    uint2 m_displaySize;
    float2 m_rcpDisplaySize;
    
    uint m_prevSceneColorSRV;
    uint m_prevSceneDepthSRV;
    uint m_prevNormalSRV;
    uint _padding0;
    
    uint m_hzbWidth;
    uint m_hzbHeight;
    uint m_firstPhaseCullingHZBSRV;
    uint m_secondPhaseCullingHZBSRV;
    
    uint m_sceneHZBSRV;
    uint m_debugLineDrawCommandUAV;
    uint m_debugLineVertexBufferUAV;
    uint m_debugFontCharBufferSRV;
    
    uint m_debugTextCounterBufferUAV;
    uint m_debugTextBufferUAV;
    uint m_enableStats;
    uint m_statsBufferUAV;
    
    uint m_pointRepeatSampler;
    uint m_pointClampSampler;
    uint m_bilinearRepeatSampler;
    uint m_bilinearClampSampler;
    
    uint m_bilinearBlackBorderSampler;
    uint m_bilinearWhiteBorderSampler;
    uint m_trilinearRepeatSampler;
    uint m_trilinearClampSampler;
    
    uint m_aniso2xSampler;
    uint m_aniso4xSampler;
    uint m_ansio8xSampler;
    uint m_aniso16xSampler;
    
    uint m_minReductionSampler;
    uint m_maxReductionSampler;
    uint m_blueNoiseTexture;
    uint m_preIntegratedGFTexture;
    
    uint m_sobolSequenceTexture;
    uint m_scramblingRankingTexture1SPP;
    uint m_scramblingRankingTexture2SPP;
    uint m_scramblingRankingTexture4SPP;
    
    uint m_scramblingRankingTexture8SPP;
    uint m_scramblingRankingTexture16SPP;
    uint m_scramblingRankingTexture32SPP;
    uint m_scramblingRankingTexture64SPP;
    
    uint m_scramblingRankingTexture128SPP;
    uint m_scramblingRankingTexture256SPP;
    float m_frameTime;
    uint m_frameIndex;
    
    float m_mipBias;
    uint m_skyCubeTexture;
    uint m_skySpecularIBLTexture;
    uint m_skyDiffuseIBLTexture;
    
    uint m_sheenETexture;
    uint m_tonyMcMapfaceTexture;
    uint m_marschnerTextureM;
    uint m_marschnerTextureN;    
};

#ifndef __cplusplus
ConstantBuffer<SceneConstant> SceneCB : register(b2);

CameraConstant GetCameraCB()
{
    return SceneCB.m_cameraCB;
}
#endif
