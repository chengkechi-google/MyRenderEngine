#pragma once
#include "DirectedAcyclicGraph.h"
#include "RHI/RHI.h"
#include "Utils/assert.h"

class RenderGraphEdge;
class RenderGraphPassBase;
class RenderGraphResourceAllocator;

class RenderGraphResource
{
public:
    RenderGraphResource(const eastl::string name)
    {
        m_name = name;
    }
    virtual ~RenderGraphResource();

    virtual void Resolve(RenderGraphEdge* edge, RenderGraphPassBase* pass);
    virtual void Realize() = 0;
    virtual IRHIResource* GetResource() = 0;
    virtual RHIAccessFlags GetInitialState() = 0;

    const char* GetName() const { return m_name.c_str(); }
    DAGNodeID GetFirstPassID() const { return m_firstPass; }
    DAGNodeID GetLastPassID() const { return m_lastPass; }
    
    bool IsUsed() const { return m_firstPass != UINT32_MAX; }
    bool IsImported() const { return m_imported; }\
    
    RHIAccessFlags GetFinalState() const { return m_lastState; }
    virtual void SetFinalState(RHIAccessFlags state) { m_lastState = state; }
    
    bool IsOutput() const { return m_output; }
    void SetOutput(bool output) { m_output = output; }
    
    bool IsOverlapping() const { return !IsOutput() && !IsImported(); }

    virtual IRHIResource* GetAliasedPrevResource(RHIAccessFlags& lastUsedState) = 0;
    virtual void Barrier(IRHICommandList* pCommandList, uint32_t subResource, RHIAccessFlags accessBefore, RHIAccessFlags accessAfter) = 0;
protected:
    eastl::string m_name;
    DAGNodeID m_firstPass = UINT32_MAX;
    DAGNodeID m_lastPass = 0;
    RHIAccessFlags m_lastState = RHIAccessBit::RHIAccessDiscard;
    bool m_imported = false;
    bool m_output = false; 
};
