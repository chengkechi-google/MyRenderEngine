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
    Graphic,
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
    Renderer* m_renderer;
};