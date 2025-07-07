#pragma once
#include "Renderer/Renderer.h"

class BillboardSpriteRenderer
{
public:
    BillboardSpriteRenderer(Renderer* pRenderer);
    ~BillboardSpriteRenderer();

    void AddSprite(const float3& position, float size, Texture2D* pTexture, const float4& color, uint32_t objectID);
    void Render();
private:
    Renderer* m_pRenderer;
    IRHIPipelineState* m_pSpritePSO = nullptr;
    IRHIPipelineState* m_pSpriteObjectIDPSO = nullptr;

    struct Sprite
    {
        float3 m_position;
        float m_size;

        uint32_t m_color;
        uint32_t m_texture;
        uint32_t m_objectID;
        float m_distance;
    };
    eastl::vector<Sprite> m_sprites;
};