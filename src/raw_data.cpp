#include "raw_data.h"

#include <assimp/color4.h>
#include <cstring>
#include <limits>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>

#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <spdlog/spdlog.h>

#include "profiler.h"
#include "scene_graph.h"

#include <filesystem>

RawScene::RawScene(SceneGraph& sceneGraph) : sceneGraph(sceneGraph)
{
        stbi_set_flip_vertically_on_load(true);
}

glm::mat4 RawScene::aiMatrixtoGLM(const aiMatrix4x4& aiMatrix)
{
        glm::mat4 result;
        result[0][0] = aiMatrix.a1;
        result[1][0] = aiMatrix.a2;
        result[2][0] = aiMatrix.a3;
        result[3][0] = aiMatrix.a4;
        result[0][1] = aiMatrix.b1;
        result[1][1] = aiMatrix.b2;
        result[2][1] = aiMatrix.b3;
        result[3][1] = aiMatrix.b4;
        result[0][2] = aiMatrix.c1;
        result[1][2] = aiMatrix.c2;
        result[2][2] = aiMatrix.c3;
        result[3][2] = aiMatrix.c4;
        result[0][3] = aiMatrix.d1;
        result[1][3] = aiMatrix.d2;
        result[2][3] = aiMatrix.d3;
        result[3][3] = aiMatrix.d4;
        return result;
}

void RawScene::processNode(aiNode* aiNode, std::shared_ptr<SceneNode> parentNode)
{
        auto node = sceneGraph.addNode(aiNode->mName.C_Str(), parentNode);

        const glm::mat4 localTransformMatrix = aiMatrixtoGLM(aiNode->mTransformation);

        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(localTransformMatrix, scale, rotation, translation, skew, perspective);

        node->setLocalPosition(translation);
        node->setLocalRotation(rotation);
        node->setLocalScale(scale);

        for (size_t i = 0; i < aiNode->mNumMeshes; i++)
        {
                node->addMesh(aiNode->mMeshes[i] + meshes.size());
        }

        for (size_t i = 0; i < aiNode->mNumChildren; i++)
        {
                processNode(aiNode->mChildren[i], node);
        }
}

void RawScene::addFile(const std::filesystem::path& filePath)
{
        PROFILE_FUNCTION();

        std::filesystem::path parentDir = filePath.parent_path();

        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(
            std::filesystem::absolute(filePath).string(),
            aiProcess_CalcTangentSpace | aiProcess_EmbedTextures | aiProcess_GenSmoothNormals |
                aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_LimitBoneWeights |
                aiProcess_RemoveRedundantMaterials | aiProcess_SplitLargeMeshes | aiProcess_Triangulate |
                aiProcess_GenUVCoords | aiProcess_SortByPType | aiProcess_FindDegenerates | aiProcess_FindInvalidData);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
        {
                spdlog::error("Failed to load file: {}!", filePath.string());
                spdlog::error("\tLog: {}", importer.GetErrorString());

                return;
        }

        spdlog::trace("Loaded file: {}", filePath.string());

        const auto& root = sceneGraph.addNode(scene->mName.C_Str(), sceneGraph.getRoot());
        processNode(scene->mRootNode, root);

        meshes.reserve(meshes.size() + scene->mNumMeshes);
        for (size_t i = 0; i < scene->mNumMeshes; i++)
        {
                meshes.emplace_back(std::make_shared<RawMesh>(scene->mMeshes[i], materials.size()));
        }

        materials.reserve(materials.size() + scene->mNumMaterials);
        for (size_t i = 0; i < scene->mNumMaterials; i++)
        {
                materials.emplace_back(std::make_shared<RawMaterial>(scene->mMaterials[i], textures.size()));
        }

        textures.reserve(textures.size() + scene->mNumTextures);
        for (std::size_t i = 0; i < scene->mNumTextures; i++)
        {
                textures.emplace_back(std::make_shared<RawTexture>(scene->mTextures[i]));
        }
}

// I guess just copy all the data over...
RawMesh::RawMesh(const aiMesh* mesh, const std::size_t materialOffset)
{
        PROFILE_FUNCTION();

        // indices
        indices.resize(mesh->mNumFaces * 3);
        for (size_t i = 0; i < mesh->mNumFaces; i++)
        {
                const auto& face = mesh->mFaces[i];
                indices[(i * 3) + 0] = face.mIndices[0];
                indices[(i * 3) + 1] = face.mIndices[1];
                indices[(i * 3) + 2] = face.mIndices[2];
        }

        vertices.resize(mesh->mNumVertices);

        max = Point3<LocalSpace>();
        min = Point3<LocalSpace>(glm::vec3(std::numeric_limits<float>::max()));

        for (size_t i = 0; i < mesh->mNumVertices; i++)
        {
                const auto& position = mesh->mVertices[i];
                vertices[i].position = Point3<LocalSpace>(position.x, position.y, position.z);

                max = Point3<LocalSpace>(glm::max(max.getv(), vertices[i].position.getv()));
                min = Point3<LocalSpace>(glm::min(min.getv(), vertices[i].position.getv()));

                if (mesh->HasTextureCoords(0))
                {
                        const auto& texCoord = mesh->mTextureCoords[0][i];
                        vertices[i].texCoord = glm::vec2(texCoord.x, texCoord.y);
                }

                if (mesh->HasNormals())
                {
                        const auto& normal = mesh->mNormals[i];
                        vertices[i].normal = Normal3<LocalSpace>(normal.x, normal.y, normal.z);
                }

                if (mesh->HasTangentsAndBitangents())
                {
                        const auto& tangent = mesh->mTangents[i];
                        vertices[i].tangent = Normal3<LocalSpace>(tangent.x, tangent.y, tangent.z);

                        const auto& bitangent = mesh->mBitangents[i];
                        vertices[i].bitangent = Normal3<LocalSpace>(bitangent.x, bitangent.y, bitangent.z);
                }
        }

        // find the centre (midway between min and max)
        const auto d = max - min;
        centre = min + (d / 2);

        materialIndex = mesh->mMaterialIndex + materialOffset;

        name = mesh->mName.data;

        spdlog::trace("Loaded mesh: {}", mesh->mName.data);
}

