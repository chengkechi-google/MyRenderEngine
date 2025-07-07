#pragma once
#include "Renderer/Renderer.h"
#include "VisibleObject.h"
#include "ModelConstants.hlsli"

class MeshMaterial;
class IPhysicsShape;
class IPhysicsRigidBody;

class StaticMesh : public IVisibleObject
{
    friend class GLTFLoader;
public:
    StaticMesh(const eastl::string& name);
    ~StaticMesh();

    virtual bool Create() override;
    virtual void Tick(float deltaTime) override;
    virtual void Render(Renderer* pRenderer) override;
    virtual bool FrustumCull(const float4* planes, uint32_t planeCount) const override;
    virtual void OnGUI() override;

    virtual void SetPosition(const float3& pos) override;
    virtual void SetRotation(const quaternion& rotation) override;
    virtual void SetScale(const float3& scale) override;

    //IPhysicsRigidBody* GetPhysicsBody() const { return m_pRigidBody.get(); }
    //void SetPhysicRigidBody(IPhysicsRigidBody* pBody);
    
    MeshMaterial* GetMaterial() const { return m_pMaterial.get(); }
    
private:
    void UpdateConstants();
    void Draw(RenderBatch& batch, IRHIPipelineState* pPSO);
    void Dispatch(RenderBatch& batch, IRHIPipelineState* pPSO);
    void DispatchGPUDriven(RenderBatch& batch, IRHIPipelineState* pPSO);
    void DisptachGPUDrivenWithCustomPSO(RenderBatch& batch, IRHIPipelineState* pPSO, IRHIPipelineState* pCustomPSO);
private:
    Renderer* m_pRenderer = nullptr;
    eastl::string m_name;
    eastl::unique_ptr<MeshMaterial> m_pMaterial = nullptr;
    eastl::unique_ptr<IRHIRayTracingBLAS> m_pBLAS;
    //eastl::unique_ptr<IPhysicsRigidBody> m_pRigidBody;
    //eastl::unique_ptr<IPhysicsShape> m_pShape;

    OffsetAllocator::Allocation m_posBuffer;
    OffsetAllocator::Allocation m_uvBuffer;
    OffsetAllocator::Allocation m_normalBuffer;
    OffsetAllocator::Allocation m_tangentBuffer;

    OffsetAllocator::Allocation m_meshletBuffer;
    OffsetAllocator::Allocation m_meshletVerticesBuffer;
    OffsetAllocator::Allocation m_meshletIndicesBuffer;
    uint32_t m_meshletCount = 0;

    OffsetAllocator::Allocation m_indexBuffer;
    RHIFormat m_indexBufferFormat;
    uint32_t m_indexCount = 0;
    uint32_t m_vertexCount = 0;

    InstanceData m_instanceData = {};
    uint32_t m_instanceIndex = 0;

    float3 m_center = {0.0f, 0.0f, 0.0f};
    float m_radius = 0.0f;

    bool m_bShowBoundingSphere = false;
    bool m_bShowTangent = false;
    bool m_bShowBiTangent = false;
    bool m_bShowNormal = false;
    
     
};