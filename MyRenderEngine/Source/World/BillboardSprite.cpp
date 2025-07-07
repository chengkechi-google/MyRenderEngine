#include "BillboardSprite.h"
#include "core/Engine.h"
#include "EASTL/sort.h"

BillboardSpriteRenderer::BillboardSpriteRenderer(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    RHIMeshShaderPipelineDesc desc;
    desc.m_pMS = pRenderer->GetShader("BillboardSprite.hlsl", "ms_main", RHIShaderType::MS);
    desc.m_pPS = pRenderer->GetShader("BillboardSprite.hlsl", "ps_main", RHIShaderType::PS);
    desc.m_rasterizerState.m_cullMode = RHICullMode::None;
    desc.m_depthStencilState.m_depthWrite = false;
    desc.m_depthStencilState.m_depthTest = true;
    desc.m_depthStencilState.m_depthFunc = RHICompareFunc::Greater;
    desc.m_blendState[0].m_blendEnable = true;
    desc.m_blendState[0].m_colorSrc = RHIBlendFactor::SrcAlpha;
    desc.m_blendState[0].m_colorDst = RHIBlendFactor::InvSrcAlpha;
    desc.m_blendState[0].m_alphaSrc = RHIBlendFactor::One;
    desc.m_blendState[0].m_alphaDst = RHIBlendFactor::InvSrcAlpha;
    desc.m_rtFormat[0] = pRenderer->GetSwapChain()->GetDesc().m_format;
    desc.m_depthStencilFromat = RHIFormat::D32F;
    m_pSpritePSO = pRenderer->GetPipelineState(desc, "Billboard Sprite PSO");

    desc.m_pMS = pRenderer->GetShader("BillboardSprite.hlsl", "ms_main", RHIShaderType::MS, {"OBJECT_ID_PASS=1" });
    desc.m_pPS = pRenderer->GetShader("BillboardSprite.hlsl", "ps_main", RHIShaderType::PS, {"OBJECT_ID_PASS=1" });
    desc.m_blendState[0].m_blendEnable = false;
    desc.m_rtFormat[0] = RHIFormat::R32UI;
    m_pSpriteObjectIDPSO = pRenderer->GetPipelineState(desc, "Billboard Sprite Object ID PSO");
}

BillboardSpriteRenderer::~BillboardSpriteRenderer()
{
    
}

void BillboardSpriteRenderer::AddSprite(const float3& position, float size, Texture2D* pTexture, const float4& color, uint32_t objectID)
{
    Sprite sprite = {};
    sprite.m_position = position;
    sprite.m_size = size;
    sprite.m_color = PackRGBA8Unorm(color);
    sprite.m_texture = pTexture->GetSRV()->GetHeapIndex();
    sprite.m_objectID = objectID;

    m_sprites.push_back(sprite);
}

void BillboardSpriteRenderer::Render()
{
    if (m_sprites.empty())
    {
        return;
    }

    Camera *pCamera = Engine::GetInstance()->GetWorld()->GetCamera();
    for (size_t i = 0; i < m_sprites.size(); ++ i)
    {
        m_sprites[i].m_distance = distance(m_sprites[i].m_position, pCamera->GetPosition());
    }

    eastl::sort(m_sprites.begin(), m_sprites.end(), [](const Sprite& left, const Sprite& right)
    {
        return left.m_distance > right.m_distance;
    });

    uint32_t spriteCount = (uint32_t) m_sprites.size();
    uint32_t spriteBufferAddress = m_pRenderer->AllocateSceneConstant(m_sprites.data(), sizeof(Sprite) * spriteCount);
    m_sprites.clear();

    uint32_t cb[]
    {
        spriteCount,
        spriteBufferAddress
    };

    RenderBatch& batch = m_pRenderer->AddGUIPassBatch();
    batch.m_label = "BillboardSprite";
    batch.SetPipelineState(m_pSpritePSO);
    batch.SetConstantBuffer(0, cb, sizeof(cb));
    batch.DispatchMesh(DivideRoundingUp(spriteCount, 64), 1, 1);

    if (m_pRenderer->IsEnableMouseHitTest())
    {
        RenderBatch& batch = m_pRenderer->AddObjectIDPassBatch();
        batch.m_label = "BillboardSprite ObjectID";
        batch.SetPipelineState(m_pSpriteObjectIDPSO);
        batch.SetConstantBuffer(0, cb, sizeof(cb));
        batch.DispatchMesh(DivideRoundingUp(spriteCount, 64), 1, 1);
    }
}


