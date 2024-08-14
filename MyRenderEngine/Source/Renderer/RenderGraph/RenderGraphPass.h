#pragma once

#include "DirectedAcyclicGraph.h"
#include "RHI/RHI.h"
#include "EASTL/functional.h"

class Renderer;
class RenderGraph;
class RenderGraphResource;
class RenderGraphEdgeColorAttachment;
class RenderGraphEdgeDepthAttachment;

enum class RenderPassType
{
    Graphics,
    Compute,
    AsyncCompute,
    Copy
};

struct RenderGraphAsyncResolveContext
{
    eastl::vector<DAGNodeID> m_computeQueuePasses;
    eastl::vector<DAGNodeID> m_preGraphicsQueuePasses;
    eastl::vector<DAGNodeID> m_postGraphicsQueuePasses;
    uint64_t m_computeFence = 0;
    uint64_t m_graphicsFence = 0;
};

struct RenderGraphPassExecuteContext
{
    Renderer* m_pRenderer;
    IRHICommandList* m_pGraphicsCommandList;
    IRHICommandList* m_pComputeCommandList;
    IRHIFence* m_pComputeQueueFence;
    IRHIFence* m_pGraphicsQueueFence;

    uint64_t m_initialComputeFenceValue;
    uint64_t m_lastSignaledComputeValue;

    uint64_t m_initialGraphicsFenceValue;
    uint64_t m_lastSignaledGraphicsValue;
};

class RenderGraphPassBase : public DAGNode
{
public:
    RenderGraphPassBase(const eastl::string& name, RenderPassType type, DirectedAcyclicGraph& graph);
    
    void ResolveBarriers(const DirectedAcyclicGraph& graph);
    void ResolveAsyncCompute(const DirectedAcyclicGraph& graph, RenderGraphAsyncResolveContext& context);
    void Execute(const RenderGraph& graph, RenderGraphPassExecuteContext& context);

    virtual eastl::string GetGraphVizName() const override { return m_name.c_str(); }
    virtual const char* GetGraphVizColor() const { return !IsCulled() ? "darkgoldenrod1" : "darkgoldenrod4"; }

    void BeginEvent(const eastl::string& name) { return m_eventNames.push_back(name); }
    void EndEvent() { ++ m_endEventNum; }
    
    RenderPassType GetType() const { return m_type; }
    DAGNodeID GetWaitGraphicsPassID() const { return m_waitGraphicsPass; }
    DAGNodeID GetSignalGraphicsPassID() const { return m_signalGraphicsPass; }

private:
    void Begin(const RenderGraph& graph, IRHICommandList* pCommandList);
    void End(IRHICommandList* pCommandList);
    
    bool HasRHIRenderPass() const;
    
    virtual void ExecuteImpl(IRHICommandList* pCommandList) = 0;

protected:
    eastl::string m_name;
    RenderPassType m_type;

    eastl::vector<eastl::string> m_eventNames;
    uint32_t m_endEventNum = 0;

    struct ResourceBarrier
    {
        RenderGraphResource* m_pResource;
        uint32_t m_subResource;
        RHIAccessFlags m_oldState;
        RHIAccessFlags m_newState;
    };
    eastl::vector<ResourceBarrier> m_resourceBarriers;

    struct AliasDiscardBarrier
    {
        IRHIResource* m_pResource;
        RHIAccessFlags m_accessBefore;
        RHIAccessFlags m_accessAfter;
    };
    eastl::vector<AliasDiscardBarrier> m_disacardBarrier;

    RenderGraphEdgeColorAttachment* m_pColorRT[RHI_MAX_RENDER_TARGET_ACCOUNT];
    RenderGraphEdgeDepthAttachment* m_pDepthRT = nullptr;

    // Only for async compute pass
    DAGNodeID m_waitGraphicsPass = UINT32_MAX;
    DAGNodeID m_signalGraphicsPass = UINT32_MAX;

    uint64_t m_signalValue = -1;
    uint64_t m_waitValue = -1;
};

template<class T>
class RenderGraphPass : public RenderGraphPassBase
{
public:
    RenderGraphPass(const eastl::string& name, RenderPassType type, DirectedAcyclicGraph& graph, const eastl::function<void(const T&, IRHICommandList*)>& execute) :
        RenderGraphPassBase(name, type, graph)
    {
        m_execute = execute;
    }

    T& GetData() { return m_parameters; }
    T const* operator->() { return &GetData(); }

private:
    void ExecuteImpl(IRHICommandList* pCommandList) override
    {
        m_execute(m_parameters, pCommandList);
    }

protected:
    T m_parameters;
    eastl::function<void(const T&, IRHICommandList*)> m_execute;
};