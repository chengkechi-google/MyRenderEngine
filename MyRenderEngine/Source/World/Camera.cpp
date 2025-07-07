#include "Camera.h"
#include "Core/Engine.h"
#include "Utils/guiUtil.h"

static inline float2 CalcDepthLinearizationParams(const float4x4& mtxProjection)
{
    // | 1  0  0  0 |
    // | 0  1  0  0 |
    // | 0  0  A  1 |
    // | 0  0  B  0 |

    // Z' = (Z * A + B) / Z     --- perspective divide
    // Z' = A + B / Z

    // Z = B / (Z' - A)
    // Z = 1 / (Z' * C1 - C2)   --- C1 = 1/B, C2 = A/B

    float A = mtxProjection[2][2];
    float B = max(mtxProjection[3][2], 0.00000001f);    //< Avoid dividing by 0

    float C1 = 1.0f / B;
    float C2 = A / B;

    return float2(C1, C2);
}

Camera::Camera()
{
    m_pos = float3(0.0f, 0.0f, 0.0f);
    m_rotation = float3(0.0f, 0.0f, 0.0f);

    Engine::GetInstance()->WindowResizeSignal.connect(&Camera::OnWindowResize, this);
}

Camera::~Camera()
{
    Engine::GetInstance()->WindowResizeSignal.disconnect(this);
}

void Camera::SetPerspective(float aspectRatio, float yFOV, float zNear)
{
    m_aspectRatio = aspectRatio;
    m_fov = yFOV;
    m_zNear = zNear;

    // LH + reversed z + infinite far plane
    // Reversed z has higher precision
    // https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
    float h = 1.0 / std::tan(0.5f * degree_to_radian(yFOV));        //< equal cot(0.5f * degree_to_radian(yFOV))
    float w = h / aspectRatio;

    m_projection = float4x4(0.0f);
    m_projection[0][0] = w;
    m_projection[1][1] = h;
    m_projection[2][2] = 0.0f;
    m_projection[2][3] = 1.0f;
    m_projection[3][2] = zNear;
}

void Camera::SetPosition(const float3& pos)
{
    m_pos = pos;
}

void Camera::SetRotation(const float3& rotation)
{
    m_rotation = rotation;
}

void Camera::SetFOV(float fov)
{
    m_fov = fov;
    SetPerspective(m_aspectRatio, m_fov, m_zNear);
}

void Camera::EnableJitter(bool value)
{
    if (m_enableJitter != value)
    {
        m_enableJitter = value;

        UpdateJitter();
        UpdateMatrix();
    }
}

void Camera::Tick(float deltaTime)
{
    m_moved = false;

    UpdateCameraRotation(deltaTime);
    UpdateCameraPosition(deltaTime);

    UpdateJitter();
    UpdateMatrix();

    if (!m_frustumLocked)
    {
        m_frustumViewPos = m_pos;
        UpdateFrustumPlanes(m_viewProjectionJitter);
    }
}

