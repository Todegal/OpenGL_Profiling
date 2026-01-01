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
#include <assimp/Exceptional.h>

#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <spdlog/spdlog.h>

#include "profiler.h"
#include "scene_graph.h"

#include <filesystem>

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

                size = {x, y};
                channels = comp;

                dataPointer = std::shared_ptr<uint8_t[]>(data, stbi_image_free);
                dataSize = size.x * size.y * channels;
        }
        else
        {
                size = {texture->mWidth, texture->mHeight};
                channels = 4;

                dataSize = size.x * size.y * sizeof(aiTexel);
                dataPointer = std::shared_ptr<uint8_t[]>(new uint8_t[dataSize]);

                std::memcpy(dataPointer.get(), texture->pcData, dataSize);
        }

        if (size.x == 0 || size.y == 0) { throw std::runtime_error("Invalid texture format!"); }

        spdlog::trace("Loaded embedded texture: {}", filename);
}

RawTexture::RawTexture(const std::filesystem::path& filepath, const std::filesystem::path& rootDir)

    : channels(0), size()
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

        size = {x, y};
        channels = comp;

        dataPointer = std::shared_ptr<uint8_t[]>(data, stbi_image_free);
        dataSize = size.x * size.y * channels;

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
                const auto index = std::atoi(albedoTexturePath.C_Str() + 1);
                albedoTextureIdx = index + indexOffset;
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
                const auto index = std::atoi(metallicRoughnessPath.C_Str() + 1);
                metallicRoughnessTextureIdx = index + indexOffset;
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
                const auto index = std::atoi(normalPath.C_Str() + 1);
                normalTextureIdx = index + indexOffset;
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

// I guess just copy all the data over...
RawMesh::RawMesh(const aiMesh* mesh, const std::size_t indexOffset)
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

        materialIdx = mesh->mMaterialIndex + indexOffset;
        name = mesh->mName.data;

        spdlog::trace("Loaded mesh: {}", mesh->mName.data);
}

const glm::mat4 RawNode::aiMatrixtoGLM(const aiMatrix4x4& aiMatrix)
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

RawNode::RawNode(const aiNode* node, std::optional<std::size_t> parentIdx, const std::vector<std::size_t>& childIndices,
                 const std::size_t indexOffset)
    : parentIdx(parentIdx), childIndices(std::move(childIndices))
{
        const glm::mat4 localTransformMatrix = aiMatrixtoGLM(node->mTransformation);

        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(localTransformMatrix, scale, rotation, translation, skew, perspective);

        name = node->mName.C_Str();

        meshIndices.reserve(node->mNumMeshes);
        for (std::size_t i = 0; i < node->mNumMeshes; ++i)
        {
                meshIndices.push_back(node->mMeshes[i] + indexOffset);
        }
}

std::size_t RawScene::processNode(aiNode* node, std::optional<std::size_t> parentIdx, const std::size_t indexOffset)
{
        const auto nodeIdx = nodes.size();
        nodes.push_back(nullptr);

        std::vector<std::size_t> childIndices;

        childIndices.reserve(node->mNumChildren);
        for (size_t i = 0; i < node->mNumChildren; i++)
        {
                childIndices.push_back(processNode(node->mChildren[i], nodeIdx, indexOffset));
        }

        nodes[nodeIdx] = std::make_shared<RawNode>(node, parentIdx, childIndices, indexOffset);

        return nodeIdx;
}

RawScene::RawScene()
{
        PROFILE_FUNCTION();

        stbi_set_flip_vertically_on_load(true);
}

void RawScene::addFile(const std::filesystem::path& filepath)
{
        PROFILE_FUNCTION();

        std::filesystem::path parentDir = filepath.parent_path();

        try
        {
                Assimp::Importer importer;
                const aiScene* scene = importer.ReadFile(
                    std::filesystem::absolute(filepath).string(),
                    aiProcess_CalcTangentSpace | aiProcess_EmbedTextures | aiProcess_GenSmoothNormals |
                        aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_LimitBoneWeights |
                        aiProcess_RemoveRedundantMaterials | aiProcess_SplitLargeMeshes | aiProcess_Triangulate |
                        aiProcess_GenUVCoords | aiProcess_SortByPType | aiProcess_FindDegenerates |
                        aiProcess_FindInvalidData);

                if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
                {
                        spdlog::error("Failed to load file: {}!", filepath.string());
                        spdlog::error("\tLog: {}", importer.GetErrorString());

                        return;
                }

                spdlog::trace("Loaded file: {}", filepath.string());

                textures.reserve(textures.size() + scene->mNumTextures);
                materials.reserve(materials.size() + scene->mNumMaterials);
                meshes.reserve(meshes.size() + scene->mNumMeshes);

                processNode(scene->mRootNode, std::nullopt, meshes.size());

                for (size_t i = 0; i < scene->mNumMeshes; i++)
                {
                        const auto m = std::make_shared<RawMesh>(scene->mMeshes[i], materials.size());
                        meshes.push_back(m);
                }

                for (size_t i = 0; i < scene->mNumMaterials; i++)
                {
                        const auto m = std::make_shared<RawMaterial>(scene->mMaterials[i], textures.size());
                        materials.push_back(m);
                }

                for (std::size_t i = 0; i < scene->mNumTextures; i++)
                {
                        const auto t = std::make_shared<RawTexture>(scene->mTextures[i]);
                        textures.push_back(t);
                }
        }
        catch (const DeadlyImportError& e)
        {
                spdlog::critical("{}", e.what());
                exit(EXIT_FAILURE);
        }
}
