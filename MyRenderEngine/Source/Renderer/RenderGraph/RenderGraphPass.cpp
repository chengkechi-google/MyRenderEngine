#include "RenderGraphPass.h"
#include "RenderGraph.h"
#include "../Renderer.h"
#include "EASTL/algorithm.h"

RenderGraphPassBase::RenderGraphPassBase(const eastl::string& name, RenderPassType type, DirectedAcyclicGraph& graph) : DAGNode(graph)
{
    m_name = name;
    m_type = type;
}

// todo : https://docs.microsoft.com/en-us/windows/win32/direct3d12/executing-and-synchronizing-command-lists#accessing-resources-from-multiple-command-queues
void RenderGraphPassBase::ResolveBarriers(const DirectedAcyclicGraph& graph)
{
    // Async pass, the resource transition will be happend in transition point
    if (m_type == RenderPassType::AsyncCompute)
    {
        return;
    }

    eastl::vector<DAGEdge*> edges;
    
    eastl::vector<DAGEdge*> resourceIncoming;
    eastl::vector<DAGEdge*> resourceOutgoing;

    // For debug
    if (m_name.find("Compute Test") != eastl::string::npos)
    {
        int i = 0;
    }

    graph.GetIncomingEdges(this, edges);
    for (size_t i = 0; i < edges.size(); ++i)
    {
        RenderGraphEdge* pEdge = (RenderGraphEdge*) edges[i];
        MY_ASSERT(pEdge->GetToNode() == this->GetID());

        RenderGraphResourceNode* pResourceNode = (RenderGraphResourceNode*) graph.GetNode(pEdge->GetFromNode());
        RenderGraphResource* pResource = pResourceNode->GetResource();
        
        graph.GetIncomingEdges(pResourceNode, resourceIncoming);
        graph.GetOutgoingEdges(pResourceNode, resourceOutgoing);
        MY_ASSERT(resourceIncoming.size() <= 1);
        MY_ASSERT(resourceOutgoing.size() >= 1);

        RHIAccessFlags oldState = RHIAccessBit::RHIAccessPresent;
        RHIAccessFlags newState = pEdge->GetUsage();

        RenderPassTypeFlags renderPassType = (RenderPassTypeFlags) m_type;

        // Try to find previous state from last pass which used this resource
        // todo : should merge states if possible, eg. shader resource ps + reosurce resource non-ps -> shader resource all
        if (resourceOutgoing.size() > 1)
        {
            // resource outgoing should be sorted
            for (int i = (int)resourceOutgoing.size() - 1; i >= 0; --i)
            {
                uint32_t subResource = ((RenderGraphEdge*) resourceOutgoing[i])->GetSubResource();
                DAGNodeID passID = resourceOutgoing[i]->GetToNode();
                if (subResource == pEdge->GetSubResource() && !graph.GetNode(passID)->IsCulled())
                {
                    const RenderGraphPassBase* pPass = (RenderGraphPassBase*) graph.GetNode(passID);
                    renderPassType |= (RenderPassTypeFlags) pPass->GetType();
                    if (passID < this->GetID())
                    {
                        oldState = ((RenderGraphEdge*) resourceOutgoing[i])->GetUsage();
                        break;
                    }
                }
            }
        }

        // If this resource is cross diffrent queues, use transition barrier
        if (((renderPassType & RenderPassType::Compute) || (renderPassType & RenderPassType::Graphics)) && (renderPassType & RenderPassType::AsyncCompute))
        {
            continue;
        }

        // If not found, get the state from the pass which output the resource
        if (oldState == RHIAccessBit::RHIAccessPresent)
        {
            if (resourceIncoming.empty())
            {
                MY_ASSERT(pResourceNode->GetVersion() == 0);
                oldState = pResource->GetInitialState();
            }
            else
            {
                oldState = ((RenderGraphEdge*) resourceIncoming[0])->GetUsage();
            }
        }
        
        bool isAliased = false;
        RHIAccessFlags aliasState;

        if (pResource->IsOverlapping() && pResource->GetFirstPassID() == this->GetID())
        {
            IRHIResource* pAliasedResource = pResource->GetAliasedPrevResource(aliasState);
            if (pAliasedResource)
            {
                m_disacardBarrier.push_back( {pAliasedResource, aliasState, newState | RHIAccessBit::RHIAccessDiscard} );
                isAliased = true;
            }
        }

        if (oldState != newState || isAliased)
        {
            // todo: if UAV to uav, can use simpe uav barrier for all uavs
            ResourceBarrier barrier;
            barrier.m_pResource = pResource;
            barrier.m_subResource = pEdge->GetSubResource();
            barrier.m_oldState = oldState;
            barrier.m_newState = newState;

            if (isAliased)
            {
                barrier.m_oldState |= aliasState | RHIAccessDiscard;
            }

            m_resourceBarriers.push_back(barrier);
        }
    }

    graph.GetOutgoingEdges(this, edges);     //< Clear inside
    for (size_t i = 0; i < edges.size(); ++i)
    {
        RenderGraphEdge* pEdge = (RenderGraphEdge*) edges[i];
        MY_ASSERT(pEdge->GetFromNode() == this->GetID());

        RHIAccessFlags newState = pEdge->GetUsage();
        if (newState == RHIAccessBit::RHIAccessRTV)
        {
            MY_ASSERT(dynamic_cast<RenderGraphEdgeColorAttachment*>(pEdge) != nullptr);
            RenderGraphEdgeColorAttachment* pColorRT = (RenderGraphEdgeColorAttachment*) pEdge;
            m_pColorRT[pColorRT->GetColorIndex()] = pColorRT;
        }
        else if(newState == RHIAccessBit::RHIAccessDSV || newState == RHIAccessBit::RHIAccessDSVReadOnly)
        {
            MY_ASSERT(dynamic_cast<RenderGraphEdgeDepthAttachment*>(pEdge) != nullptr);
            m_pDepthRT = (RenderGraphEdgeDepthAttachment*) pEdge;
        }
    }
}

