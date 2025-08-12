#include "raw_data.h"

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
#include <spdlog/spdlog.h>

#include "profiler.h"

#include <filesystem>

void RawScene::addFile(const std::filesystem::path& filePath)
{
        PROFILE_FUNCTION();

        std::filesystem::path parentDir = std::filesystem::absolute(filePath).parent_path();

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

        meshes.resize(scene->mNumMeshes);
        for (size_t i = 0; i < scene->mNumMeshes; i++)
        {
                meshes[i] = std::make_shared<RawMesh>(scene->mMeshes[i]);
        }

        materials.resize(scene->mNumMaterials);
        for (size_t i = 0; i < scene->mNumMaterials; i++)
        {
                materials[i] = std::make_shared<RawMaterial>(scene->mMaterials[i], scene->mTextures, parentDir);
        }
}

// I guess just copy all the data over...
RawMesh::RawMesh(const aiMesh* mesh)
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

        positions.resize(mesh->mNumVertices);
        texCoords.resize(mesh->mNumVertices);
        normals.resize(mesh->mNumVertices);
        tangents.resize(mesh->mNumVertices);
        bitangents.resize(mesh->mNumVertices);

        max = glm::vec3(0);
        min = glm::vec3(std::numeric_limits<float>::max());

        for (size_t i = 0; i < mesh->mNumVertices; i++)
        {
                const auto& position = mesh->mVertices[i];
                positions[i] = glm::vec3(position.x, position.y, position.z);

                max = glm::max(max, positions[i]);
                min = glm::min(min, positions[i]);

                if (mesh->HasTextureCoords(0))
                {
                        const auto& texCoord = mesh->mTextureCoords[0][i];
                        texCoords[i] = glm::vec2(texCoord.x, texCoord.y);
                }

                if (mesh->HasNormals())
                {
                        const auto& normal = mesh->mNormals[i];
                        normals[i] = glm::vec3(normal.x, normal.y, normal.z);
                }

                if (mesh->HasTangentsAndBitangents())
                {
                        const auto& tangent = mesh->mTangents[i];
                        tangents[i] = glm::vec3(tangent.x, tangent.y, tangent.z);

                        const auto& bitangent = mesh->mBitangents[i];
                        bitangents[i] = glm::vec3(bitangent.x, bitangent.y, bitangent.z);
                }
        }

        materialIndex = mesh->mMaterialIndex;

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
                stbi_set_flip_vertically_on_load(true);
                uint8_t* data = stbi_load_from_memory(reinterpret_cast<stbi_uc*>(texture->pcData), texture->mWidth, &x,
                                                      &y, &comp, 0);

                if (!data)
                {
                        throw std::runtime_error(std::format("Failed to load texture from memory: {}", filename));
                }

                width = x;
                height = y;
                channels = comp;

                dataPointer = std::shared_ptr<uint8_t[]>(data, stbi_image_free);
                dataSpan = std::span<uint8_t>(dataPointer.get(), width * height * channels);
        }
        else
        {
                width = texture->mWidth;
                height = texture->mHeight;
                channels = 4;

                size_t size = width * height * channels;
                dataPointer = std::shared_ptr<uint8_t[]>(new uint8_t[size]);

                std::memcpy(dataPointer.get(), texture->pcData, size);

                dataSpan = std::span<const uint8_t>(dataPointer.get(), width * height * channels);
        }

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
        dataSpan = std::span<uint8_t>(dataPointer.get(), width * height * channels);

        spdlog::trace("Loaded texture: {}", filepath.string());
}

RawMaterial::RawMaterial(const aiMaterial* material, aiTexture** textures, const std::filesystem::path& rootDir)
{
        PROFILE_FUNCTION();

        aiString albedoTexturePath;
        material->GetTexture(aiTextureType_BASE_COLOR, 0, &albedoTexturePath);

        if (albedoTexturePath.C_Str()[0] == '*')
        {
                int index = std::atoi(albedoTexturePath.C_Str() + 1);
                const aiTexture* texture = textures[index];

                albedoTexture = std::make_unique<RawTexture>(texture);
        }
        else if (!albedoTexturePath.Empty())
        {
                albedoTexture = std::make_unique<RawTexture>(albedoTexturePath.C_Str(), rootDir);
        }
        else { throw std::runtime_error("Cannot create material without albedo texture!"); }
}
