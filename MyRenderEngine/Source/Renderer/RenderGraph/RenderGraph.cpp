#include "RenderGraph.h"
#include "Core/Engine.h"
#include "Utils/profiler.h"

RenderGraph::RenderGraph(Renderer* pRenderer) :
    m_resourceAllocator(pRenderer->GetDevice())
{
    IRHIDevice* pDevice = pRenderer->GetDevice();
    m_pComputeQueueFence.reset(pDevice->CreateFence("RenderGraph::m_pComputeQueueFence"));
    m_pGraphicsQueueFence.reset(pDevice->CreateFence("RenderGraph::m_pGraphicsQueueFence"));
}

void RenderGraph::BeginEvent(const eastl::string& name)
{
    m_eventNames.push_back(name);
}

void RenderGraph::EndEvent()
{
    if (!m_eventNames.empty())
    {
        m_eventNames.pop_back();
    }
    else
    {
        m_passes.back()->EndEvent();
    }
}

void RenderGraph::Clear()
{
    for (size_t i = 0; i < m_objFinalizer.size(); ++i)
    {
        m_objFinalizer[i].finalizer(m_objFinalizer[i].m_pObj);
    }
    m_objFinalizer.clear();

    m_graph.Clear();

    m_passes.clear();
    m_resourceNodes.clear();
    m_resources.clear();

    m_allocator.Reset();
    m_resourceAllocator.Reset();

    m_outputResources.clear();
}

void RenderGraph::Compile()
{
    CPU_EVENT("Render", "RenderGraph::Compile");

    m_graph.Cull();

    RenderGraphAsyncResolveContext context;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        RenderGraphPassBase* pPass = m_passes[i];
        if (!pPass->IsCulled())
        {
            pPass->ResolveAsyncCompute(m_graph, context);
        }
    }

    eastl::vector<DAGEdge*> edges;
    for (size_t i = 0; i < m_resourceNodes.size(); ++i)
    {
        RenderGraphResourceNode* pNode = m_resourceNodes[i];
        if (pNode->IsCulled())
        {
            continue;
        }

        RenderGraphResource* pResource = pNode->GetResource();
        m_graph.GetOutgoingEdges(pNode, edges);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            RenderGraphEdge* pEdge = (RenderGraphEdge*) edges[i];
            RenderGraphPassBase* pPass = (RenderGraphPassBase*) m_graph.GetNode(pEdge->GetToNode());

            if (!pPass->IsCulled())
            {
                pResource->Resolve(pEdge, pPass);
            }           
        }

        m_graph.GetIncomingEdges(pNode, edges); // Clear edges isnide function
        for(size_t i = 0; i < edges.size(); ++ i)
        { 
            RenderGraphEdge* pEdge = (RenderGraphEdge*) edges[i];
            RenderGraphPassBase* pPass = (RenderGraphPassBase*) m_graph.GetNode(pEdge->GetToNode());
            
            if (!pPass->IsCulled())
            {
                pResource->Resolve(pEdge, pPass);
            }
        }
    }

    for (size_t i = 0; i < m_resources.size(); ++i)
    {
        RenderGraphResource* pResource = m_resources[i];
        if (pResource->IsUsed())
        {
            pResource->Realize();
        }
    }

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        RenderGraphPassBase* pPass = m_passes[i];
        if (!pPass->IsCulled())
        {
            pPass->ResolveBarriers(m_graph);
        }
    }
}