void RenderGraphPassBase::ResolveAsyncCompute(const DirectedAcyclicGraph& graph, RenderGraphAsyncResolveContext& context)
{
    if (m_type == RenderPassType::AsyncCompute)
    {
        eastl::vector<DAGEdge*> edges;
        eastl::vector<DAGEdge*> resourceIncoming;
        eastl::vector<DAGEdge*> resourceOutgoing;

        RHIAccessFlags oldState;
        DAGNodeID smallestID = UINT32_MAX;
        graph.GetIncomingEdges(this, edges);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            RenderPassTypeFlags renderPassType= m_type;
            RenderGraphEdge* pEdge = (RenderGraphEdge*) edges[i];
            MY_ASSERT(pEdge->GetToNode() == this->GetID());
            
            RenderGraphResourceNode* pResourceNode = (RenderGraphResourceNode*) graph.GetNode(pEdge->GetFromNode());
            RenderGraphResource* pResource = pResourceNode->GetResource();
            oldState = pResource->GetInitialState();
            
            graph.GetIncomingEdges(pResourceNode, resourceIncoming);
            MY_ASSERT(resourceIncoming.size() <= 1);                    //< Should not have 2 pass write to same resource

            if (!resourceIncoming.empty())
            {
                oldState = ((RenderGraphEdge*) resourceIncoming[i])->GetUsage();
                RenderGraphPassBase* pPrePass = (RenderGraphPassBase*)graph.GetNode(resourceIncoming[0]->GetFromNode());
                renderPassType |= pPrePass->GetType();
                
                /*if (!pPrePass->IsCulled() && pPrePass->GetType() != RenderPassType::AsyncCompute)
                {
                    context.m_preGraphicsQueuePasses.push_back(pPrePass->GetID());
                }*/
            }

            // Get reosurce outgoing to find all previous pass and add a transition point, the rule is all previous pass have to be read resource only before asyc compute
            graph.GetOutgoingEdges(pResourceNode, resourceOutgoing);
            
            RHIAccessFlags newState = pEdge->GetUsage();
            if (!resourceOutgoing.empty())
            {
                // resource outgoing should be sorted
                for (int i = (int)resourceOutgoing.size() - 1; i >= 0; --i)
                {
                    uint32_t subResource = ((RenderGraphEdge*) resourceOutgoing[i])->GetSubResource();
                    DAGNodeID passID = resourceOutgoing[i]->GetToNode();
                    // Should check read or write state ti find out the wait point, but for now I assume before pass are all use read, and onlyh write to new resource
                    if (subResource == pEdge->GetSubResource() && passID < this->GetID() && !graph.GetNode(passID)->IsCulled())
                    {
                        newState |= ((RenderGraphEdge*) resourceOutgoing[i])->GetUsage();
                        if (passID < smallestID)
                        {
                            smallestID = passID;
                        }
                    }
                }
            } 

            // I don't check if the resource is crossed queue, if it is async pass, every resource transition will be happened in transition point
            //if (((renderPassType & RenderPassType::Compute) || (renderPassType & RenderPassType::Graphics)) && (renderPassType & RenderPassType::AsyncCompute))
            {
                RenderGraphPassBase* transitionPass = smallestID == UINT32_MAX ? this : (RenderGraphPassBase*) graph.GetNode(smallestID);
                if (!transitionPass->m_asyncTrasitionContext.m_bIsAsyncTrasitionPoint)
                {   
                    transitionPass->m_asyncTrasitionContext.m_bIsAsyncTrasitionPoint = true;
                    transitionPass->m_asyncTrasitionContext.m_signalValue = ++ context.m_graphicsFence;
                }

                if (oldState != newState)
                {
                    // todo: if UAV to uav, can use simpe uav barrier for all uavs
                    ResourceBarrier barrier;
                    barrier.m_pResource = pResource;
                    barrier.m_subResource = pEdge->GetSubResource();
                    barrier.m_oldState = oldState;
                    barrier.m_newState = newState;

                    transitionPass->m_asyncTrasitionContext.m_asyncTrasitionBarriers.push_back(barrier);
                }
                context.m_preGraphicsQueuePasses.push_back(transitionPass->GetID());
            }
        }

        graph.GetOutgoingEdges(this, edges);
        for (size_t i = 0; i < edges.size(); ++i)
        {
            RenderGraphEdge* pEdge = (RenderGraphEdge*) edges[i];
            MY_ASSERT(pEdge->GetFromNode() == this->GetID());

            RenderGraphResourceNode* pResourceNode = (RenderGraphResourceNode*) graph.GetNode(pEdge->GetToNode());
            graph.GetOutgoingEdges(pResourceNode, resourceOutgoing);
            
            for (size_t i = 0; i < resourceOutgoing.size(); ++i)
            {
                RenderGraphPassBase* pPostPass = (RenderGraphPassBase*) graph.GetNode(resourceOutgoing[i]->GetToNode());
                if (!pPostPass->IsCulled() && pPostPass->GetType() != RenderPassType::AsyncCompute)
                {
                    context.m_postGraphicsQueuePasses.push_back(pPostPass->GetID());
                }
            }           
        }

        context.m_computeQueuePasses.push_back(GetID());
    }
    else
    {
        if (!context.m_computeQueuePasses.empty())
        {
            if (!context.m_preGraphicsQueuePasses.empty())
            {
                DAGNodeID graphicsPassToWaitID = context.m_preGraphicsQueuePasses[0];//*eastl::max_element(context.m_preGraphicsQueuePasses.begin(), context.m_preGraphicsQueuePasses.end());     //< find final wait graphic pass
                
                RenderGraphPassBase* pGraphicsPassToWait = (RenderGraphPassBase*)graph.GetNode(graphicsPassToWaitID);
                /*if (pGraphicsPassToWait->m_signalValue == -1)
                {
                    pGraphicsPassToWait->m_signalValue = pGraphicsPassToWait->m_asyncTrasitionContext.m_signalValue;// ++ context.m_graphicsFence;
                }*/

                RenderGraphPassBase* pComputePass = (RenderGraphPassBase*) graph.GetNode(context.m_computeQueuePasses[0]);
                pComputePass->m_waitValue = pGraphicsPassToWait->m_asyncTrasitionContext.m_signalValue;//pGraphicsPassToWait->m_signalValue;

                for (size_t i = 0; i < context.m_computeQueuePasses.size(); ++i)
                {
                    RenderGraphPassBase* pComputePass = (RenderGraphPassBase*) graph.GetNode(context.m_computeQueuePasses[i]);
                    pComputePass->m_waitGraphicsPass = graphicsPassToWaitID;
                }
            }
        
            if (!context.m_postGraphicsQueuePasses.empty())
            {
                DAGNodeID graphicsPassToSignalID = *eastl::min_element(context.m_postGraphicsQueuePasses.begin(), context.m_postGraphicsQueuePasses.end()); //< first graphic pass

                RenderGraphPassBase* pComputePass = (RenderGraphPassBase*) graph.GetNode(context.m_computeQueuePasses.back());
                if (pComputePass->m_signalValue == -1)
                {
                    pComputePass->m_signalValue = ++ context.m_computeFence;
                }

                RenderGraphPassBase* pGraphicsPassToSignal = (RenderGraphPassBase*) graph.GetNode(graphicsPassToSignalID);
                pGraphicsPassToSignal->m_waitValue = pComputePass->m_signalValue;
                for (size_t i = 0; i < context.m_computeQueuePasses.size(); ++i)
                {
                    RenderGraphPassBase* pComputePass = (RenderGraphPassBase*) graph.GetNode(context.m_computeQueuePasses[i]);
                    pComputePass->m_signalGraphicsPass = graphicsPassToSignalID;
                }
            }

            context.m_computeQueuePasses.clear();
            context.m_preGraphicsQueuePasses.clear();
            context.m_postGraphicsQueuePasses.clear();
        }
    }
}

