#include "light.h"
#include "ResourceCache.h"
#include "BillboardSprite.h"
#include "core/Engine.h"

ILight::~ILight()
{
    ResourceCache::GetInstance()->ReleaseTexture2D(m_pIconTexture);
}

void ILight::Render(Renderer* pRenderer)
{
    if (m_pIconTexture)
    {
        World* pWorld = Engine::GetInstance()->GetWorld();
        pWorld->GetBillboardSpriteRenderer()->AddSprite(m_pos, 64, m_pIconTexture, float4(1.0f, 1.0f, 1.0f, 1.0f), m_id);
    }
}

