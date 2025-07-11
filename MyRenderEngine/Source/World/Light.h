#pragma once
#include "VisibleObject.h"

class ILight : public IVisibleObject
{
public:
    ~ILight();

    virtual void Render(Renderer* pRenderer) override;

    float3 GetLightDirection() const { return m_lightDir; }
    void SetLightDirection(const float3& direction) { m_lightDir = direction; }

    float3 GetLightColor() const { return m_lightColor; }
    void SetLightColor(const float3& color) { m_lightColor = color; }

    float GetLightIntensity() const { return m_lightIntensity; }
    void SetLightIntensity(float intensity) { m_lightIntensity = intensity; }

    float GetLightRadius() const { return m_lightRadius; }
    void SetLightRadius(float radius) { m_lightRadius = radius; }

    float GetLightSourceRadius() const { return m_lightSourceRadius; }
    void SetLightSourceRadius(float radius) { m_lightSourceRadius = radius; }

    float GetLightFalloff() const { return m_falloff; }
    void SetLightFalloff(float falloff) { m_falloff = falloff; }
    
protected:
    float3 m_lightDir = {0.0f, 1.0f, 0.0f };
    float3 m_lightColor = { 1.0f, 1.0f, 1.0f };
    float m_lightIntensity = 1.0f;
    float m_lightRadius = 5.0f;
    float m_lightSourceRadius = 0.005f;
    float m_falloff = 1.0f;

    Texture2D* m_pIconTexture = nullptr;
};
