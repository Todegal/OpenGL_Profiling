#pragma once

#include <assimp/scene.h>

#include <assimp/texture.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <span>
#include <vector>

#include "transform.h"

// So my thoughts are: load the assimp scene,
// pass it with some index to various defined classes
// move/take ownership of the data we need
// (it seems like that can't be done)
// release the data we don't
// GENIUS

class RawTexture
{
      public:
        RawTexture(const aiTexture* texture);
        RawTexture(const std::filesystem::path& filepath, const std::filesystem::path& rootDir = "");
        ~RawTexture() = default;

        RawTexture(RawTexture&) = delete;
        RawTexture& operator=(RawTexture&) = delete;

        RawTexture(RawTexture&&) = delete;
        RawTexture& operator=(RawTexture&&) = delete;

        const std::span<const uint8_t> getData() const noexcept
        {
                return {dataPointer.get(), dataSize};
        }

        const glm::ivec2& getDimensions() const
        {
                return size;
        }

        int getChannels() const
        {
                return channels;
        }

      private:
        int channels;
        glm::ivec2 size;

        std::shared_ptr<uint8_t[]> dataPointer;
        std::size_t dataSize;
};

class RawMaterial
{
      public:
        RawMaterial(const aiMaterial* material, const std::size_t indexOffset = 0);
        ~RawMaterial() = default;

        RawMaterial(RawMaterial&) = delete;
        RawMaterial& operator=(RawMaterial&) = delete;

        RawMaterial(RawMaterial&&) = delete;
        RawMaterial& operator=(RawMaterial&&) = delete;

        // ALBEDO

        bool getHasAlbedoTexture() const
        {
                return hasAlbedoTexture;
        }

        auto getAlbedoTextureIdx() const
        {
                return albedoTextureIdx;
        }

        const glm::vec4& getAlbedoFactor() const
        {
                return albedoFactor;
        }

        // METALLIC ROUGHNESS

        bool getHasMetallicRoughnessTexture() const
        {
                return hasMetallicRoughnessTexture;
        }

        auto getMatallicRoughnessTextureIdx() const
        {
                return metallicRoughnessTextureIdx;
        }

        const glm::vec4& getMetallicRoughnessFactor() const
        {
                return metallicRoughnessFactor;
        }

        // NORMAL

        bool getHasNormalTexture() const
        {
                return hasNormalTexture;
        }

        auto getNormalTextureIdx() const
        {
                return normalTextureIdx;
        }

        auto getNormalScale() const
        {
                return normalScale;
        }

        bool isTranslucent() const
        {
                return albedoFactor.a < 1.0f;
        }

      private:
        bool hasAlbedoTexture{};
        std::size_t albedoTextureIdx;
        glm::vec4 albedoFactor{};

        bool hasMetallicRoughnessTexture{};
        std::size_t metallicRoughnessTextureIdx;
        glm::vec4 metallicRoughnessFactor{};

        bool hasNormalTexture{};
        std::size_t normalTextureIdx;
        float normalScale{};
};

class RawMesh
{
      public:
        RawMesh(const aiMesh* mesh, const std::size_t indexOffset = 0);
        ~RawMesh() = default;

        RawMesh(RawMesh&) = delete;
        RawMesh& operator=(RawMesh&) = delete;

        RawMesh(RawMesh&&) = delete;
        RawMesh& operator=(RawMesh&&) = delete;

        struct Vertex
        {
                Point3<LocalSpace> position{};
                glm::vec2 texCoord{};
                Normal3<LocalSpace> normal{};
                Normal3<LocalSpace> tangent{};
                Normal3<LocalSpace> bitangent{};
        };

        const std::string& getName() const
        {
                return name;
        }

        const std::vector<uint32_t>& getIndices() const
        {
                return indices;
        }

        const std::vector<Vertex> getVertices() const
        {
                return vertices;
        }

        auto getMaterialIdx() const
        {
                return materialIdx;
        }

        const Point3<LocalSpace>& getMin() const
        {
                return min;
        }

        const Point3<LocalSpace>& getMax() const
        {
                return max;
        }

        const Point3<LocalSpace>& getCentre() const
        {
                return centre;
        }

      private:
        std::string name;

        std::vector<uint32_t> indices;
        std::vector<Vertex> vertices;

        std::size_t materialIdx;

        // Mesh Bounds
        Point3<LocalSpace> min;
        Point3<LocalSpace> max;
        Point3<LocalSpace> centre;
};

class RawNode
{
      public:
        RawNode(const aiNode* node, std::optional<std::size_t> parentIdx, const std::vector<std::size_t>& childIndices,
                const std::size_t indexOffset = 0);
        ~RawNode() = default;

        RawNode(RawNode&) = delete;
        RawNode& operator=(RawNode&) = delete;

        RawNode(RawNode&&) = delete;
        RawNode& operator=(RawNode&&) = delete;

        const std::string& getName() const
        {
                return name;
        }

        const glm::vec3& getTranslation() const
        {
                return translation;
        }

        const glm::quat& getRotation() const
        {
                return rotation;
        }

        const glm::vec3& getScale() const
        {
                return scale;
        }

        const std::vector<std::size_t>& getChildIndices() const
        {
                return childIndices;
        }

        std::optional<std::size_t> getParentIdx() const
        {
                return parentIdx;
        }

        const std::vector<std::size_t>& getMeshIndices() const
        {
                return meshIndices;
        }

      private:
        std::string name;

        glm::vec3 translation;
        glm::quat rotation;
        glm::vec3 scale;

        std::vector<std::size_t> childIndices;
        std::optional<std::size_t> parentIdx;

        std::vector<std::size_t> meshIndices;

        static const glm::mat4 aiMatrixtoGLM(const aiMatrix4x4& aiMatrix);
};

// So this is the container class
// which contains all the sub data
// model data, texture data etc.
// loads new model files in the "addFile" function
// which encapsulates the lifetime of the assimp scene
class RawScene
{
      public:
        RawScene();
        ~RawScene() = default;

        void addFile(const std::filesystem::path& filepath);

        const std::vector<std::shared_ptr<RawTexture>>& getTextures() const
        {
                return textures;
        }

        const std::vector<std::shared_ptr<RawMaterial>>& getMaterials() const
        {
                return materials;
        }

        const std::vector<std::shared_ptr<RawMesh>>& getMeshes() const
        {
                return meshes;
        }

        const std::vector<std::shared_ptr<RawNode>>& getNodes() const
        {
                return nodes;
        }

      private:
        std::size_t processNode(aiNode* aiNode, std::optional<std::size_t> parentIdx,
                                const std::size_t indexOffset);

        std::vector<std::shared_ptr<RawTexture>> textures;
        std::vector<std::shared_ptr<RawMaterial>> materials;
        std::vector<std::shared_ptr<RawMesh>> meshes;
        std::vector<std::shared_ptr<RawNode>> nodes;
};
