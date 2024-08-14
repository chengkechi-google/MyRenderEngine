#include "StaticMesh.h"
#include "MeshMaterial.h"
#include "ResourceCache.h"
#include "Core/Engine.h"
#include "Utils/guiUtil.h"

StaticMesh::StaticMesh(const eastl::string& name)
{
    m_name = name;
}

StaticMesh::~StaticMesh()
{
    ResourceCache* pCache = ResourceCache::GetInstance();

    pCache->ReleaseSceneBuffer(m_posBuffer);
    pCache->ReleaseSceneBuffer(m_uvBuffer);
    pCache->ReleaseSceneBuffer(m_normalBuffer);
    pCache->ReleaseSceneBuffer(m_tangentBuffer);

    pCache->ReleaseSceneBuffer(m_meshletBuffer);
    pCache->ReleaseSceneBuffer(m_meshletVerticesBuffer);
    pCache->ReleaseSceneBuffer(m_meshletIndicesBuffer);

    pCache->ReleaseSceneBuffer(m_indexBuffer);

    // todo:
}

bool StaticMesh::Create()
{
    // todo: need to cache BLAS for same modules

    RHIRayTracingGeometry geometry;
    geometry.m_vertexBuffer = m_pRenderer->GetSceneStaticBuffer();
    geometry.m_vertexBufferOffset = m_posBuffer.offset;
    geometry.m_vertexCount = m_vertexCount;
    geometry.m_vertexStride = sizeof(float3);
    geometry.m_vertexFormat = RHIFormat::RGB32F;
    geometry.m_indexBuffer = m_pRenderer->GetSceneStaticBuffer();
    geometry.m_indexBufferOffset = m_indexBuffer.offset;
    geometry.m_indexCount = m_indexCount;
    geometry.m_indexFormat = m_indexBufferFormat;
    geometry.m_opaque = m_pMaterial->IsAlphaTest() ? false : true;  // todo: alpha blend

    RHIRayTracingBLASDesc desc;
    desc.m_geometries.push_back(geometry);
    desc.m_flags = RHIRayTracingASFlagAllowCompaction | RHIRayTracingASFlagPreferFastTrace;

    IRHIDevice* pDevice = m_pRenderer->GetDevice();
    m_pBLAS.reset(pDevice->CreateRayTracingBLAS(desc, "BLAS : " + m_name));
    m_pRenderer->BuildRayTracingBLAS(m_pBLAS.get());

    return true;
}

void StaticMesh::Tick(float deltaTime)
{
    // todo:

    UpdateConstants();

    RHIRayTracingInstanceFlags flags = m_pMaterial->IsFrontFaceCCW() ? RHIRayTracingInstanceFlagFrontFaceCCW : 0;
    m_instanceIndex = m_pRenderer->AddInstance(m_instanceData, m_pBLAS.get(), flags);
}

