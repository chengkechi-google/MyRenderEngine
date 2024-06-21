#pragma once

#include "Camera.h"
#include "VisbleObject.h"

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

    void LoadScene(const eastl::string& file);
    void SaveScene(const eastl::string& file);

    void AddObject(IVisibleObject* pObject);

    void Tick(float deltaTime);

    IVisibleObject* GetVisibleObject(uint32_t index) const;

private:
    void ClearScene();

    void CreateVisibleObject(tinyxml2::XMLElement* pElement);
    void CreateCamera(tinyxml2::XMLElement* pElement);
private:
    eastl::unique_ptr<Camera> m_pCamera;
    eastl::vector<eastl::unique_ptr<IVisibleObject>> m_objects;
};