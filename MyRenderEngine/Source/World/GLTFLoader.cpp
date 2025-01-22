#include "GLTFLoader.h"
#include "StaticMesh.h"
#include "MeshMaterial.h"
#include "ResourceCache.h"
#include "Core/Engine.h"
#include "Utils/string.h"
#include "Utils/fmt.h"
#include "tinyxml2/tinyxml2.h"
#include "meshoptimizer/meshoptimizer.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

inline float3 str_to_float3(const eastl::string& str)
{
    eastl::vector<float> v;
    v.reserve(3);
    string_to_float_array(str, v);
    return float3(v[0], v[1], v[2]);
}

inline void GetTransform(const cgltf_node* pNode, float4x4& matrix)
{
    float3 translation;
    float4 rotation;
    float3 scale;

    if (pNode->has_matrix)
    {
        float4x4 matrix = float4x4(pNode->matrix);
        decompose(matrix, translation, rotation, scale);
    }
    else
    {
        translation = float3(pNode->translation);
        rotation = float4(pNode->rotation);
        scale = float3(pNode->scale);
    }

    // right hand to left hand
    translation.z *= -1;
    rotation.z *= -1;
    rotation.w *= -1;  

    float4x4 T = translation_matrix(translation);
    float4x4 R = rotation_matrix(rotation);
    float4x4 S = scaling_matrix(scale);

    matrix = mul(T, mul(R, S));
}

inline bool IsFrontFaceCCW(const cgltf_node* pNode)
{
    //gltf 2.0 spec : If the determinant is a positive value, the winding order triangle faces is counterclockwise; in the opposite case, the winding order is clockwise.
    cgltf_float m[16];
    cgltf_node_transform_world(pNode, m);
    
    float4x4 matrix(m);
    return determinant(matrix) > 0.0f;
}

inline uint32_t GetMeshIndex(const cgltf_data* pData, const cgltf_mesh* pMesh)
{
    for (cgltf_size i = 0; i < pData->meshes_count; ++i)
    {
        if (&pData->meshes[i] == pMesh)
        {
            return (uint32_t) i;
        }
    }

    MY_ASSERT(false);
    return 0;
}

inline uint32_t GetNodeIndex(const cgltf_data* pData, const cgltf_node* pNode)
{
    for (cgltf_size i = 0; i < pData->nodes_count; ++i)
    {
        if (&pData->nodes[i] == pNode)
        {
            return (uint32_t) i;
        }
    }

    MY_ASSERT(false);
    return 0;
}

GLTFLoader::GLTFLoader(World* pWorld)
{
    m_pWorld = pWorld;

    float4x4 T = translation_matrix(m_position);
    float4x4 R = rotation_matrix(m_rotation);
    float4x4 S = scaling_matrix(m_scale);
    m_mtxWorld = mul(T, mul(R, S));
}

void GLTFLoader::LoadSetting(tinyxml2::XMLElement* pElement)
{
    m_file = pElement->FindAttribute("file")->Value();

    const tinyxml2::XMLAttribute* pPositionAttr = pElement->FindAttribute("position");
    if (pPositionAttr)
    {
        m_position = str_to_float3(pPositionAttr->Value());
    }

    const tinyxml2::XMLAttribute* pRotationAttr = pElement->FindAttribute("rotation");
    if (pRotationAttr)
    {
        m_rotation = rotation_quat(str_to_float3(pRotationAttr->Value()));
    }

    const tinyxml2::XMLAttribute* pScaleAttr = pElement->FindAttribute("scale");
    if (pScaleAttr)
    {
        m_scale = str_to_float3(pScaleAttr->Value());
    }

    float4x4 T = translation_matrix(m_position);
    float4x4 R = rotation_matrix(m_rotation);
    float4x4 S = scaling_matrix(m_scale);
    m_mtxWorld = mul(T, mul(R, S));
}

