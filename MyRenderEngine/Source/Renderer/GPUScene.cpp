#include "GPUScene.h"
#include "Renderer.h"

#define MAX_CONSTANT_BUFFER_SIZE (8 * 1024 * 1024) //< 8 MB
#define ALLOCATION_ALIGNMENT (4)

GPUScene::GPUScene(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    const uint32_t staticBufferSize = 448 * 1024 * 1024; 
    m_pSceneStaticBuffer.reset(pRenderer->CreateRawBuffer(nullptr, staticBufferSize, "GPUScene::m_pSceneStaticBuffer"));
    m_pSceneStaticBufferAllocator = eastl::make_unique<OffsetAllocator::Allocator>(staticBufferSize);

    const uint32_t animationBufferSize = 32 * 1024 * 1024;
    m_pSceneAnimationBuffer.reset(pRenderer->CreateRawBuffer(nullptr, animationBufferSize, "GPUScene::m_pSceneAnimationBuffer", RHIMemoryType::GPUOnly, true));
    m_pSceneAnimationBufferAllocator = eastl::make_unique<OffsetAllocator::Allocator>(animationBufferSize);

    for (int i = 0; i < RHI_MAX_INFLIGHT_FRAMES; ++i)
    {
        m_pConstantBuffer[i].reset(pRenderer->CreateRawBuffer(nullptr, MAX_CONSTANT_BUFFER_SIZE, "GPUScene::m_pConstantBuffer", RHIMemoryType::CPUToGPU));
    }
}

GPUScene::~GPUScene()
{

}

OffsetAllocator::Allocation GPUScene::AllocateStaticBuffer(uint32_t size)
{
    // todo: resize
    return m_pSceneAnimationBufferAllocator->allocate(RoundUpPow2(size, ALLOCATION_ALIGNMENT));
}

void GPUScene::FreeStaticBuffer(OffsetAllocator::Allocation allocation)
{
    if (allocation.offset >= m_pSceneStaticBuffer->GetBuffer()->GetDesc().m_size)
    {
        MY_ASSERT(false);
        return;
    }

    m_pSceneStaticBufferAllocator->free(allocation);
}

OffsetAllocator::Allocation GPUScene::AllocateAnimationBuffer(uint32_t size)
{
    // todo: resize
    return m_pSceneAnimationBufferAllocator->allocate(RoundUpPow2(size, ALLOCATION_ALIGNMENT));
}

void GPUScene::FreeAnimationBuffer(OffsetAllocator::Allocation allocation)
{
    if (allocation.offset >= m_pSceneAnimationBuffer->GetBuffer()->GetDesc().m_size)
    {
        MY_ASSERT(false);
        return;
    }

    m_pSceneAnimationBufferAllocator->free(allocation);
}

void GPUScene::Update()
{
    uint32_t instanceCount = (uint32_t) m_instanceData.size();
    m_instanceDataAddress = m_pRenderer->AllocateSceneConstant(m_instanceData.data(), sizeof(InstanceData) * instanceCount);

    uint32_t rtInstanceCount = (uint32_t) m_rayTracingInstances.size();
    if (m_pSceneTLAS == nullptr || m_pSceneTLAS->GetDesc().m_instanceCount < rtInstanceCount)
    {
        RHIRayTracingTLASDesc desc;
        desc.m_instanceCount = max(rtInstanceCount, 1);
        desc.m_flags = RHIRayTracingASFlagBit::RHIRayTracingASFlagPreferFastBuild;

        IRHIDevice* pDevice = m_pRenderer->GetDevice();
        m_pSceneTLAS.reset(pDevice->CreateRayTracongTLAS(desc, "GPUScene::m_pSceneTLAS"));

        RHIShaderResourceViewDesc srvDesc;
        srvDesc.m_type = RHIShaderResourceViewType::RayTracingTLAS;
        m_pSceneTLASSRV.reset(pDevice->CreateShaderResourceView(m_pSceneTLAS.get(), srvDesc, "GPUScene::m_pSceneTLAS"));
    }
}

void GPUScene::BuildRayTracingAS(IRHICommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "BuildTLAS");

    pCommandList->BuildRayTracingTLAS(m_pSceneTLAS.get(), m_rayTracingInstances.data(), (uint32_t) m_rayTracingInstances.size());
    pCommandList->GlobalBarrier(RHIAccessMaskAS, RHIAccessMaskSRV);

    m_rayTracingInstances.clear();
}

uint32_t GPUScene::AllocateConstantBuffer(uint32_t size)
{
    MY_ASSERT(m_constantBufferOffset + size <= MAX_CONSTANT_BUFFER_SIZE);

    uint32_t address = m_constantBufferOffset;
    m_constantBufferOffset += RoundUpPow2(size, ALLOCATION_ALIGNMENT);

    return address;
}

uint32_t GPUScene::AddInstance(const InstanceData& data, IRHIRayTracingBLAS* pBLAS, RHIRayTracingInstanceFlags flags)
{
    m_instanceData.push_back(data);
    uint32_t instanceID = (uint32_t) m_instanceData.size() - 1;

    if (pBLAS)
    {
        float4x4 transform = transpose(data.mtxWorld);
        
        RHIRayTracingInstance instance;
        instance.m_pBLAS = pBLAS;
        memcpy(instance.m_transform, &transform, sizeof(float) * 12); // 3 * 4 matrix
        instance.m_instanceID = instanceID;
        instance.m_instanceMask = 0xFF; // todo:
        instance.m_flags = flags;

        m_rayTracingInstances.push_back(instance);
    }

    return instanceID;
}

void GPUScene::ResetFrameData()
{
    m_instanceData.clear();
    m_constantBufferOffset = 0;
}

void GPUScene::BeginAnimationUpdate(IRHICommandList* pCommandList)
{
    pCommandList->BufferBarrier(m_pSceneAnimationBuffer->GetBuffer(), RHIAccessBit::RHIAccessVertexShaderSRV | RHIAccessBit::RHIAccessASRead, RHIAccessBit::RHIAccessComputeShaderUAV);
}

void GPUScene::EndAnimationUpdate(IRHICommandList* pCommandList)
{
    pCommandList->BufferBarrier(m_pSceneAnimationBuffer->GetBuffer(), RHIAccessBit::RHIAccessComputeShaderUAV, RHIAccessBit::RHIAccessVertexShaderSRV | RHIAccessBit::RHIAccessASRead);
}

IRHIBuffer* GPUScene::GetSceneConstantBuffer() const
{
    uint32_t frameIndex = m_pRenderer->GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;
    return m_pConstantBuffer[frameIndex]->GetBuffer();
}

IRHIDescriptor* GPUScene::GetSceneConstantSRV() const
{
    uint32_t frameIndex = m_pRenderer->GetFrameID() % RHI_MAX_INFLIGHT_FRAMES;
    return m_pConstantBuffer[frameIndex]->GetSRV();
}