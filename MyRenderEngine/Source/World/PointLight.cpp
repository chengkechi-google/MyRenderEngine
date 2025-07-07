#include "PointLight.h"
#include "ResourceCache.h"
#include "Renderer/Renderer.h"
#include "Core/Engine.h"
#include "Utils/guiUtil.h"

bool PointLight::Create()
{
    eastl::string assetPath = Engine::GetInstance()->GetAssetPath();
    m_pIconTexture = ResourceCache::GetInstance()->GetTexture2D(assetPath + "ui/point_light.png");
    return true;
}

void PointLight::Tick(float deltaTime)
{
    LocalLightData data = {};
    data.m_lightType = (uint) LocalLightType::Point;
    data.m_position = m_pos;
    data.m_radius = m_lightRadius;
    data.m_color = m_lightColor * m_lightIntensity;
    data.m_falloff = m_falloff;
    data.m_sourceRadius = m_lightSourceRadius;

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    pRenderer->AddLocalLight(data);
}

bool PointLight::FrustumCull(const float4* pPlanes, uint32_t planeCount) const
{
    return ::FrustumCull(pPlanes, planeCount, m_pos, m_lightRadius);
}

void PointLight::OnGUI()
{
    IVisibleObject::OnGUI();

    if (ImGui::CollapsingHeader("PointLight"))
    {
        // ## can be use to hide a label name but have unique ID
        // Label "Color", ID "Color##Light'
        ImGui::ColorEdit3("Color##Light", (float*)&m_lightColor, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Intensity##Light", &m_lightIntensity, 0.0f, 100.0f);
        ImGui::SliderFloat("Radius##Light", &m_lightRadius, 0.01f, 20.0f);
        ImGui::SliderFloat("Falloff##Light", &m_falloff, 1.0f, 16.0f);
    }

    float4x4 T = translation_matrix(m_pos);
    float4x4 R = rotation_matrix(m_rotation);

    Im3d::PushMatrix(mul(T, R));
    Im3d::PushSize(3.0f);
    Im3d::DrawSphere(float3(0.0f, 0.0f, 0.0f), m_lightRadius);
    Im3d::PopSize();
    Im3d::PopMatrix();
}

