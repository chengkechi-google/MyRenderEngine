#pragma once
#include "Utils/math.h"
#include "EASTL/string.h"

class World;
class StaticMesh;
class MeshMaterial;
class Texture2D;

struct cgltf_data;
struct cgltf_node;
struct cgltf_primitive;
struct cgltf_material;
struct cgltf_texture_view;
struct cgltf_animation;
struct cgltf_skin;

namespace tinyxml2
{
    class XMLElement;
}

class GLTFLoader
{
public:
    GLTFLoader(World* pWorld);
    
    void LoadSetting(tinyxml2::XMLElement* pElement);
    void Load(const char* file = nullptr);

private:
    void LoadStaticMeshNode(const cgltf_data* pData, const cgltf_node* pNode, const float4x4& mtxPresentWorld);
    StaticMesh* LoadStaticMesh(const cgltf_primitive* pPrimitive, const eastl::string& name, bool bFrontFaceCCW);

    MeshMaterial* LoadMaterial(const cgltf_material* pMaterial);
    Texture2D* LoadTexture(const cgltf_texture_view& textureView, bool srgb);

private:
    World* m_pWorld = nullptr;
    eastl::string m_file;
    
    float3 m_position = float3(0.0f, 0.0f, 0.0f);
    quaternion m_rotation = quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    float3 m_scale = float3(1.0f, 1.0f, 1.0f);
    float4x4 m_mtxWorld;
};