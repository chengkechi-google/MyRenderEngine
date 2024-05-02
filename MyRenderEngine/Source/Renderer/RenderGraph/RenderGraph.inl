#pragma once
#include "RenderGraph.h"

class RenderGraphEdge : public DAGEdge
{
public:
    RenderGraphEdge(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to, RHIAccessFlags usage, uint32_t subResource) :
        DAGEdge(graph, from, to)
    {
        m_usage = usage;
        m_subResource = subResource;
    }

    RHIAccessFlags GetUsage() const { return m_usage; }
    uint32_t GetSubResource() const { return m_subResource; }

private:
    RHIAccessFlags m_usage;
    uint32_t m_subResource;
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