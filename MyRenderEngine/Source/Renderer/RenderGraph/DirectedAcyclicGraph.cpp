#include "DirectedAcyclicGraph.h"
#include "Utils/assert.h"
#include <sstream>
#include <algorithm>

DAGEdge::DAGEdge(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to) :
    m_from(from->GetID()),
    m_to(to->GetID())
{
    MY_ASSERT(graph.GetNode(m_from) == from);
    MY_ASSERT(graph.GetNode(m_to) == to);

    graph.RegisterEdge(this);
}

DAGNode::DAGNode(DirectedAcyclicGraph& graph)
{
    m_id = graph.GenerateNodeID();
    graph.RegisterNode(this);
}

eastl::string DAGNode::GraphVizify() const
{
    eastl::string s;
    s.reserve(128);

    s.append("[label=\"");
    s.append(GetGraphVizName());
    s.append("\", style=filled, shape=");
    s.append(GetGraphVizShape());
    s.append(", fillcolor = ");
    s.append(GetGraphVizColor());
    s.append("]");
    s.shrink_to_fit();

    return s;
}

DAGEdge* DirectedAcyclicGraph::GetEdge(DAGNodeID from, DAGNodeID to) const
{
    for (size_t i = 0; i < m_edges.size(); ++i)
    {
        if (m_edges[i]->m_from == from && m_edges[i]->m_to == to)
        {
            return m_edges[i];
        }
    }

    return nullptr;
}

void DirectedAcyclicGraph::RegisterNode(DAGNode* node)
{
    MY_ASSERT(node->GetID() == m_nodes.size());
    m_nodes.push_back(node);
}

void DirectedAcyclicGraph::RegisterEdge(DAGEdge* edge)
{
    m_edges.push_back(edge);
}

void DirectedAcyclicGraph::Clear()
{
    m_edges.clear();
    m_nodes.clear();
}

void DirectedAcyclicGraph::Cull()
{
    // Update reference count
    for (size_t i = 0; i < m_edges.size(); ++i)
    {
        DAGEdge* edge = m_edges[i];
        DAGNode* node = m_nodes[edge->m_from];
        node->m_refCount ++;
    }

    // Cull nodes with 0 reference count
    eastl::vector<DAGNode*> stack; 
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i]->GetRefCount() == 0)
        {
            stack.push_back(m_nodes[i]);
        }
    }

    while (!stack.empty())
    {
        DAGNode* node = stack.back();
        stack.pop_back();

        eastl::vector<DAGEdge*> icoming;
        GetIncomingEdges(node, icoming);

        for (size_t i = 0; i < icoming.size(); ++i)
        {
            DAGNode* linkedNode = GetNode(icoming[i]->m_from);
            if (--linkedNode->m_refCount == 0)
            {
                stack.push_back(linkedNode);
            }
        }
    }
}

bool DirectedAcyclicGraph::IsEdgeValid(const DAGEdge* edge) const
{
    return !GetNode(edge->m_from)->IsCulled() && !GetNode(edge->m_to)->IsCulled();
}

void DirectedAcyclicGraph::GetIncomingEdges(const DAGNode* node, eastl::vector<DAGEdge*>& edges) const
{
    edges.clear();

    for (size_t i = 0; i < m_edges.size(); ++i)
    {
        if (m_edges[i]->m_to == node->GetID())
        {
            edges.push_back(m_edges[i]);
        }
    }
}

void DirectedAcyclicGraph::GetOutgoingEdges(const DAGNode* node, eastl::vector<DAGEdge*>& edges) const
{
    edges.clear();

    for (size_t i = 0; i < m_edges.size(); ++i)
    {
        if (m_edges[i]->m_from == node->GetID())
        {
            edges.push_back(m_edges[i]);
        }
    }
}

eastl::string DirectedAcyclicGraph::ExportGraphViz()
{
    std::stringstream out;

    out << "digraph {\n";
    out << "  rankdir = LR\n";
    //out << "bgcolor = black\n";
    out << "  node [fontname=\"helvetica\", fontsize=10]\n\n";

    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        uint32_t id = m_nodes[i]->GetID();
        eastl::string s = m_nodes[i]->GraphVizify();
        out << "  \"N" << id << "\" " << s.c_str() << "\n";
    }

    out << "\n";
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        DAGNode* node = m_nodes[i];
        uint32_t id = node->GetID();

        eastl::vector<DAGEdge*> edges;
        GetOutgoingEdges(node, edges);

        auto first = edges.begin();
        auto pos = std::partition(first, edges.end(),
            [this](auto const& edge) { return IsEdgeValid(edge); });

        eastl::string s = node->GetGraphVizEdgeColor();

        // render the valid edges
        if (first != pos)
        {
            out << "  N" << id << " -> { ";
            while (first != pos)
            {
                DAGNode const* ref = GetNode((*first++)->m_to);
                out << "N" << ref->GetID() << " ";
            }
            out << "} [color=" << s.c_str() << "2]\n";
        }

        // render the invalid edges
        if (first != edges.end())
        {
            out << "  N" << id << " -> { ";
            while (first != edges.end())
            {
                DAGNode const* ref = GetNode((*first++)->m_to);
                out << "N" << ref->GetID() << " ";
            }
            out << "} [color=" << s.c_str() << "4 style=dashed]\n";
        }
    }

    out << "}" << std::endl;

    return out.str().c_str();
}