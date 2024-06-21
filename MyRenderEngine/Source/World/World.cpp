#include "World.h"
#include "StaticMesh.h"
#include "MeshMaterial.h"
#include "Utils/assert.h"
#include "Utils/string.h"
#include "Utils/profiler.h"
#include "Utils/guiUtil.h"
#include "Utils/parallel_for.h"
#include "tinyxml2/tinyxml2.h"
#include "EASTL/atomic.h"

World::World()
{
    m_pCamera = eastl::make_unique<Camera>();    
}

World::~World() = default;