void RenderGraph::Execute(Renderer* pRenderer, IRHICommandList* pCommandList, IRHICommandList* pComputeCommandList)
{
    CPU_EVENT("Render", "RenderGraph::Execute");
    GPU_EVENT(pCommandList, "RenderGraph");

    RenderGraphPassExecuteContext context = {};
    context.m_pRenderer = pRenderer;
    context.m_pGraphicsCommandList = pCommandList;
    context.m_pComputeCommandList = pComputeCommandList;
    context.m_pGraphicsQueueFence = m_pGraphicsQueueFence.get();
    context.m_pComputeQueueFence = m_pComputeQueueFence.get();
    context.m_initialGraphicsFenceValue = m_graphicsQueueFenceValue;
    context.m_initialComputeFenceValue = m_computeQueueFenceValue;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        RenderGraphPassBase* pPass = m_passes[i];
        pPass->Execute(*this, context);
    }

    m_computeQueueFenceValue = context.m_lastSignaledComputeValue;
    m_graphicsQueueFenceValue = context.m_lastSignaledGraphicsValue;
    
    for (size_t i = 0; i < m_outputResources.size(); ++i)
    {
        const PresentTarget& target = m_outputResources[i];
        if (target.m_pResource->GetFinalState() != target.m_state)
        {
            target.m_pResource->Barrier(pCommandList, 0, target.m_pResource->GetFinalState(), target.m_state);
            target.m_pResource->SetFinalState(target.m_state);
        }
    }

    m_outputResources.clear();
}

void RenderGraph::Present(const RGHandle& handle, RHIAccessFlags finalState)
{
    MY_ASSERT(handle.IsValid());
    
    RenderGraphResource* pResource = GetTexture(handle);
    pResource->SetOutput(true);

    RenderGraphResourceNode* pNode = m_resourceNodes[handle.m_node];
    pNode->MakeTarget();

    PresentTarget target;
    target.m_pResource = pResource;
    target.m_state = finalState;

    m_outputResources.push_back(target);
}

RGTexture* RenderGraph::GetTexture(const RGHandle& handle)
{
    if (!handle.IsValid())
    {
        return nullptr;
    }

    RenderGraphResource* pResource = m_resources[handle.m_index];
    MY_ASSERT(dynamic_cast<RGTexture*>(pResource) != nullptr);
    return (RGTexture*) pResource;
}

RGBuffer* RenderGraph::GetBuffer(const RGHandle& handle)
{
    if (!handle.IsValid())
    {
        return nullptr;
    }

    RenderGraphResource* pResource = m_resources[handle.m_index];
    MY_ASSERT(dynamic_cast<RGBuffer*>(pResource) != nullptr);
    return (RGBuffer*) pResource;
}

eastl::string RenderGraph::Export()
{
    return m_graph.ExportGraphViz();
}

RGHandle RenderGraph::Import(IRHITexture* pTexture, RHIAccessFlags state)
{
    auto resource = Allocate<RGTexture>(m_resourceAllocator, pTexture, state);
    auto node = AllocatePOD<RenderGraphResourceNode>(m_graph, resource, 0);

    RGHandle handle;
    handle.m_index = (uint16_t) m_resources.size();
    handle.m_node = (uint16_t) m_resourceNodes.size();

    m_resources.push_back(resource);
    m_resourceNodes.push_back(node);

    return handle;
}

RGHandle RenderGraph::Import(IRHIBuffer* pBuffer, RHIAccessFlags state)
{
    auto resource = Allocate<RGBuffer>(m_resourceAllocator, pBuffer, state);
    auto node = AllocatePOD<RenderGraphResourceNode>(m_graph, resource, 0);

    RGHandle handle;
    handle.m_index = (uint16_t) m_resources.size();
    handle.m_node = (uint16_t) m_resourceNodes.size();

    m_resources.push_back(resource);
    m_resourceNodes.push_back(node);

    return handle;
}

RGHandle RenderGraph::Read(RenderGraphPassBase* pPass, const RGHandle& input, RHIAccessFlags usage, uint32_t subresource)
{
    MY_ASSERT(input.IsValid());
    RenderGraphResourceNode* inputNode = m_resourceNodes[input.m_node];
    AllocatePOD<RenderGraphEdge>(m_graph, inputNode, pPass, usage, subresource);
    return input;
}

RGHandle RenderGraph::Write(RenderGraphPassBase* pPass, const RGHandle& input, RHIAccessFlags usage, uint32_t subresource)
{
    MY_ASSERT(input.IsValid());
    RenderGraphResource* pResource = m_resources[input.m_index];

    RenderGraphResourceNode* inputNode = m_resourceNodes[input.m_node];
    AllocatePOD<RenderGraphEdge>(m_graph, inputNode, pPass, usage, subresource);

    RenderGraphResourceNode* outputNode = AllocatePOD<RenderGraphResourceNode>(m_graph, pResource, inputNode->GetVersion() + 1);
    AllocatePOD<RenderGraphEdge>(m_graph, pPass, outputNode, usage, subresource);

    RGHandle output;
    output.m_index = input.m_index;
    output.m_node = (uint16_t) m_resourceNodes.size();
    m_resourceNodes.push_back(outputNode);
    return output;
}