void GLTFLoader::Load(const char* pGLTFFile)
{
    eastl::string file = Engine::GetInstance()->GetAssetPath() + (pGLTFFile ? pGLTFFile : m_file);

    cgltf_options options = {};
    cgltf_data* pData = NULL;
    cgltf_result result = cgltf_parse_file(&options, file.c_str(), &pData);
    if (result != cgltf_result_success)
    {
        MY_ASSERT(false);
        return;
    }

    cgltf_load_buffers(&options, pData, file.c_str());

    // todo: load animation objects
    if (pData->animations_count)
    {

    }

    for (cgltf_size i = 0; i < pData->scenes_count; ++i)
    {
        for (cgltf_size node = 0; node < pData->scenes[i].nodes_count; ++ node)
        {
            LoadStaticMeshNode(pData, pData->scenes[i].nodes[node], m_mtxWorld);
        }
    }
    
    cgltf_free(pData);
}

void GLTFLoader::LoadStaticMeshNode(const cgltf_data* pData, const cgltf_node* pNode, const float4x4& mtxParentToWorld)
{
    float4x4 mtxLocalToParent;
    GetTransform(pNode, mtxLocalToParent);

    float4x4 mtxLocalToWorld = mul(mtxParentToWorld, mtxLocalToParent);

    if (pNode->mesh)
    {
        float3 position;
        float4 rotation;
        float3 scale;
        decompose(mtxLocalToWorld, position, rotation, scale);

        uint32_t meshIndex = GetMeshIndex(pData, pNode->mesh);
        bool bFrontFaceCCW = IsFrontFaceCCW(pNode);

        for (cgltf_size i = 0; i < pNode->mesh->primitives_count; ++i)
        {
            eastl::string name = fmt::format("mesh_{}_{}_{}", meshIndex, i, (pNode->mesh->name ? pNode->mesh->name : "")).c_str();
            
            StaticMesh* pMesh = LoadStaticMesh(&pNode->mesh->primitives[i], name, bFrontFaceCCW);

            pMesh->m_pMaterial->m_bFrontFaceCCW = bFrontFaceCCW;
            pMesh->SetPosition(position);
            pMesh->SetRotation(rotation);
            pMesh->SetScale(scale);
        }
    }

    for (cgltf_size i = 0; i < pNode->children_count; ++i)
    {
        LoadStaticMeshNode(pData, pNode->children[i], mtxLocalToWorld);
    }
}

Texture2D* GLTFLoader::LoadTexture(const cgltf_texture_view& textureView, bool srgb)
{
    // Sometimes no texture path on the GLTF material
    if (textureView.texture == nullptr || textureView.texture->image->uri == nullptr)
    {
        return nullptr;
    }

    size_t lastSlash = m_file.find_last_of('/');
    eastl::string path = Engine::GetInstance()->GetAssetPath() + m_file.substr(0, lastSlash + 1);
    
    Texture2D* pTexture = ResourceCache::GetInstance()->GetTexture2D(path + textureView.texture->image->uri, srgb);

    return pTexture;
}

inline MaterialTextureInfo LoadTextureInfo(const Texture2D* pTexture, const cgltf_texture_view& textureView)
{
    MaterialTextureInfo info;

    if (pTexture)
    {
        info.m_index = pTexture->GetSRV()->GetHeapIndex();
        info.m_width = pTexture->GetTexture()->GetDesc().m_width;
        info.m_height = pTexture->GetTexture()->GetDesc().m_height;

        if (textureView.has_transform)
        {
            info.m_bTransform = true;
            info.m_offset = float2(textureView.transform.offset);
            info.m_scale = float2(textureView.transform.scale);
            info.m_rotation = textureView.transform.rotation;
        }
    }

    return info;
}