void Camera::SetupCameraCB(CameraConstant& cameraCB)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    
    cameraCB.m_cameraPos = GetPosition();
    cameraCB.m_spreadAngle = atanf(2.0f * tanf(degree_to_radian(m_fov) * 0.5f) / pRenderer->GetRenderHeight()); //< "Texture Level-of-Detail Strategies for Real-Time Ray Tracing", Eq.20
    cameraCB.m_nearZ = m_zNear;
    cameraCB.m_linearZParams = CalcDepthLinearizationParams(m_projection);
    cameraCB.m_jitter = m_jitter;
    cameraCB.m_prevJitter = m_prevJitter;

    cameraCB.m_mtxView = m_view;
    cameraCB.m_mtxViewInverse = inverse(m_view);
    cameraCB.m_mtxProjection = m_projectionJitter;
    cameraCB.m_mtxProjectionInverse = inverse(m_projectionJitter);
    cameraCB.m_mtxViewProjection = m_viewProjectionJitter;
    cameraCB.m_mtxViewProjectionInverse = inverse(m_viewProjectionJitter);

    cameraCB.m_mtxViewProjectionNoJitter = m_viewProjection;
    cameraCB.m_mtxPrevViewProjectionNoJitter = m_prevViewProjectionJitter;
    cameraCB.m_mtxClipToPrevClipNoJitter = mul(m_prevViewProjection, inverse(m_viewProjection));
    cameraCB.m_mtxClipToPrevViewNoJitter = mul(m_prevView, inverse(m_viewProjection));

    cameraCB.m_mtxPrevView = m_prevView;
    cameraCB.m_mtxPrevViewProjection = m_prevViewProjectionJitter;
    cameraCB.m_mtxPrevViewProjectionInverse = inverse(m_prevViewProjectionJitter);

    cameraCB.m_culling.m_viewPos = m_frustumViewPos;

    cameraCB.m_physicalCamera.m_aperture = m_aperture;
    cameraCB.m_physicalCamera.m_shutterSpeed = 1.0f / m_shutterSpeed;
    cameraCB.m_physicalCamera.m_iso = m_iso;
    cameraCB.m_physicalCamera.m_exposureCompensation = m_exposureCompensation;

    for (int i = 0; i < 6; ++i)
    {
        cameraCB.m_culling.m_planes[i] = m_frustumPlanes[i];
    }
}

void Camera::UpdateJitter()
{
    if (m_enableJitter)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
        uint64_t frameIndex = pRenderer->GetFrameID();

        m_prevJitter = m_jitter;

        switch (pRenderer->GetTemporalUpscaleMode())
        {
            case TemporalSuperResolution::FSR2:
            case TemporalSuperResolution::DLSS:
            case TemporalSuperResolution::XeSS:
            {
                // todo get jitter from library
                break;
            }

            default:
            {
                const float* offset = nullptr;
                float scale = 1.0f;
                // following work of Vaidyanathan et all: https://software.intel.com/content/www/us/en/develop/articles/coarse-pixel-shading-with-temporal-supersampling.html
                // Actually it is just render to low resolution and upscaling....
                static const float halton23_16[16][2] = {{0.0f, 0.0f}, {0.5f, 0.333333f}, {0.25f, 0.666667f}, {0.75f, 0.111111f}, {0.125f, 0.444444f}, {0.625f, 0.777778f}, {0.375f, 0.222222f}, {0.875f, 0.555556f}, {0.0625f, 0.888889f}, {0.5625f, 0.037037f}, {0.3125f, 0.37037f}, {0.8125f, 0.703704f}, {0.1875f, 0.148148f}, {0.6875f, 0.481481f}, {0.4375f, 0.814815f}, {0.9375, 0.259259f}};
                static const float blueNoise16[16][2] = {{ 1.5f, 0.59375f }, { 1.21875f, 1.375f }, { 1.6875f, 1.90625f }, { 0.375f, 0.84375f }, { 1.125f, 1.875f }, { 0.71875f, 1.65625f }, { 1.9375f ,0.71875f }, { 0.65625f ,0.125f }, { 0.90625f, 0.9375f }, { 1.65625f, 1.4375f }, { 0.5f, 1.28125f }, { 0.21875f, 0.0625f }, { 1.843750f, 0.312500f }, { 1.09375f, 0.5625f }, { 0.0625f, 1.21875f }, { 0.28125f, 1.65625f }};
                
                offset = halton23_16[frameIndex % 16];
                // Not sure why we need this, 
                static float2 correctionOffset = {-55, 0};
                if (correctionOffset.x == -55)
                {
                    correctionOffset = {0.0f, 0.0f};
                    for (int i = 0; i < 16; ++i)
                    {
                        correctionOffset += float2(halton23_16[i][0], halton23_16[i][1]);
                    }
                    correctionOffset /= 16.0f;
                    correctionOffset = float2(0.5f, 0.5f) - correctionOffset;
                }

                m_jitter = { (offset[0] + correctionOffset.x) * scale - 0.5f, (offset[1] + correctionOffset.y) * scale - 0.5f};
                break;
            }
        }
    }
}

