#pragma once
#include "DirectedAcyclicGraph.h"
#include "RHI/RHI.h"
#include "Utils/assert.h"

class RenderGraphEdge;
class RenderGraphPassBase;
class RenderGraphResourceAllocator;

// todo: should move allocator to base resourc class
class RenderGraphResource
{
public:
    RenderGraphResource(const eastl::string name)
    {
        m_name = name;
    }
    virtual ~RenderGraphResource() {}

    virtual void Resolve(RenderGraphEdge* edge, RenderGraphPassBase* pass);
    virtual void Realize() = 0;
    virtual IRHIResource* GetResource() = 0;
    virtual RHIAccessFlags GetInitialState() = 0;

    const char* GetName() const { return m_name.c_str(); }
    DAGNodeID GetFirstPassID() const { return m_firstPass; }
    DAGNodeID GetLastPassID() const { return m_lastPass; }
    
    bool IsUsed() const { return m_firstPass != UINT32_MAX; }
    bool IsImported() const { return m_imported; }
    
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

class RGTexture : public RenderGraphResource
{
public:
    using Desc = RHITextureDesc;

    RGTexture(RenderGraphResourceAllocator& allocator, const eastl::string& name, const Desc& desc);
    RGTexture(RenderGraphResourceAllocator& allocator, IRHITexture* pTexture, RHIAccessFlags state);
    ~RGTexture();

    IRHITexture* GetTexture() const { return m_pTexture; }
    IRHIDescriptor* GetSRV();
    IRHIDescriptor* GetUAV();
    IRHIDescriptor* GetUAV(uint32_t mip, uint32_t arraySlice);

    virtual void Resolve(RenderGraphEdge* pEdge, RenderGraphPassBase* pPass) override;
    virtual void Realize() override;
    virtual IRHIResource* GetResource() override { return m_pTexture; }
    virtual RHIAccessFlags GetInitialState() override { return m_initialState; }
    virtual void Barrier(IRHICommandList* pCommandList, uint32_t subresource, RHIAccessFlags accessBefore, RHIAccessFlags accessAfter) override;
    virtual IRHIResource* GetAliasedPrevResource(RHIAccessFlags& lastUsedState) override;

private:
    Desc m_desc;
    IRHITexture* m_pTexture = nullptr;
    RHIAccessFlags m_initialState = RHIAccessDiscard;
    RenderGraphResourceAllocator& m_allocator;
};

class RGBuffer : public RenderGraphResource
{
public:
    using Desc = RHIBufferDesc;
    
    RGBuffer(RenderGraphResourceAllocator& allocator, const eastl::string& name, const Desc& desc);
    RGBuffer(RenderGraphResourceAllocator& allocator, IRHIBuffer* pBuffer, RHIAccessFlags state);
    ~RGBuffer();

    IRHIBuffer* GetBuffer() const { return m_pBuffer; }
    IRHIDescriptor* GetSRV();
    IRHIDescriptor* GetUAV();

    virtual void Resolve(RenderGraphEdge* pEdge, RenderGraphPassBase* pPass) override;
    virtual void Realize() override;
    virtual IRHIResource* GetResource() override { return m_pBuffer; }
    virtual RHIAccessFlags GetInitialState() override { return m_initialState; }
    virtual void Barrier(IRHICommandList* pCommandList, uint32_t subresource, RHIAccessFlags accessBefore, RHIAccessFlags accessAfter) override;
    virtual IRHIResource* GetAliasedPrevResource(RHIAccessFlags& lastUsedState) override;
    
private:
    Desc m_desc;
    IRHIBuffer* m_pBuffer = nullptr;
    RHIAccessFlags m_initialState = RHIAccessBit::RHIAccessDiscard;
    RenderGraphResourceAllocator& m_allocator;
};