MeshMaterial* GLTFLoader::LoadMaterial(const cgltf_material* pGLTFMaterial)
{
    MeshMaterial* pMaterial = new MeshMaterial;
    if (pGLTFMaterial == nullptr)
    {
        MY_ASSERT(false);
        return pMaterial;
    }

    pMaterial->m_name = pGLTFMaterial->name != nullptr ? pGLTFMaterial->name : "";

    if (pGLTFMaterial->has_pbr_metallic_roughness)
    {
        pMaterial->m_bPBRMetallicRoughness = true;
        pMaterial->m_pAlbedoTexture = LoadTexture(pGLTFMaterial->pbr_metallic_roughness.base_color_texture, true);
        pMaterial->m_materialCB.m_albedoTexture = LoadTextureInfo(pMaterial->m_pAlbedoTexture, pGLTFMaterial->pbr_metallic_roughness.base_color_texture);
        pMaterial->m_pMetallicRoughnessTexture = LoadTexture(pGLTFMaterial->pbr_metallic_roughness.metallic_roughness_texture, false);
        pMaterial->m_materialCB.m_metallicRoughnessTexture = LoadTextureInfo(pMaterial->m_pMetallicRoughnessTexture, pGLTFMaterial->pbr_metallic_roughness.metallic_roughness_texture);
        pMaterial->m_albedoColor = float3(pGLTFMaterial->pbr_metallic_roughness.base_color_factor);
        pMaterial->m_metallic = pGLTFMaterial->pbr_metallic_roughness.metallic_factor;
        pMaterial->m_roughness = pGLTFMaterial->pbr_metallic_roughness.roughness_factor;
    }
    else if (pGLTFMaterial->has_pbr_specular_glossiness)
    {
        pMaterial->m_bPBRSpecularGlossiness = true;
        pMaterial->m_pDiffuseTexture = LoadTexture(pGLTFMaterial->pbr_specular_glossiness.diffuse_texture, true);
        pMaterial->m_materialCB.m_diffuseTexture = LoadTextureInfo(pMaterial->m_pDiffuseTexture, pGLTFMaterial->pbr_specular_glossiness.diffuse_texture);
        pMaterial->m_pSpecularGlossinessTexture = LoadTexture(pGLTFMaterial->pbr_specular_glossiness.specular_glossiness_texture, true);
        pMaterial->m_materialCB.m_specularGlossinessTexture = LoadTextureInfo(pMaterial->m_pSpecularGlossinessTexture, pGLTFMaterial->pbr_specular_glossiness.specular_glossiness_texture);
        pMaterial->m_diffuseColor = float3(pGLTFMaterial->pbr_specular_glossiness.diffuse_factor);
        pMaterial->m_specularColor = float3(pGLTFMaterial->pbr_specular_glossiness.specular_factor);
        pMaterial->m_glossiness = pGLTFMaterial->pbr_specular_glossiness.glossiness_factor;
    }

    pMaterial->m_pNormalTexture = LoadTexture(pGLTFMaterial->normal_texture, false);
    pMaterial->m_materialCB.m_normalTexture = LoadTextureInfo(pMaterial->m_pNormalTexture, pGLTFMaterial->normal_texture);
    pMaterial->m_pEmissiveTexture = LoadTexture(pGLTFMaterial->emissive_texture, true);
    pMaterial->m_materialCB.m_emissiveTexture = LoadTextureInfo(pMaterial->m_pEmissiveTexture, pGLTFMaterial->emissive_texture);
    pMaterial->m_pAOTexture = LoadTexture(pGLTFMaterial->occlusion_texture, false);
    pMaterial->m_materialCB.m_aoTexture = LoadTextureInfo(pMaterial->m_pAOTexture, pGLTFMaterial->occlusion_texture);

    pMaterial->m_emissiveColor = float3(pGLTFMaterial->emissive_factor);
    pMaterial->m_alphaCutoff = pGLTFMaterial->alpha_cutoff;
    pMaterial->m_bAlphaTest = pGLTFMaterial->alpha_mode == cgltf_alpha_mode_mask;
    pMaterial->m_bAlphaBlend = pGLTFMaterial->alpha_mode == cgltf_alpha_mode_blend;
    
    if (pGLTFMaterial->has_anisotropy)
    {
        // todo: GLTF 2.0 support anisotropy, should set up the anisotrppy parameters form GFLTF aniso material
        pMaterial->m_shadingModel = ShadingModel::Anisotropy;
        pMaterial->m_pAnisotropicTagentTexture = LoadTexture(pGLTFMaterial->anisotropy.anisotropy_texture, false);
        pMaterial->m_materialCB.m_anisotrupyTexture = LoadTextureInfo(pMaterial->m_pAnisotropicTagentTexture, pGLTFMaterial->anisotropy.anisotropy_texture);
    }

    if (pGLTFMaterial->has_sheen)
    {
        pMaterial->m_shadingModel = ShadingModel::Sheen;
        pMaterial->m_pSheenColorTexture = LoadTexture(pGLTFMaterial->sheen.sheen_color_texture, true);
        pMaterial->m_materialCB.m_sheenColorTexture = LoadTextureInfo(pMaterial->m_pSheenColorTexture, pGLTFMaterial->sheen.sheen_color_texture);
        pMaterial->m_pSheenRoughnessTexture = LoadTexture(pGLTFMaterial->sheen.sheen_roughness_texture, false);
        pMaterial->m_materialCB.m_sheenRoughnessTexture = LoadTextureInfo(pMaterial->m_pSheenRoughnessTexture, pGLTFMaterial->sheen.sheen_roughness_texture);
        
        pMaterial->m_sheenColor = float3(pGLTFMaterial->sheen.sheen_color_factor);
        pMaterial->m_sheenRoughness = pGLTFMaterial->sheen.sheen_roughness_factor;
    }

    if (pGLTFMaterial->has_clearcoat)
    {
        pMaterial->m_shadingModel = ShadingModel::ClearCoat;
        pMaterial->m_pClearCoatTexture = LoadTexture(pGLTFMaterial->clearcoat.clearcoat_texture, false);
        pMaterial->m_materialCB.m_clearCoatTexture = LoadTextureInfo(pMaterial->m_pClearCoatTexture, pGLTFMaterial->clearcoat.clearcoat_texture);
        pMaterial->m_pClearCoatNormalTexture = LoadTexture(pGLTFMaterial->clearcoat.clearcoat_normal_texture, false);
        pMaterial->m_materialCB.m_clearCoatNormalTexture = LoadTextureInfo(pMaterial->m_pClearCoatNormalTexture, pGLTFMaterial->clearcoat.clearcoat_normal_texture);
        pMaterial->m_pClearCoatRoughnessTexture = LoadTexture(pGLTFMaterial->clearcoat.clearcoat_roughness_texture, false);
        pMaterial->m_materialCB.m_clearCoatRoughnessTexture = LoadTextureInfo(pMaterial->m_pClearCoatRoughnessTexture, pGLTFMaterial->clearcoat.clearcoat_roughness_texture);

        pMaterial->m_clearCoat = pGLTFMaterial->clearcoat.clearcoat_factor;
        pMaterial->m_clearCoatRoughness = pGLTFMaterial->clearcoat.clearcoat_roughness_factor;
    }

    return pMaterial;
}
meshopt_Stream LoadBufferStream(const cgltf_accessor* pAccessor, bool convertToLH, size_t& count)
{
    uint32_t stride = (uint32_t) pAccessor->stride;
    
    meshopt_Stream stream;
    stream.data = MY_ALLOC(stride * pAccessor->count);
    stream.size = stride;
    stream.stride = stride;

    void* pData = (char*) pAccessor->buffer_view->buffer->data + pAccessor->buffer_view->offset + pAccessor->offset;
    memcpy((void*)stream.data, pData, stride * pAccessor->count);
    if (convertToLH)
    {
        MY_ASSERT(stride >= sizeof(float3));
        for (uint32_t i = 0; i < (uint32_t)pAccessor->count; ++i)
        {
            float3* v = (float3*) stream.data + i;
            v->z = -v->z;
        }
    }

    count = pAccessor->count;
    return stream;
}