void Camera::UpdateMatrix()
{
    m_prevView = m_view;
    m_prevProjection = m_projection;
    m_prevViewProjection = m_viewProjection;
    m_prevViewProjectionJitter = m_viewProjectionJitter;

    m_world = mul(translation_matrix(m_pos), rotation_matrix(rotation_quat(m_rotation)));
    m_view = inverse(m_world);
    m_viewProjection = mul(m_projection, m_view);

    if (m_enableJitter)
    {
        uint32_t width = Engine::GetInstance()->GetRenderer()->GetRenderWidth();
        uint32_t height = Engine::GetInstance()->GetRenderer()->GetRenderHeight();

        float4x4 mtxJitter = translation_matrix(float3(2.0f * m_jitter.x / (float)width, -2.0f * m_jitter.y / (float)height, 0.0f));

        m_projectionJitter = mul(mtxJitter, m_projection);
        m_prevViewProjection = mul(m_projectionJitter, m_view);
    }
    else
    {
        m_projectionJitter = m_projection;
        m_viewProjectionJitter = m_viewProjection;
    }
}

void Camera::UpdateFrustumPlanes(const float4x4& matrix)
{
    // https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
    
    // Left clipping plane
    m_frustumPlanes[0].x = matrix[0][3] + matrix[0][0];
    m_frustumPlanes[0].y = matrix[1][3] + matrix[1][0];
    m_frustumPlanes[0].z = matrix[2][3] + matrix[2][0];
    m_frustumPlanes[0].w = matrix[3][3] + matrix[3][0];
    m_frustumPlanes[0] = normalize_plane(m_frustumPlanes[0]);

    // Right clipping plane
    m_frustumPlanes[1].x = matrix[0][3] - matrix[0][0];
    m_frustumPlanes[1].y = matrix[1][3] - matrix[1][0];
    m_frustumPlanes[1].z = matrix[2][3] - matrix[2][0];
    m_frustumPlanes[1].w = matrix[3][3] - matrix[3][0];
    m_frustumPlanes[1] = normalize_plane(m_frustumPlanes[1]);

    // Top clipping plane
    m_frustumPlanes[2].x = matrix[0][3] - matrix[0][1];
    m_frustumPlanes[2].y = matrix[1][3] - matrix[1][1];
    m_frustumPlanes[2].z = matrix[2][3] - matrix[2][1];
    m_frustumPlanes[2].w = matrix[3][3] - matrix[3][1];
    m_frustumPlanes[2] = normalize_plane(m_frustumPlanes[2]);

    // Bottom clipping plane
    m_frustumPlanes[3].x = matrix[0][3] + matrix[0][1];
    m_frustumPlanes[3].y = matrix[1][3] + matrix[1][1];
    m_frustumPlanes[3].z = matrix[2][3] + matrix[2][1];
    m_frustumPlanes[3].w = matrix[3][3] + matrix[3][1];
    m_frustumPlanes[3] = normalize_plane(m_frustumPlanes[3]);

    // Far clipping plane
    m_frustumPlanes[4].x = matrix[0][2];
    m_frustumPlanes[4].y = matrix[1][2];
    m_frustumPlanes[4].z = matrix[2][2];
    m_frustumPlanes[4].w = matrix[3][2];
    m_frustumPlanes[4] = normalize_plane(m_frustumPlanes[4]);

    //Near clipping plane
    m_frustumPlanes[5].x = matrix[0][3] - matrix[0][2];
    m_frustumPlanes[5].y = matrix[1][3] - matrix[1][2];
    m_frustumPlanes[5].z = matrix[2][3] - matrix[2][2];
    m_frustumPlanes[5].w = matrix[3][3] - matrix[3][2];
    m_frustumPlanes[5] = normalize_plane(m_frustumPlanes[5]);
}

void Camera::OnWindowResize(void* pWindow, uint32_t width, uint32_t height)
{
    m_aspectRatio = (float) width / height;
    SetPerspective(m_aspectRatio, m_fov, m_zNear);
}

