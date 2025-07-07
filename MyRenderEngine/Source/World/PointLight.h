#pragma once
#include "light.h"

class PointLight : public ILight
{
public:
    virtual bool Create() override;
    virtual void Tick(float deltaTime) override;
    virtual bool FrustumCull(const float4* pPlanes, uint32_t planeCount) const override;
    virtual void OnGUI() override;
};