StaticMesh* GLTFLoader::LoadStaticMesh(const cgltf_primitive* pPrimitive, const eastl::string& name, bool bFrontFaceCCW)
{
    StaticMesh* pMesh = new StaticMesh(m_file + " " + name);
    pMesh->m_pMaterial.reset(LoadMaterial(pPrimitive->material));

    size_t indexCount;
    meshopt_Stream indices = LoadBufferStream(pPrimitive->indices, false, indexCount);

    size_t vertexCount;
    eastl::vector<meshopt_Stream> vertexStreams;
    eastl::vector<cgltf_attribute_type> vertexTypes;

    for (cgltf_size i = 0; i < pPrimitive->attributes_count; ++i)
    {
        switch (pPrimitive->attributes[i].type)
        {
            case cgltf_attribute_type_position:
            {
                vertexStreams.push_back(LoadBufferStream(pPrimitive->attributes[i].data, true, vertexCount));
                vertexTypes.push_back(pPrimitive->attributes[i].type);

                float3 min = float3(pPrimitive->attributes[i].data->min);
                min.z = -min.z;

                float3 max = float3(pPrimitive->attributes[i].data->max);
                max.z = -max.z;

                float3 center = (min + max) / 2.0f;
                float radius = length(max - min) / 2.0f;

                pMesh->m_center = center;
                pMesh->m_radius = radius;

                break;
            }

            case cgltf_attribute_type_texcoord:
            {
                if (pPrimitive->attributes[i].index == 0)       //< Only support singe uv set
                {
                    vertexStreams.push_back(LoadBufferStream(pPrimitive->attributes[i].data, false, vertexCount));
                    vertexTypes.push_back(pPrimitive->attributes[i].type);
                }
                break;
            }

            case cgltf_attribute_type_normal:
            {
                vertexStreams.push_back(LoadBufferStream(pPrimitive->attributes[i].data, false, vertexCount));
                vertexTypes.push_back(pPrimitive->attributes[i].type);
                break;
            }

            case cgltf_attribute_type_tangent:
            {
                vertexStreams.push_back(LoadBufferStream(pPrimitive->attributes[i].data, false, vertexCount));
                vertexTypes.push_back(pPrimitive->attributes[i].type);
                break;
            }
        }
    }

    eastl::vector<unsigned int> remap(indexCount);
    
    void* remappedIndices = MY_ALLOC(indices.stride * indexCount);
    eastl::vector<void*> remappedVertices;

    size_t remappedVertexCount;
    switch (indices.stride)
    {
        case 4:
            remappedVertexCount = meshopt_generateVertexRemapMulti(&remap[0], (const unsigned int*) indices.data, indexCount, vertexCount, vertexStreams.data(), vertexStreams.size());
            meshopt_remapIndexBuffer((unsigned int*) remappedIndices, (const unsigned int*) indices.data, indexCount, &remap[0]);
            break;

        case 2:
            remappedVertexCount = meshopt_generateVertexRemapMulti(&remap[0], (const unsigned short*) indices.data, indexCount, vertexCount, vertexStreams.data(), vertexStreams.size());
            meshopt_remapIndexBuffer((unsigned short*) remappedIndices, (const unsigned short*) indices.data, indexCount, &remap[0]);
            break;

        case 1:
            remappedVertexCount = meshopt_generateVertexRemapMulti(&remap[0], (const unsigned char*) indices.data, indexCount, vertexCount, vertexStreams.data(), vertexStreams.size());
            meshopt_remapIndexBuffer((unsigned char*) remappedIndices, (const unsigned char*) indices.data, indexCount, &remap[0]);
            break;

        default:
            MY_ASSERT(false);
            break;
    }

    void* pPosVertices = nullptr;
    size_t posStride = 0;

    for (size_t i = 0; i < vertexStreams.size(); ++i)
    {
        void* pVertices = MY_ALLOC(vertexStreams[i].stride * remappedVertexCount);
        meshopt_remapVertexBuffer(pVertices, vertexStreams[i].data, vertexCount, vertexStreams[i].stride, &remap[0]);

        remappedVertices.push_back(pVertices);

        if (vertexTypes[i] == cgltf_attribute_type_position)
        {
            pPosVertices = pVertices;
            posStride = vertexStreams[i].stride;
        }
    }

    size_t maxVertices = 64;
    size_t maxTriangles = 124;
    const float coneWeight = 0.5f;
    size_t maxMeshlets = meshopt_buildMeshletsBound(indexCount, maxVertices, maxTriangles);

    eastl::vector<meshopt_Meshlet> meshlets(maxMeshlets);
    eastl::vector<unsigned int> meshletVertices(maxMeshlets * maxVertices);
    eastl::vector<unsigned char> meshletTriangles(maxMeshlets * maxTriangles * 3);

    size_t meshletCount;
    switch (indices.stride)
    {
        case 4:
        {
            meshletCount = meshopt_buildMeshlets(meshlets.data(), meshletVertices.data(), meshletTriangles.data(),
                (const unsigned int*) remappedIndices, indexCount,
                (const float*) pPosVertices, remappedVertexCount, posStride,
                maxVertices, maxTriangles, coneWeight);
            break;
        }

        case 2:
        {
            meshletCount = meshopt_buildMeshlets(meshlets.data(), meshletVertices.data(), meshletTriangles.data(),
                (const unsigned short*) remappedIndices, indexCount,
                (const float*) pPosVertices, remappedVertexCount, posStride,
                maxVertices, maxTriangles, coneWeight);
            break;
        }

        case 1:
        {
            meshletCount = meshopt_buildMeshlets(meshlets.data(), meshletVertices.data(), meshletTriangles.data(),
                (const unsigned char*) remappedIndices, indexCount,
                (const float*) pPosVertices, remappedVertexCount, posStride,
                maxVertices, maxTriangles, coneWeight);
            break;
        }

        default:
            MY_ASSERT(false);
            break;
    }

    const meshopt_Meshlet& last = meshlets[meshletCount - 1];
    meshletVertices.resize(last.vertex_offset + last.vertex_count);
    meshletTriangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
    meshlets.resize(meshletCount);

    eastl::vector<unsigned short> meshletTriangles16;
    meshletTriangles16.reserve(meshletTriangles.size());
    for (size_t i = 0; i < meshletTriangles.size(); ++i)
    {
        meshletTriangles16.push_back(meshletTriangles[i]);
    }

    struct MeshletBound
    {
        float3 m_center;
        float m_radius;

        union 
        {
            // Axix + cutoff, rgb8snorm
            struct
            {
                int8_t m_axixX;
                int8_t m_axisY;
                int8_t m_axisZ;
                int8_t m_cutoff;
            };

            uint32_t m_cone;
        };

        uint m_vertexCount;
        uint m_triangleCount;
        uint m_vertexOffset;
        uint m_triangleOffset;
    };
    eastl::vector<MeshletBound> meshletBounds(meshletCount);
    
    for (size_t i = 0; i < meshletCount; ++i)
    {
        const meshopt_Meshlet& m = meshlets[i];
        meshopt_Bounds meshoptBound = meshopt_computeMeshletBounds(&meshletVertices[m.vertex_offset], &meshletTriangles[m.triangle_offset],
            m.triangle_count, (const float*) pPosVertices, remappedVertexCount, posStride);

        MeshletBound bound;
        bound.m_center = float3(meshoptBound.center);
        bound.m_radius = meshoptBound.radius;
        bound.m_axixX = meshoptBound.cone_axis_s8[0];
        bound.m_axisY = meshoptBound.cone_axis_s8[1];
        bound.m_axisZ = meshoptBound.cone_axis_s8[2];
        bound.m_cutoff = meshoptBound.cone_cutoff_s8;
        bound.m_vertexCount = m.vertex_count;
        bound.m_triangleCount = m.triangle_count;
        bound.m_vertexOffset = m.vertex_offset;
        bound.m_triangleOffset = m.triangle_offset;

        meshletBounds[i] = bound;
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    ResourceCache* pCache = ResourceCache::GetInstance();

    pMesh->m_pRenderer = pRenderer;
    if (indices.stride == 1)
    {
        uint16_t* pData = (uint16_t*) MY_ALLOC(sizeof(uint16_t) * indexCount);
        for (uint32_t i = 0; i < indexCount; ++i)
        {
            pData[i] = ((const char*) indices.data)[i];
        }

        indices.stride = 2;
        
        MY_FREE((void*)indices.data);
        indices.data = pData;
    }

    pMesh->m_indexBuffer = pCache->GetSceneBuffer("model(" + m_file + " " + name + ") IB", remappedIndices, (uint32_t) indices.stride * (uint32_t) indexCount);
    pMesh->m_indexBufferFormat = indices.stride == 4 ? RHIFormat::R32UI : RHIFormat::R16UI;
    pMesh->m_indexCount = (uint32_t) indexCount;
    pMesh->m_vertexCount = (uint32_t) remappedVertexCount;

    for (size_t i = 0; i < vertexTypes.size(); ++i)
    {
        switch (vertexTypes[i])
        {
            case cgltf_attribute_type_position:
                pMesh->m_posBuffer = pCache->GetSceneBuffer("model(" + m_file + " " + name + ") Pos", remappedVertices[i], (uint32_t) vertexStreams[i].stride * (uint32_t) remappedVertexCount);
                break;

            case cgltf_attribute_type_texcoord:
                pMesh->m_uvBuffer = pCache->GetSceneBuffer("model(" + m_file + " " + name + ") UV", remappedVertices[i], (uint32_t) vertexStreams[i].stride * (uint32_t) remappedVertexCount);
                break;

            case cgltf_attribute_type_normal:
                pMesh->m_normalBuffer = pCache->GetSceneBuffer("model(" + m_file + " " + name + ") Normal", remappedVertices[i], (uint32_t) vertexStreams[i].stride * (uint32_t) remappedVertexCount);
                break;

            case cgltf_attribute_type_tangent:
                pMesh->m_tangentBuffer = pCache->GetSceneBuffer("model(" + m_file + " " + name + ") Tangent", remappedVertices[i], (uint32_t) vertexStreams[i].stride * (uint32_t) remappedVertexCount);
                break;
            default:
                MY_ASSERT(false);
                break;
        }
    }

    pMesh->m_meshletCount = (uint32_t) meshletCount;
    pMesh->m_meshletBuffer = pCache->GetSceneBuffer("model(" + m_file + " " + name + ") Meshlet", meshletBounds.data(), sizeof(MeshletBound) * (uint32_t) meshletBounds.size());
    pMesh->m_meshletVerticesBuffer = pCache->GetSceneBuffer("model(" + m_file + " " + name + ") Meshlet Vertices", meshletVertices.data(), sizeof(unsigned int) * (uint32_t) meshletVertices.size());
    pMesh->m_meshletIndicesBuffer = pCache->GetSceneBuffer("model(" + m_file + " " + name + ") Meshlet Indices", meshletTriangles16.data(), sizeof(unsigned short) * (uint32_t) meshletTriangles16.size());

    pMesh->Create();
    m_pWorld->AddObject(pMesh);

    MY_FREE((void*) indices.data);
    for (size_t i = 0; i < vertexStreams.size(); ++i)
    {
        MY_FREE((void*) vertexStreams[i].data);
    }

    MY_FREE((void*) remappedIndices);
    for (size_t i = 0; i < remappedVertices.size(); ++i)
    {
        MY_FREE(remappedVertices[i]);
    }

    return pMesh;

}