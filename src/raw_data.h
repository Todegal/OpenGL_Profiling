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

#include "scene_graph.h"
#include "transform.h"

// So my thoughts are: load the assimp scene,
// pass it with some index to various defined classes
// move/take ownership of the data we need
// (it seems like that can't be done)
// release the data we don't
// GENIUS

class RawMesh
{
      public:
        RawMesh(const aiMesh* mesh, const std::size_t materialOffset);
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

      private:
        std::vector<uint32_t> indices;

        std::vector<Vertex> vertices;

        std::size_t materialIndex;

        // Mesh Bounds
        Point3<LocalSpace> max;
        Point3<LocalSpace> min;
        Point3<LocalSpace> centre;

        std::string name;

      public:
        const std::vector<uint32_t>& getIndices() const
        {
                return indices;
        }

        const std::vector<Vertex> getVertices() const
        {
                return vertices;
        }

        std::size_t getMaterialIndex() const
        {
                return materialIndex;
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

        const std::string& getName() const
        {
                return name;
        }
};

class RawTexture
{
      public:
        RawTexture(const aiTexture* texture);
        RawTexture(const std::filesystem::path& filepath, const std::filesystem::path& rootDir = "");
        ~RawTexture() = default;

        const std::span<const uint8_t> getData() const noexcept
        {
                return {dataPointer.get(), dataSize};
        }

        uint32_t getWidth() const
        {
                return width;
        }
        uint32_t getHeight() const
        {
                return height;
        }

        glm::ivec2 getDimensions() const
        {
                return {width, height};
        }

        int getChannels() const
        {
                return channels;
        }

      private:
        int channels;
        uint32_t width;
        uint32_t height;

        std::shared_ptr<uint8_t[]> dataPointer;
        size_t dataSize;
};

class RawMaterial
{
      public:
        RawMaterial(const aiMaterial* material, const std::size_t indexOffset);
        ~RawMaterial() = default;

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

        auto getMetallicRoughnessTextureIdx() const
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
        bool hasAlbedoTexture;
        std::size_t albedoTextureIdx;
        glm::vec4 albedoFactor;

        bool hasMetallicRoughnessTexture;
        std::size_t metallicRoughnessTextureIdx;
        glm::vec4 metallicRoughnessFactor;

        bool hasNormalTexture;
        std::size_t normalTextureIdx;
        float normalScale;
};

// So this is the container class
// which contains all the sub data
// model data, texture data etc.
// loads new model files in the "addFile" function
// which encapsulates the lifetime of the assimp scene
class RawScene
{
      public:
        RawScene(SceneGraph& sceneGraph);
        ~RawScene() = default;

        void addFile(const std::filesystem::path& filePath);

        const std::vector<std::shared_ptr<RawMesh>>& getMeshes() const
        {
                return meshes;
        }

        const std::vector<std::shared_ptr<RawMaterial>>& getMaterials() const
        {
                return materials;
        }

        const std::vector<std::shared_ptr<RawTexture>>& getTextures() const
        {
                return textures;
        }

      private:
        glm::mat4 aiMatrixtoGLM(const aiMatrix4x4& aiMatrix);
        void processNode(aiNode* aiNode, std::shared_ptr<SceneNode> parentNode = nullptr);

        SceneGraph& sceneGraph;

        std::vector<std::shared_ptr<RawMesh>> meshes;
        std::vector<std::shared_ptr<RawMaterial>> materials;
        std::vector<std::shared_ptr<RawTexture>> textures;
};