RGHandle RenderGraph::WriteColor(RenderGraphPassBase* pPass, uint32_t colorIndex, const RGHandle& input, uint32_t subresource, RHIRenderPassLoadOp loadOp, const float4& clearColor)
{
    MY_ASSERT(input.IsValid());
    RenderGraphResource* pResource = m_resources[input.m_index];

    RHIAccessFlags usage = RHIAccessBit::RHIAccessRTV;

    RenderGraphResourceNode* inputNode = m_resourceNodes[input.m_node];
    AllocatePOD<RenderGraphEdgeColorAttachment>(m_graph, inputNode, pPass, usage, subresource, colorIndex, loadOp, clearColor);

    RenderGraphResourceNode* outputNode = AllocatePOD<RenderGraphResourceNode>(m_graph, pResource, inputNode->GetVersion() + 1);
    AllocatePOD<RenderGraphEdgeColorAttachment>(m_graph, pPass, outputNode, usage, subresource, colorIndex, loadOp, clearColor);

    RGHandle output;
    output.m_index = input.m_index;
    output.m_node = (uint16_t) m_resourceNodes.size();
    m_resourceNodes.push_back(outputNode);

    return output;
}

RGHandle RenderGraph::WriteDepth(RenderGraphPassBase* pPass, const RGHandle& input, uint32_t subresource, RHIRenderPassLoadOp depthLoadOp, RHIRenderPassLoadOp stencilLoadOp, float clearDepth, uint32_t clearStencil)
{   
    MY_ASSERT(input.IsValid());
    RenderGraphResource* pResource = m_resources[input.m_index];
    
    RHIAccessFlags usage = RHIAccessBit::RHIAccessDSV;

    RenderGraphResourceNode* inputNode = m_resourceNodes[input.m_node];
    AllocatePOD<RenderGraphEdgeDepthAttachment>(m_graph, inputNode, pPass, usage, subresource, depthLoadOp, stencilLoadOp, clearDepth, clearStencil);

    RenderGraphResourceNode* outputNode = AllocatePOD<RenderGraphResourceNode>(m_graph, pResource, inputNode->GetVersion() + 1);
    AllocatePOD<RenderGraphEdgeDepthAttachment>(m_graph, pPass, outputNode, usage, subresource, depthLoadOp, stencilLoadOp, clearDepth, clearStencil);

    RGHandle output;
    output.m_index = input.m_index;
    output.m_node = (uint16_t) m_resourceNodes.size();
    m_resourceNodes.push_back(outputNode);

    return output;
}

RGHandle RenderGraph::ReadDepth(RenderGraphPassBase* pPass, const RGHandle& input, uint32_t subresource)
{
    MY_ASSERT(input.IsValid());
    RenderGraphResource* pResource = m_resources[input.m_index];

    RHIAccessFlags usage = RHIAccessBit::RHIAccessDSVReadOnly;
    
    RenderGraphResourceNode* inputNode = m_resourceNodes[input.m_node];
    AllocatePOD<RenderGraphEdgeDepthAttachment>(m_graph, inputNode, pPass, usage, subresource, RHIRenderPassLoadOp::Load, RHIRenderPassLoadOp::Load, 0.0f, 0);

    RenderGraphResourceNode* outputNode = AllocatePOD<RenderGraphResourceNode>(m_graph, pResource, inputNode->GetVersion() + 1);
    AllocatePOD<RenderGraphEdgeDepthAttachment>(m_graph, pPass, outputNode, usage, subresource, RHIRenderPassLoadOp::Load, RHIRenderPassLoadOp::Load, 0.0f, 0);

    RGHandle output;
    output.m_index = input.m_index;
    output.m_node = m_resourceNodes.size();
    m_resourceNodes.push_back(outputNode);

    return output;
}