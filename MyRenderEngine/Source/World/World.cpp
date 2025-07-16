#include "World.h"
#include "PointLight.h"
#include "GLTFLoader.h"
#include "StaticMesh.h"
#include "MeshMaterial.h"
#include "Utils/assert.h"
#include "Utils/string.h"
#include "Utils/profiler.h"
#include "Utils/log.h"
#include "Utils/guiUtil.h"
#include "Utils/parallel_for.h"
#include "tinyxml2/tinyxml2.h"
#include "EASTL/atomic.h"
#include "BillboardSprite.h"

World::World()
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    
    m_pCamera = eastl::make_unique<Camera>();
    m_pBillboardSpriteRenderer = eastl::make_unique<BillboardSpriteRenderer>(pRenderer);
}

World::~World() = default;

void World::LoadScene(const eastl::string& file)
{
    MY_INFO("Loading Scene : {}", file);

    tinyxml2::XMLDocument doc;
    if(tinyxml2::XML_SUCCESS != doc.LoadFile(file.c_str()))
    {
        MY_ASSERT(false);
        return;
    }

    ClearScene();

    tinyxml2::XMLNode* pRootNode = doc.FirstChild();
    MY_ASSERT(pRootNode != nullptr && strcmp(pRootNode->Value(), "scene") == 0);

    for (tinyxml2::XMLElement* pElement = pRootNode->FirstChildElement(); pElement != nullptr; pElement = (tinyxml2::XMLElement*)pElement->NextSibling())
    {
        CreateVisibleObject(pElement);
    }
}

void World::SaveScene(const eastl::string& file)
{

}

void World::AddObject(IVisibleObject* pObject)
{
    MY_ASSERT(pObject != nullptr);

    pObject->SetID((uint32_t) m_objects.size());
    m_objects.push_back(eastl::unique_ptr<IVisibleObject>(pObject));
}

void World::Tick(float deltaTime)
{
    CPU_EVENT("Tick", "World::Tick");

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    m_pCamera->Tick(deltaTime);

    for (auto iter = m_objects.begin(); iter != m_objects.end(); ++iter)
    {
        (*iter)->Tick(deltaTime);
    }

    if (pRenderer->GetOutputType() != RendererOutput::Physics)
    {
        eastl::vector<IVisibleObject*> visibleObjects(m_objects.size());
        eastl::atomic<uint32_t> visibleCount{ 0 };

        ParallelFor((uint32_t)m_objects.size(), [&](uint32_t i)
            {
                if (m_objects[i]->FrustumCull(m_pCamera->GetFrustumPlanes(), 6))
                {                   
                    // Maybe I should use a temporal vector to store objects every thread 
                    uint32_t index = visibleCount.fetch_add(1);
                    visibleObjects[index] = m_objects[i].get();
                }
            });

        visibleObjects.resize(visibleCount);

        for (auto* iter = visibleObjects.begin(); iter != visibleObjects.end(); ++iter)
        {
            (*iter)->Render(pRenderer);
        }
    }

    m_pBillboardSpriteRenderer->Render();
}

IVisibleObject* World::GetVisibleObject(uint32_t index) const
{
    if (index > m_objects.size())
    {
        MY_ASSERT(false);
        return nullptr;
    }

    return m_objects[index].get();
}

void World::ClearScene()
{
    m_objects.clear();
}

inline float3 str_to_float3(const eastl::string& str)
{
    eastl::vector<float> v;
    v.reserve(3);
    string_to_float_array(str, v);
    return float3(v[0], v[1], v[2]);
}

inline void LoadVisibleObject(tinyxml2::XMLElement* pElement, IVisibleObject* pObject)
{
    const tinyxml2::XMLAttribute* pPosition = pElement->FindAttribute("position");
    if (pPosition)
    {
        pObject->SetPosition(str_to_float3(pPosition->Value()));
    }

    const tinyxml2::XMLAttribute* pRotation = pElement->FindAttribute("rotation");
    if (pRotation)
    {
        pObject->SetRotation(rotation_quat(str_to_float3(pRotation->Value())));
    }

    const tinyxml2::XMLAttribute* pScale = pElement->FindAttribute("scale");
    if (pScale)
    {
        pObject->SetScale(str_to_float3(pScale->Value()));
    }
}

void World::CreateVisibleObject(tinyxml2::XMLElement* pElement)
{
    if (strcmp(pElement->Value(), "camera") == 0)
    {
        CreateCamera(pElement);
    }
    else if (strcmp(pElement->Value(), "model") == 0)
    {
        CreateModel(pElement);
    }
    else if (strcmp(pElement->Value(), "light") == 0)
    {
        CreateLight(pElement);
    }
}

inline void LoadLight(tinyxml2::XMLElement* pElement, ILight* pLight)
{
    LoadVisibleObject(pElement, pLight);

    const tinyxml2::XMLAttribute* pIntensity = pElement->FindAttribute("intensity");
    if (pIntensity)
    {
        pLight->SetLightIntensity(pIntensity->FloatValue());
    }

    const tinyxml2::XMLAttribute* pColor = pElement->FindAttribute("color");
    if (pColor)
    {
        pLight->SetLightColor(str_to_float3(pColor->Value()));
    }

    const tinyxml2::XMLAttribute* pRadius = pElement->FindAttribute("radius");
    if (pRadius)
    {
        pLight->SetLightRadius(pRadius->FloatValue());
    }

    const tinyxml2::XMLAttribute* pFalloff = pElement->FindAttribute("falloff");
    if (pFalloff)
    {
        pLight->SetLightFalloff(pFalloff->FloatValue());
    }
}

void World::CreateLight(tinyxml2::XMLElement* pElement)
{
    ILight* pLight = nullptr;

    const tinyxml2::XMLAttribute* pType = pElement->FindAttribute("type");
    MY_ASSERT(pType != nullptr);

    if (strcmp(pType->Value(), "directional") == 0)
    {
        return;
    }
    else if (strcmp(pType->Value(), "point") == 0)
    {
        pLight = new PointLight();
    }
    else
    {
        MY_ASSERT(false);
    }

    // Load light data
    LoadLight(pElement, pLight);

    if (!pLight->Create())
    {
        delete pLight;
        return;
    }

    AddObject(pLight);

    const tinyxml2::XMLAttribute* pPrimary = pElement->FindAttribute("primary");
    if (pPrimary && pPrimary->BoolValue())
    {
        m_pPrimaryLight = pLight;
    }

}

void World::CreateCamera(tinyxml2::XMLElement* pElement)
{
    const tinyxml2::XMLAttribute* pPosition = pElement->FindAttribute("position");
    if (pPosition)
    {
        m_pCamera->SetPosition(str_to_float3(pPosition->Value()));
    }

    const tinyxml2::XMLAttribute* pRotation = pElement->FindAttribute("rotation");
    if (pRotation)
    {
        m_pCamera->SetRotation(str_to_float3(pRotation->Value()));
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    uint32_t windowWidth = pRenderer->GetRenderWidth();
    uint32_t windowHeight = pRenderer->GetRenderHeight();

    m_pCamera->SetPerspective((float) windowWidth / windowHeight, pElement->FindAttribute("fov")->FloatValue(), pElement->FindAttribute("znear")->FloatValue());
}

void World::CreateModel(tinyxml2::XMLElement* pElement)
{
    // Load GLTF 
    GLTFLoader loader(this);
    loader.LoadSetting(pElement);
    loader.Load();
}

