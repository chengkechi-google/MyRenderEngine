#pragma once
#include "Utils/math.h"
#include "GlobalConstants.hlsli"

class IRHICommandList;

class Camera
{
public:
    Camera();
    ~Camera();

    void SetPerspective(float aspectRatio, float yFOV, float zNear);
    
    void SetPosition(const float3& pos);
    const float3& GetPosition() const { return m_pos; }

    void SetRotation(const float3& rotation);
    const float3& GetRotation() const { return m_rotation; }

    const float4x4& GetWorldMatrix() const { return m_world; }
    const float4x4& GetViewMatrix() const { return m_view; }
    const float4x4& GetPrevViewMatrix() const { return m_prevView; }
    const float4x4& GetProjectionMatrix() const { return m_projectionJitter; }
    const float4x4& GetViewProjectionMatrix() const { return m_viewProjectionJitter; }

    const float4x4& GetNonJitterProjectionMatrix() const { return m_projection; }
    const float4x4& GetNonJitterPrevProjectionMatrix() const { return m_prevProjection; }
    const float4x4& GetNonJitterViewProjectionMatrix() const { return m_viewProjection; }
    const float4x4& GetNonJitterPrevViewProjectionMatrix() const { return m_prevViewProjection; }

    float3 GetLeft() const { return -m_world[0].xyz(); }
    float3 GetRight() const { return m_world[0].xyz(); }
    float3 GetForward() const { return m_world[2].xyz(); }
    float3 GetBack() const { return -m_world[2].xyz(); }
    float3 GetUp() const { return m_world[1].xyz(); }
    float3 GetDwon() const { return -m_world[1].xyz(); }

    float GetMoveSpeed() const { return m_moveSpeed; }
    void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
    
    float GetFOV() const { return m_fov; }
    void SetFOV(float fov);

    void EnableJitter(bool value);
    float2 GetJitter() const { return m_jitter; }
    float2 GetPrevJitter() const { return m_prevJitter; }

    void LockViewFrustum(bool value) { m_frustumLocked = value; }
    void SetFrustumViewPosition(const float3& pos) { m_frustumViewPos = pos; }
    void UpdateFrustumPlanes(const float4x4& matrix);
    const float4* GetFrustumPlanes() const { return m_frustumPlanes; }

    void Tick(float deltaTime);
    
    void SetupCameraCB(CameraConstant& cameraCB);
    void DrawViewFrustum(IRHICommandList* pCommandList);

    bool IsMoved() const { return m_moved; }
    float GetZNear() const { return m_zNear; }

private:
    void UpdateJitter();
    void UpdateMatrix();
    void OnWindowResize(void* pWindow, uint32_t width, uint32_t height);

    void UpdateCameraRotation(float deltaTime);
    void UpdateCameraPosition(float deltaTime);

private:
    float3 m_pos;
    float3 m_rotation;  //< in degree

    bool m_moved = false;

    float4x4 m_world;
    float4x4 m_view;
    float4x4 m_projection;
    float4x4 m_viewProjection;

    float4x4 m_prevView;
    float4x4 m_prevProjection;
    float4x4 m_prevViewProjection;

    float4x4 m_projectionJitter;
    float4x4 m_viewProjectionJitter;

    float4x4 m_prevViewProjectionJitter;

    float m_aspectRatio = 0.0f;
    float m_fov = 0.0;
    float m_zNear = 0.0f;
    float m_moveSpeed = 10.0f;

    float3 m_prevMoveVelocity = {};
    float2 m_prevRotateVelocity = {};
    float m_moveSmoothness = 0.5f;
    float m_rotateSmoothess = 0.5f;

    float m_aperture = 1.0f;
    int m_shutterSpeed = 60;
    int m_iso = 500;
    float m_exposureCompensation = 0.8f;

    bool m_enableJitter = false;
    float2 m_prevJitter{0.0f, 0.0f};
    float2 m_jitter{0.0f, 0.0f};

    bool m_frustumLocked = false;
    float4 m_frustumPlanes[6];
    float3 m_frustumViewPos;
    
};