void RenderGraphPassBase::Execute(const RenderGraph& graph, RenderGraphPassExecuteContext& context)
{
    IRHICommandList* pCommandList = m_type == RenderPassType::AsyncCompute ? context.m_pComputeCommandList : context.m_pGraphicsCommandList;
    IRHICommandList* pMostCompetentCommandList = context.m_pGraphicsCommandList;

    // Flush transition point
    if (m_asyncTrasitionContext.m_bIsAsyncTrasitionPoint)
    {
        for (size_t i = 0; i < m_asyncTrasitionContext.m_asyncTrasitionBarriers.size(); ++i)
        {
            const ResourceBarrier& barrier = m_asyncTrasitionContext.m_asyncTrasitionBarriers[i];
            barrier.m_pResource->Barrier(pCommandList, barrier.m_subResource, barrier.m_oldState, barrier.m_newState);
        }

        pMostCompetentCommandList->End();
        if (m_type == RenderPassType::AsyncCompute)
        {
            pCommandList->Signal(context.m_pComputeQueueFence, context.m_initialComputeFenceValue + m_asyncTrasitionContext.m_signalValue);
            context.m_lastSignaledComputeValue = context.m_initialComputeFenceValue + m_signalValue;
        }else
        {
            pCommandList->Signal(context.m_pGraphicsQueueFence, context.m_initialGraphicsFenceValue + m_asyncTrasitionContext.m_signalValue);
            context.m_lastSignaledGraphicsValue = context.m_initialGraphicsFenceValue + m_signalValue;
        }

        pCommandList->Submit();

        pCommandList->Begin();
        context.m_pRenderer->SetupGlobalConstants(pCommandList);
    }

    // If need wait for other passes, execute the commands and add pending wait
    if (m_waitValue != -1)
    {
        pCommandList->End();
        pCommandList->Submit(); 

        pCommandList->Begin();
        context.m_pRenderer->SetupGlobalConstants(pCommandList);

        if (m_type == RenderPassType::AsyncCompute)
        {
            pCommandList->Wait(context.m_pGraphicsQueueFence, context.m_initialGraphicsFenceValue + m_waitValue); // GPU wait
        }
        else
        {
            pCommandList->Wait(context.m_pComputeQueueFence, context.m_initialComputeFenceValue + m_waitValue);
        }
    }

    for (size_t i = 0; i < m_eventNames.size(); ++i)
    {
        context.m_pGraphicsCommandList->BeginEvent(m_eventNames[i]);
        BeginMicroProfileGPUEvent(pCommandList, m_eventNames[i]);
    }

    if (!IsCulled())
    {
        GPU_EVENT(pCommandList, m_name);

        Begin(graph, pCommandList);
        ExecuteImpl(pCommandList);
        End(pCommandList);      
    }

    for (uint32_t i = 0; i < m_endEventNum; ++i)
    {
        context.m_pGraphicsCommandList->EndEvent();
        EndMicroProfileGPUEvent(context.m_pGraphicsCommandList);
    }

    if (m_signalValue != -1)
    {
        pCommandList->End();
        if (m_type == RenderPassType::AsyncCompute)
        {
            pCommandList->Signal(context.m_pComputeQueueFence, context.m_initialComputeFenceValue + m_signalValue);
            context.m_lastSignaledComputeValue = context.m_initialComputeFenceValue + m_signalValue;
        }else
        {
            pCommandList->Signal(context.m_pGraphicsQueueFence, context.m_initialGraphicsFenceValue + m_signalValue);
            context.m_lastSignaledGraphicsValue = context.m_initialGraphicsFenceValue + m_signalValue;
        }

        pCommandList->Submit();

        pCommandList->Begin();
        context.m_pRenderer->SetupGlobalConstants(pCommandList);
    }
}

