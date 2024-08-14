#pragma once
#include "RenderGraph.h"
#include "RenderGraphBuilder.h"

class RenderGraphEdge : public DAGEdge
{
public:
    RenderGraphEdge(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to, RHIAccessFlags usage, uint32_t subresource) :
        DAGEdge(graph, from, to)
    {
        m_usage = usage;
        m_subresource = subresource;
    }

    RHIAccessFlags GetUsage() const { return m_usage; }
    uint32_t GetSubResource() const { return m_subresource; }

private:
    RHIAccessFlags m_usage;
    uint32_t m_subresource;
};

class RenderGraphResourceNode : public DAGNode
{
public:
    RenderGraphResourceNode(DirectedAcyclicGraph& graph, RenderGraphResource* pResource, uint32_t version) :
        DAGNode(graph),
        m_graph(graph)
    {
        m_pResource = pResource;
        m_version = version;
    }

    RenderGraphResource* GetResource() const { return m_pResource; }
    uint32_t GetVersion() const { return m_version; }
    
    virtual eastl::string GetGraphVizName() const override
    {
        eastl::string str = m_pResource->GetName();
        str.append("\nVersion:");
        str.append(eastl::to_string(m_version));
        if (m_version > 0)
        {
            eastl::vector<DAGEdge*> incomingEdges;
            m_graph.GetIncomingEdges(this, incomingEdges);
            MY_ASSERT(incomingEdges.size() == 1);
            uint32_t subResource = ((RenderGraphEdge*) incomingEdges[0])->GetSubResource();
            str.append("\nSubResource:");
            str.append(eastl::to_string(subResource));
        }

        return str;
    }

private:
    RenderGraphResource* m_pResource;
    uint32_t m_version;
    DirectedAcyclicGraph& m_graph;
};

class RenderGraphEdgeColorAttachment : public RenderGraphEdge
{
public:
    RenderGraphEdgeColorAttachment(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to, RHIAccessFlags usage, uint32_t subResource,
        uint32_t colorIndex, RHIRenderPassLoadOp loadOp, const float4& clearColor) :
        RenderGraphEdge(graph, from, to, usage, subResource)
    {
        m_colorIndex = colorIndex;
        m_loadOp = loadOp;
        m_clearColor[0] = clearColor[0];
        m_clearColor[1] = clearColor[1];
        m_clearColor[2] = clearColor[2];
        m_clearColor[3] = clearColor[3];
        
    }

    uint32_t GetColorIndex() const { return m_colorIndex; }
    RHIRenderPassLoadOp GetLoadOp() const { return m_loadOp; }
    const float* GetClearColor() const { return m_clearColor; }
private:
    uint32_t m_colorIndex;
    RHIRenderPassLoadOp m_loadOp;
    float m_clearColor[4] = {};
};

class RenderGraphEdgeDepthAttachment : public RenderGraphEdge
{
public:
    RenderGraphEdgeDepthAttachment(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to, RHIAccessFlags usage, uint32_t subResource,
        RHIRenderPassLoadOp depthLoadOp, RHIRenderPassLoadOp stencilLoadOp, float clearDepth, uint32_t clearStencil) :
        RenderGraphEdge(graph, from, to, usage, subResource)
    {
        m_depthLoadOp = depthLoadOp;
        m_stencilLoadOp = stencilLoadOp;
        m_clearDepth = clearDepth;
        m_clearStencil = clearStencil;
        m_readOnly = (usage & RHIAccessBit::RHIAccessDSVReadOnly) ? true : false;
    }

    RHIRenderPassLoadOp GetDepthLoadOp() const { return m_depthLoadOp; }
    RHIRenderPassLoadOp GetStencilLoadOp() const { return m_stencilLoadOp; }
    float GetClearDepth() const { return m_clearDepth; }
    uint32_t GetClearStencil() const { return m_clearStencil; }
    bool IsReadOnly() const { return m_readOnly; }

private:
    RHIRenderPassLoadOp m_depthLoadOp;
    RHIRenderPassLoadOp m_stencilLoadOp;
    float m_clearDepth;
    uint32_t m_clearStencil;
    bool m_readOnly;
};

template<typename T>
void ClassFinalizer(void* p)
{
    ((T*) p)->~T();
}

template<typename T, typename... ArgsT>
inline T* RenderGraph::Allocate(ArgsT&&... arguments)
{
    T* p = (T*) m_allocator.Alloc(sizeof(T));
    new(p) T(eastl::forward<ArgsT>(arguments)...);

    ObjFinalizer finalizer;
    finalizer.m_pObj = p;
    finalizer.finalizer = &ClassFinalizer<T>;
    m_objFinalizer.push_back(finalizer);
    
    return p;
}

template<typename T, typename... ArgsT>
inline T* RenderGraph::AllocatePOD(ArgsT&&... arguments)
{
    T* p = (T*) m_allocator.Alloc(sizeof(T));
    new (p) T(eastl::forward<ArgsT>(arguments)...);

    return p;
}

template<typename Data, typename Setup, typename Exec>
inline RenderGraphPass<Data>& RenderGraph::AddPass(const eastl::string& name, RenderPassType type, const Setup& setup, const Exec& execute)
{
    auto pass = Allocate<RenderGraphPass<Data>>(name, type, m_graph, execute);

    for (size_t i = 0; i < m_eventNames.size(); ++i)
    {
        pass->BeginEvent(m_eventNames[i]);
    }
    m_eventNames.clear();

    RGBuilder builder(this, pass);
    setup(pass->GetData(), builder);

    m_passes.push_back(pass);
    return *pass;
}

template<typename Resource>
inline RGHandle RenderGraph::Create(const typename Resource::Desc& desc, const eastl::string& name)
{
    auto resource = Allocate<Resource>(m_resourceAllocator, name, desc);
    auto node = AllocatePOD<RenderGraphResourceNode>(m_graph, resource, 0);

    RGHandle handle;
    handle.m_index = (uint16_t) m_resources.size();
    handle.m_node = (uint16_t) m_resourceNodes.size();

    m_resources.push_back(resource);
    m_resourceNodes.push_back(node);

    return handle;
}