#pragma once
#include "Resource/RawBuffer.h"
#include "Utils/math.h"
#include "OffsetAllocator/offsetAllocator.hpp"
#include "GPUScene.hlsli"

class Renderer;

class GPUScene
{
public:
    GPUScene(Renderer* pRenderer);
    ~GPUScene();

    OffsetAllocator::Allocation AllocateStaticBuffer(uint32_t size);
    void FreeStaticBuffer(OffsetAllocator::Allocation allocation);

    OffsetAllocator::Allocation AllocateAnimationBuffer(uint32_t size);
    void FreeAnimationBuffer(OffsetAllocator::Allocation allocation);

    uint32_t AllocateConstantBuffer(uint32_t size);

    uint32_t AddInstance(const InstanceData& data, IRHIRayTracingBLAS* pBLAS, RHIRayTracingInstanceFlags flags);
    uint32_t GetInstanceCount() const { return (uint32_t)m_instanceData.size(); }

    void Update();
    void BuildRayTracingAS(IRHICommandList* pCommandList);
    void ResetFrameData();

    void BeginAnimationUpdate(IRHICommandList* pCommandList);
    void EndAnimationUpdate(IRHICommandList* pCommandList);

    IRHIBuffer* GetSceneStaticBuffer() const { return m_pSceneStaticBuffer->GetBuffer(); }
    IRHIDescriptor* GetSceneStaticBufferSRV() const { return m_pSceneStaticBuffer->GetSRV(); }
    
    IRHIBuffer* GetSceneAnimationBuffer() const { return m_pSceneAnimationBuffer->GetBuffer(); }
    IRHIDescriptor* GetSceneAnimationBufferSRV() const { return m_pSceneAnimationBuffer->GetSRV(); }
    IRHIDescriptor* GetSceneAnimationBufferUAV() const { return m_pSceneAnimationBuffer->GetUAV(); }

    IRHIBuffer* GetSceneConstantBuffer() const;
    IRHIDescriptor* GetSceneConstantSRV() const;

    uint32_t GetInstanceDataAddress() const { return m_instanceDataAddress; }
    
    IRHIDescriptor* GetRayTracingTLASSRV() const { return m_pSceneTLASSRV.get(); 
}
private:
    Renderer* m_pRenderer = nullptr;
    
    eastl::vector<InstanceData> m_instanceData;
    uint32_t m_instanceDataAddress = 0;

    eastl::unique_ptr<RawBuffer> m_pSceneStaticBuffer;
    eastl::unique_ptr<OffsetAllocator::Allocator> m_pSceneStaticBufferAllocator;

    eastl::unique_ptr<RawBuffer> m_pSceneAnimationBuffer;
    eastl::unique_ptr<OffsetAllocator::Allocator> m_pSceneAnimationBufferAllocator;

    eastl::unique_ptr<RawBuffer> m_pConstantBuffer[RHI_MAX_INFLIGHT_FRAMES];    // todo: change tp GPU memory, and only update dirty regions
    uint32_t m_constantBufferOffset = 0;

    eastl::unique_ptr<IRHIRayTracingTLAS> m_pSceneTLAS;
    eastl::unique_ptr<IRHIDescriptor> m_pSceneTLASSRV;
    eastl::vector<RHIRayTracingInstance> m_rayTracingInstances;
};