void RenderGraphPassBase::Begin(const RenderGraph& graph, IRHICommandList* pCommandList)
{   
    // Discard barrier, transition previous resource from any layout to undefine to avoid compression metadata initialize
    for (size_t i = 0; i < m_disacardBarrier.size(); ++i)
    {
        const AliasDiscardBarrier& barrier = m_disacardBarrier[i];
        if (barrier.m_pResource->IsTexture())
        {
            pCommandList->TextureBarrier((IRHITexture*) barrier.m_pResource, RHI_ALL_SUB_RESOURCE, barrier.m_accessBefore, barrier.m_accessAfter);
        }
        else
        {
            pCommandList->BufferBarrier((IRHIBuffer*) barrier.m_pResource, barrier.m_accessBefore, barrier.m_accessAfter);
        }
    }

    for (size_t i = 0; i < m_resourceBarriers.size(); ++i)
    {
        const ResourceBarrier& barrier = m_resourceBarriers[i];
        barrier.m_pResource->Barrier(pCommandList, barrier.m_subResource, barrier.m_oldState, barrier.m_newState);
    }

    if (HasRHIRenderPass())
    {
        RHIRenderPassDesc desc;
        for (int i = 0; i < RHI_MAX_RENDER_TARGET_ACCOUNT; ++i)
        {
            if (m_pColorRT[i] != nullptr)
            {
                RenderGraphResourceNode* pNode = (RenderGraphResourceNode*) graph.GetDAG().GetNode(m_pColorRT[i]->GetToNode());
                IRHITexture* pTexture = ((RGTexture*) pNode->GetResource())->GetTexture();

                uint32_t mip, slice;
                DecomposeSubresource(pTexture->GetDesc(), m_pColorRT[i]->GetSubResource(), mip, slice);
                
                desc.m_color[i].m_pTexture = pTexture;
                desc.m_color[i].m_mipSlice = mip;
                desc.m_color[i].m_arraySlice = slice;
                desc.m_color[i].m_loadOp = m_pColorRT[i]->GetLoadOp();
                desc.m_color[i].m_storeOp = pNode->IsCulled() ? RHIRenderPassStoreOp::DontCare : RHIRenderPassStoreOp::Store;
                memcpy(desc.m_color[i].m_clearColor, m_pColorRT[i]->GetClearColor(), sizeof(float) * 4);
            }
        }

        if (m_pDepthRT != nullptr)
        {
            RenderGraphResourceNode* pNode = (RenderGraphResourceNode*) graph.GetDAG().GetNode(m_pDepthRT->GetToNode());
            IRHITexture* pTexture = ((RGTexture*) pNode->GetResource())->GetTexture();

            uint32_t mip, slice;
            DecomposeSubresource(pTexture->GetDesc(), m_pDepthRT->GetSubResource(), mip, slice);

            desc.m_depth.m_pTexture = pTexture;
            desc.m_depth.m_mipSlice = mip;
            desc.m_depth.m_arraySlice = slice;
            desc.m_depth.m_loadOp = m_pDepthRT->GetDepthLoadOp();
            desc.m_depth.m_storeOp = pNode->IsCulled() ? RHIRenderPassStoreOp::DontCare : RHIRenderPassStoreOp::Store;
            desc.m_depth.m_stencilLoadOp = m_pDepthRT->GetStencilLoadOp();
            desc.m_depth.m_stencilStoreOp = pNode->IsCulled() ? RHIRenderPassStoreOp::DontCare : RHIRenderPassStoreOp::Store;
            desc.m_depth.m_clearDepth = m_pDepthRT->GetClearDepth();
            desc.m_depth.m_clearStencil = m_pDepthRT->GetClearStencil();
            desc.m_depth.m_readOnly = m_pDepthRT->IsReadOnly();
        }

        pCommandList->BeginRenderPass(desc);
    }
}

void RenderGraphPassBase::End(IRHICommandList* pCommandList)
{
    if (HasRHIRenderPass())
    {
        pCommandList->EndRenderPass();
    }
}

bool RenderGraphPassBase::HasRHIRenderPass() const
{
    for (int i = 0; i < RHI_MAX_RENDER_TARGET_ACCOUNT; ++i)
    {
        if (m_pColorRT[i] != nullptr)
        {
            return true;
        }
    }

    return m_pDepthRT != nullptr;
}