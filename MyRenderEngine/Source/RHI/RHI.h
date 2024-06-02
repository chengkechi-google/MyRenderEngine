#pragma once
#include "RHIDefines.h"
#include "RHIDevice.h"
#include "RHITexture.h"
#include "RHIBuffer.h"
#include "RHICommandList.h"
#include "RHIFence.h"
#include "RHIShader.h"
#include "RHIPipelineState.h"
#include "RHISwapChain.h"
#include "RHIDescriptor.h"
#include "RHIHeap.h"

IRHIDevice* CreateRHIDevice(const RHIDeviceDesc& desc);
void BeginMicroProfileGPUEvent(IRHICommandList* pCommandList, const eastl::string& eventName);
void EndMicroProfileGPUEvent(IRHICommandList* pCommandList);
uint32_t GetFormatRowPitch(RHIFormat format, uint32_t witdth);
uint32_t GetFormatBlockWidth(RHIFormat format);
uint32_t GetFormatBlockHeight(RHIFormat format);
uint32_t GetFormatComponentNum(RHIFormat format);
bool IsDepthFormat(RHIFormat format);
bool IsStencilFormat(RHIFormat format);
uint32_t CalcSubresource(const RHITextureDesc& desc, uint32_t mipLevel, uint32_t arraySlice);
void DecomposeSubresource(const RHITextureDesc& desc, uint32_t subresource, uint32_t& mip, uint32_t& slice);

class RenderEvent
{
public:
    RenderEvent(IRHICommandList* pCommandList, const eastl::string& eventName) : m_pCommandList(pCommandList)
    {
        m_pCommandList->BeginEvent(eventName);
    }

    ~RenderEvent()
    {
        m_pCommandList->EndEvent();
    }

private:
    IRHICommandList* m_pCommandList;
};

class MicroProfileRenderEvent
{
public:
    MicroProfileRenderEvent(IRHICommandList* pCommandList, const eastl::string& eventName) : m_pCommandList(pCommandList)
    {
        BeginMicroProfileGPUEvent(pCommandList, eventName);
    }

    ~MicroProfileRenderEvent()
    {
        EndMicroProfileGPUEvent(m_pCommandList);
    }
private:
    IRHICommandList* m_pCommandList;
};

#define GPU_EVENT_DEBUG(pCommandList, eventName) RenderEvent __render_event__(pCommandList, eventName);
#define GPU_EVENT_PROFILER(pCommandList, eventName) MicroProfileRenderEvent __mp_event__(pCommandList, eventName);
#define GPU_EVENT(pCommandList, eventName) GPU_EVENT_DEBUG(pCommandList, eventName); GPU_EVENT_PROFILER(pCommandList, eventName);


