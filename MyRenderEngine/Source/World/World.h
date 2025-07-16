#pragma once

#include "Camera.h"
#include "Light.h"
#include "VisibleObject.h"

namespace tinyxml2
{
    class XMLElement;
}

class World
{
public:
    World();
    ~World();

    Camera* GetCamera() const { return m_pCamera.get(); }
    class BillboardSpriteRenderer* GetBillboardSpriteRenderer() const { return m_pBillboardSpriteRenderer.get(); }

    void LoadScene(const eastl::string& file);
    void SaveScene(const eastl::string& file);

    void AddObject(IVisibleObject* pObject);

    void Tick(float deltaTime);

    IVisibleObject* GetVisibleObject(uint32_t index) const;

private:
    void ClearScene();

    void CreateVisibleObject(tinyxml2::XMLElement* pElement);       
    void CreateLight(tinyxml2::XMLElement* pElement);
    void CreateCamera(tinyxml2::XMLElement* pElement);
    void CreateModel(tinyxml2::XMLElement* pElement);           //< Load GLTF file
private:
    eastl::unique_ptr<Camera> m_pCamera;
    eastl::unique_ptr<class BillboardSpriteRenderer> m_pBillboardSpriteRenderer;
    
    eastl::vector<eastl::unique_ptr<IVisibleObject>> m_objects;

    ILight* m_pPrimaryLight = nullptr;
};