void StaticMesh::UpdateConstants()
{
    m_pMaterial->UpdateConstants();

    m_instanceData.m_instanceType = (uint) InstanceType::Model;
    m_instanceData.m_indexBufferAddress = m_indexBuffer.offset;
    m_instanceData.m_indexStride = m_indexBufferFormat == RHIFormat::R32UI ? 4 : 2;
    m_instanceData.m_triangleCount = m_indexCount / 3;

    m_instanceData.m_meshletCount = m_meshletCount;
    m_instanceData.m_meshletIndicesBufferAddress = m_meshletBuffer.offset;
    m_instanceData.m_meshletVerticesBufferAddress = m_meshletVerticesBuffer.offset;
    m_instanceData.m_meshletIndicesBufferAddress = m_meshletIndicesBuffer.offset;

    m_instanceData.m_posBufferAddress = m_posBuffer.offset;
    m_instanceData.m_uvBufferAddress = m_uvBuffer.offset;
    m_instanceData.m_normalBufferAddress = m_normalBuffer.offset;
    m_instanceData.m_tangentBufferAddress = m_tangentBuffer.offset;

    m_instanceData.m_bVertexAnimation = false;
    m_instanceData.m_materialDataAddress = m_pRenderer->AllocateSceneConstant((void*)m_pMaterial->GetConstants(), sizeof(ModelMaterialConstant));
    m_instanceData.m_objectID = m_id;
    m_instanceData.m_scale = max(max(abs(m_scale.x), abs(m_scale.y)), abs(m_scale.z));

    float4x4 t = translation_matrix(m_pos);
    float4x4 r = rotation_matrix(m_rotation);
    float4x4 s = scaling_matrix(m_scale);
    float4x4 mtxWorld = mul(t, mul(r, s)); //< Scale -> Rotate -> Trasnlate

    m_instanceData.m_center = mul(mtxWorld, float4(m_center, 1.0f)).xyz();
    m_instanceData.m_radius = m_radius * m_instanceData.m_scale;
    
    m_instanceData.m_bShowBoundingSphere = m_bShowBoundingSphere;
    m_instanceData.m_bShowTangent = m_bShowTangent;
    m_instanceData.m_bShowBitangent = m_bShowBiTangent;
    m_instanceData.m_bShowNormal = m_bShowNormal;

    m_instanceData.m_mtxPrevWorld = m_instanceData.m_mtxWorld;
    m_instanceData.m_mtxWorld = mtxWorld;
    m_instanceData.m_mtxWorldInverseTranspose = transpose(inverse(mtxWorld));
}

void StaticMesh::Render(Renderer* pRenderer)
{
    RenderBatch& basePassBatch = pRenderer->AddBasePassBatch();
#if 0
    Dispatch(basePassBatch, m_pMaterial->GetMeshletPSO());
#else
    Draw(basePassBatch, m_pMaterial->GetPSO());
#endif

    if (!nearly_equal(m_instanceData.m_mtxPrevWorld, m_instanceData.m_mtxWorld))
    {
        RenderBatch& velocityPassBatch = pRenderer->AddVelocityPassBatch();
        Draw(velocityPassBatch, m_pMaterial->GetVelocityPSO());
    }

    if (pRenderer->IsEnableMouseHitTest())
    {
        RenderBatch& idPassBatch = pRenderer->AddObjectIDPassBatch();
        Draw(idPassBatch, m_pMaterial->GetIDPSO());
    }

    if (m_id == pRenderer->GetMouseHitObjectID())
    {
        RenderBatch& outlinePassBatch = pRenderer->AddForwardPassBatch();
        Draw(outlinePassBatch, m_pMaterial->GetOutlinePSO());
    }
}

bool StaticMesh::FrustumCull(const float4* planes, uint32_t planeCount) const
{
    return ::FrustumCull(planes, planeCount, m_center, m_radius);
}

void StaticMesh::Draw(RenderBatch& batch, IRHIPipelineState* pPSO)
{
    uint32_t rootConsts[1] = {m_instanceIndex};

    batch.m_label = m_name.c_str();
    batch.SetPipelineState(pPSO);
    batch.SetConstantBuffer(0, rootConsts, sizeof(rootConsts));

    batch.SetIndexBuffer(m_pRenderer->GetSceneStaticBuffer(), m_indexBuffer.offset, m_indexBufferFormat);
    batch.DrawIndexed(m_indexCount);    
}

void StaticMesh::Dispatch(RenderBatch& batch, IRHIPipelineState* pPSO)
{
    batch.m_label = m_name.c_str();
    batch.SetPipelineState(pPSO);
    batch.m_center = m_instanceData.m_center;
    batch.m_radius = m_instanceData.m_radius;
    batch.m_meshletCount = m_meshletCount;
    batch.m_instaceIndex = m_instanceIndex;
}

void StaticMesh::OnGUI()
{

}

void StaticMesh::SetPosition(const float3& pos)
{
    IVisibleObject::SetPosition(pos);
}

void StaticMesh::SetRotation(const quaternion& rotation)
{
    IVisibleObject::SetRotation(rotation);
}

void StaticMesh::SetScale(const float3& scale)
{
    IVisibleObject::SetScale(scale);
}