#pragma once

// https://levelup.gitconnected.com/organizing-gpu-work-with-directed-acyclic-graphs-f3fd5f2c2af3
#include "EASTL/vector.h"
#include "EASTL/string.h"

using DAGNodeID = uint32_t;

class DirectedAcyclicGraph;
class DAGNode;

class DAGEdge
{
    friend class DirectedAcyclicGraph;
public:
    DAGEdge(DirectedAcyclicGraph& graph, DAGNode* from, DAGNode* to);
    virtual ~DAGEdge() {}

    DAGNodeID GetFromNode() const { return m_from; }
    DAGNodeID GetToNode() const { return m_to; }

private:
    const DAGNodeID m_from;
    const DAGNodeID m_to;
};

class DAGNode
{
    friend class DirectedAcyclicGraph;
public:
    DAGNode(DirectedAcyclicGraph& graph);
    virtual ~DAGNode() {}
    
    DAGNodeID GetID() const { return m_id; }

    // Prevents this node from being culled. Must be called before culling.
    void MakeTarget() { m_refCount = TARGET; }
    bool IsTarget() const { return m_refCount == TARGET; }
    bool IsCulled() const { return m_refCount == 0; }
    uint32_t GetRefCount() const { return IsTarget() ? 1 : m_refCount; }
    
    virtual eastl::string GetGraphVizName() const { return "unknown"; }
    virtual const char* GetGraphVizColor() const { return !IsCulled() ? "skyblue" : "skyblue4"; }
    virtual const char* GetGraphVizEdgeColor() const { return "darkolivegreen"; }
    virtual const char* GetGraphVizShape() const { return "rectangle"; }
    eastl::string GraphVizify() const;
    
private:
    DAGNodeID m_id;
    uint32_t m_refCount = 0;

    static const uint32_t TARGET = 0x80000000u;
};

class DirectedAcyclicGraph
{
public:
    DAGNodeID GenerateNodeID() { return (DAGNodeID) m_nodes.size(); }
    DAGNode* GetNode(DAGNodeID id) const { return m_nodes[id]; }
    DAGEdge* GetEdge(DAGNodeID from, DAGNodeID to) const;

    void RegisterNode(DAGNode* node);
    void RegisterEdge(DAGEdge* edge);
    
    void Clear();
    void Cull();
    bool IsEdgeValid(const DAGEdge* edge) const;
    
    void GetIncomingEdges(const DAGNode* node, eastl::vector<DAGEdge*>& edges);
    void GetOutgoingEdges(const DAGNode* node, eastl::vector<DAGEdge*>& edges);

    eastl::string ExportGraphViz();
private:
    eastl::vector<DAGNode*> m_nodes;
    eastl::vector<DAGEdge*> m_edges;
};