void Camera::UpdateCameraRotation(float deltaTime)
{
    ImGuiIO& io = ImGui::GetIO();

    float2 rotateVelocity = {};

    if (!io.NavActive)
    {
        const float rotateSpeed = 120.0f;
        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickRight))
        {
            rotateVelocity.y = rotateSpeed;
        }

        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickLeft))
        {
            rotateVelocity.y = -rotateSpeed;
        }

        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickDown))
        {
            rotateVelocity.x = rotateSpeed;
        }

        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickUp))
        {
            rotateVelocity.x = -rotateSpeed;
        }
    }

    if (!io.WantCaptureMouse)
    {
        if (ImGui::IsMouseDragging(1) && !ImGui::IsKeyDown(ImGuiKey_LeftAlt))
        {
            const float rotateSpeed = 6.0f;

            rotateVelocity.x = io.MouseDelta.y * rotateSpeed;
            rotateVelocity.y = io.MouseDelta.x * rotateSpeed;
        }
    }

    rotateVelocity = lerp(m_prevRotateVelocity, rotateVelocity, 1.0f - exp(-deltaTime * 10.0f / m_rotateSmoothess));
    m_prevRotateVelocity = rotateVelocity;

    m_rotation.x = normalize_angle(m_rotation.x + rotateVelocity.x * deltaTime);
    m_rotation.y = normalize_angle(m_rotation.y + rotateVelocity.y * deltaTime);

    m_world = mul(translation_matrix(m_pos), rotation_matrix(rotation_quat(m_rotation)));

    m_moved |= length(rotateVelocity * deltaTime) > 1e-3f;
}

void Camera::UpdateCameraPosition(float deltaTime)
{
    bool moveLeft = false;
    bool moveRight = false;
    bool moveForward = false;
    bool moveBackward = false;
    float moveSpeed = m_moveSpeed;

    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantCaptureKeyboard && !io.NavActive)
    {
        moveLeft = ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_GamepadLStickLeft);
        moveRight = ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_GamepadRStickRight);
        moveForward = ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_GamepadLStickUp);
        moveBackward = ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_GamepadRStickDown);
    }

    if (!io.WantCaptureMouse)
    {
        if (!nearly_equal(io.MouseWheel, 0.0f))
        {
            moveForward = io.MouseWheel > 0.0f;
            moveBackward = io.MouseWheel < 0.0f;

            moveSpeed *= abs(io.MouseWheel);
        }

        if (ImGui::IsMouseDragging(1) && ImGui::IsKeyDown(ImGuiKey_LeftAlt))
        {
            moveForward = io.MouseDelta.y > 0.0f;
            moveBackward = io.MouseDelta.y < 0.0f;

            moveSpeed *= abs(io.MouseDelta.y) * 0.1f;
        }
    }

    float3 moveVelocity = float3(0.0f, 0.0f, 0.0f);
    if (moveForward) moveVelocity += GetForward();
    if (moveBackward) moveVelocity += GetBack();
    if (moveLeft) moveVelocity += GetLeft();
    if (moveRight) moveVelocity += GetRight();

    if (length(moveVelocity) > 0.0f)
    {
        moveVelocity = normalize(moveVelocity) * moveSpeed;
    }

    moveVelocity = lerp(m_prevMoveVelocity, moveVelocity, 1.0f - exp(-deltaTime * 10.0f / m_moveSmoothness));
    m_prevMoveVelocity = moveVelocity;

    m_pos += moveVelocity * deltaTime;
    m_world = mul(translation_matrix(m_pos), rotation_matrix(rotation_quat(m_rotation)));

    m_moved |= length(moveVelocity * deltaTime) > 1e-3f;
}



void Camera::DrawViewFrustum(IRHICommandList* pCommandList)
{
    if (!m_frustumLocked)
    {
        return;
    }

    //GPU_EVENT(pCommandList, "Camera::DrawViewFrustum");

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    RHIComputePipelineDesc psoDesc;
    psoDesc.m_pCS = pRenderer->GetShader("viewFrustum.hlsl", "main", RHIShaderType::CS, {});
    IRHIPipelineState* pPSO = pRenderer->GetPipelineState(psoDesc, "ViewFrustum PSO");

    pCommandList->SetPipelineState(pPSO);
    pCommandList->Dispatch(1, 1, 1);
}