#pragma once
#include "RenderGraphPass.h"
#include "RenderGraphHandle.h"
#include "RenderGraphResource.h"
#include "RenderGraphResourceAllocator.h"
#include "Utils/linear_allocator.h"
#include "Utils/math.h"
#include "EASTL/unique_ptr.h"


class RenderGraphResourceNode;
class Renderer;

class RenderGraph
{
    friend class RGBuilder;

public:
    RenderGraph(Renderer* pRenderer);
    
    template<typename Data, typename Setup, typename Exec>
    RenderGraphPass<Data>& AddPass(const eastl::string& name, RenderPassType type, const Setup& setup, const Exec& execute);

    void BeginEvent(const eastl::string& name);
    void EndEvent();

    void Clear();
    void Compile();
    void Execute(Renderer* pRenderer, IRHICommandList* pCommandList, IRHICommandList* pComputeCommandList);

    void Present(const RGHandle& handle, RHIAccessFlags finalState);

    RGHandle Import(IRHITexture* pTexture, RHIAccessFlags state);
    RGHandle Import(IRHIBuffer* pBuffer, RHIAccessFlags state);

    RGTexture* GetTexture(const RGHandle& handle);
    RGBuffer* GetBuffer(const RGHandle& handle);

    const DirectedAcyclicGraph& GetDAG() const { return m_graph; }
    eastl::string Export();

private:
    template<typename T, typename... ArgsT>
    T* Allocate(ArgsT&&... arguments);

    template<typename T, typename... ArgsT>
    T* AllocatePOD(ArgsT&&... arguments);

    template<typename Resource>
    RGHandle Create(const typename Resource::Desc& desc, const eastl::string& name);

    RGHandle Read(RenderGraphPassBase* pPass, const RGHandle& input, RHIAccessFlags usage, uint32_t subresource);
    RGHandle Write(RenderGraphPassBase* pPass, const RGHandle& input, RHIAccessFlags usage, uint32_t subresource);

    RGHandle WriteColor(RenderGraphPassBase* pPass, uint32_t colorIndex, const RGHandle& input, uint32_t subresource, RHIRenderPassLoadOp loadOp, const float4& clearColor);
    RGHandle WriteDepth(RenderGraphPassBase* pPass, const RGHandle& input, uint32_t subresource, RHIRenderPassLoadOp depthLoadOp, RHIRenderPassLoadOp stencilLoadOp, float clearDepth, uint32_t clearStencil);
    RGHandle ReadDepth(RenderGraphPassBase* pPass, const RGHandle& input, uint32_t subresource);
   
private:
    LinearAllocator m_allocator { 512* 1024 };    //< 512 KByte
    RenderGraphResourceAllocator m_resourceAllocator;
    DirectedAcyclicGraph m_graph;

    eastl::vector<eastl::string> m_eventNames;
    
    eastl::unique_ptr<IRHIFence> m_pComputeQueueFence;
    uint64_t m_computeQueueFenceValue = 0;

    eastl::unique_ptr<IRHIFence> m_pGraphicsQueueFence;
    uint64_t m_graphicsQueueFenceValue = 0;

    eastl::vector<RenderGraphPassBase*> m_passes;
    eastl::vector<RenderGraphResource*> m_resources;
    eastl::vector<RenderGraphResourceNode*> m_resourceNodes;

    struct ObjFinalizer
    {
        void* m_pObj;
        void(*finalizer)(void*);
    };
    eastl::vector<ObjFinalizer> m_objFinalizer;
    
    struct PresentTarget
    {
        RenderGraphResource* m_pResource;
        RHIAccessFlags m_state;
    };
    eastl::vector<PresentTarget> m_outputResources;
};

class RenderGraphEvent
{
public:
    RenderGraphEvent(RenderGraph* pGraph, const char* name) : m_pRenderGraph(pGraph)
    {
        m_pRenderGraph->BeginEvent(name);
    }

    ~RenderGraphEvent()
    {
        m_pRenderGraph->EndEvent();
    }
private:
    RenderGraph* m_pRenderGraph;
};

#define RENDER_GRAPH_EVENT(graph, eventName) RenderGraphEvent __graph_event__(graph, eventName);

#include "RenderGraph.inl"