RawTexture::RawTexture(const aiTexture* texture)
{
        PROFILE_FUNCTION();

        const char* filename = texture->mFilename.C_Str();
        if (texture->mHeight == 0)
        {
                int x, y, comp;
                uint8_t* data = stbi_load_from_memory(reinterpret_cast<stbi_uc*>(texture->pcData), texture->mWidth, &x,
                                                      &y, &comp, 0);

                if (!data)
                {
                        const char* failureReason = stbi_failure_reason();
                        throw std::runtime_error(
                            std::format("Failed to load texture from memory: {}, error: {}", filename, failureReason));
                }

                width = x;
                height = y;
                channels = comp;

                dataPointer = std::shared_ptr<uint8_t[]>(data, stbi_image_free);
                dataSize = width * height * channels;
        }
        else
        {
                width = texture->mWidth;
                height = texture->mHeight;
                channels = 4;

                size_t size = width * height * sizeof(aiTexel);
                dataPointer = std::shared_ptr<uint8_t[]>(new uint8_t[size]);

                std::memcpy(dataPointer.get(), texture->pcData, size);

                dataSize = width * height * sizeof(aiTexel);
        }

        if (width == 0 || height == 0) { throw std::runtime_error("Invalid texture format!"); }

        spdlog::trace("Loaded embedded texture: {}", filename);
}

RawTexture::RawTexture(const std::filesystem::path& filepath, const std::filesystem::path& rootDir)
    : channels(0), width(0), height(0)
{
        PROFILE_FUNCTION();

        const std::filesystem::path absoluteFilePath =
            (rootDir != "") ? std::filesystem::absolute(rootDir / filepath) : std::filesystem::absolute(filepath);

        if (!std::filesystem::exists(absoluteFilePath))
        {
                throw std::runtime_error(std::format("Invalid filepath: {}", absoluteFilePath.string()));
        }

        int x, y, comp;
        uint8_t* data = stbi_load(absoluteFilePath.string().c_str(), &x, &y, &comp, 0);

        if (!data) { throw std::runtime_error(std::format("Failed to load filepath: {}", absoluteFilePath.string())); }

        width = x;
        height = y;
        channels = comp;

        dataPointer = std::shared_ptr<uint8_t[]>(data, stbi_image_free);
        dataSize = width * height * channels;

        spdlog::trace("Loaded texture: {}", filepath.string());
}

RawMaterial::RawMaterial(const aiMaterial* material, const std::size_t indexOffset)
{
        PROFILE_FUNCTION();

        aiString albedoTexturePath;
        material->GetTexture(aiTextureType_BASE_COLOR, 0, &albedoTexturePath);

        // todo: come up with what I'm going to do with non-embeded textures ... die?

        if (albedoTexturePath.C_Str()[0] == '*')
        {
                albedoTextureIdx = std::atoi(albedoTexturePath.C_Str() + 1) + indexOffset;
                hasAlbedoTexture = true;
        }
        /*else if (!albedoTexturePath.Empty())
        {
                albedoTexture = std::make_unique<RawTexture>(albedoTexturePath.C_Str(), rootDir);
        }*/
        else { hasAlbedoTexture = false; }

        aiColor3D baseColour(0.f, 0.f, 0.f);
        material->Get(AI_MATKEY_BASE_COLOR, baseColour);

        albedoFactor = glm::vec4(baseColour.r, baseColour.g, baseColour.b, 1.0f);

        aiString metallicRoughnessPath;
        material->GetTexture(aiTextureType_BASE_COLOR, 0, &metallicRoughnessPath);

        if (metallicRoughnessPath.C_Str()[0] == '*')
        {
                metallicRoughnessTextureIdx = std::atoi(metallicRoughnessPath.C_Str() + 1) + indexOffset;
                hasMetallicRoughnessTexture = true;
        }
        // else if (!metallicRoughnessPath.Empty())
        //{
        //         metallicRoughnessTexture = std::make_shared<const RawTexture>(metallicRoughnessPath.C_Str(),
        //         rootDir);
        // }
        else { hasMetallicRoughnessTexture = false; }

        ai_real metallicFactor, roughnessFactor;
        material->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor);
        material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor);

        metallicRoughnessFactor = glm::vec4(0.0f, roughnessFactor, metallicFactor, 0.0f);

        aiString normalPath;
        material->GetTexture(aiTextureType_NORMALS, 0, &normalPath);

        if (normalPath.C_Str()[0] == '*')
        {
                normalTextureIdx = std::atoi(normalPath.C_Str() + 1) + indexOffset;
                hasNormalTexture = true;
        }
        // else if (!normalPath.Empty())
        //{
        //         normalTexture = std::make_shared<const RawTexture>(normalPath.C_Str(), rootDir);
        // }
        else { hasNormalTexture = false; }

        // TODO: find a way to get this from ASSIMP if required...
        normalScale = 1.0f;
}
