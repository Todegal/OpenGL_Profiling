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

// So my thoughts are: load the assimp scene,
// pass it with some index to various defined classes
// move/take ownership of the data we need
// (it seems like that can't be done)
// release the data we don't
// GENIUS

class RawMesh
{
      public:
        RawMesh(const aiMesh* mesh);
        ~RawMesh() = default;

        RawMesh(RawMesh&) = delete;
        RawMesh& operator=(RawMesh&) = delete;

        RawMesh(RawMesh&&) = delete;
        RawMesh& operator=(RawMesh&&) = delete;

        const std::vector<uint32_t>& getIndices() const
        {
                return indices;
        }

        const std::vector<glm::vec3>& getPositions() const
        {
                return positions;
        }

        const std::vector<glm::vec2>& getTexCoords() const
        {
                return texCoords;
        }

        const std::vector<glm::vec3>& getNormals() const
        {
                return normals;
        }

        const std::vector<glm::vec3>& getTangents() const
        {
                return tangents;
        }

        const std::vector<glm::vec3>& getBitangents() const
        {
                return bitangents;
        }

        uint32_t getMaterialIndex() const
        {
                return materialIndex;
        }

        const glm::vec3& getMin() const
        {
                return min;
        }

        const glm::vec3& getMax() const
        {
                return max;
        }

        const glm::vec3& getCentre() const
        {
                return centre;
        }

        const std::string& getName() const
        {
                return name;
        }

      private:
        std::vector<uint32_t> indices;

        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> texCoords;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec3> tangents;
        std::vector<glm::vec3> bitangents;

        uint32_t materialIndex;

        // Mesh Bounds
        glm::vec3 max;
        glm::vec3 min;
        glm::vec3 centre;

        std::string name;
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
        RawMaterial(const aiMaterial* material, aiTexture** textures, const std::filesystem::path& rootDir = "");
        ~RawMaterial() = default;

        // ALBEDO

        bool hasAlbedoTexture() const
        {
                return (albedoTexture != nullptr);
        }

        std::shared_ptr<const RawTexture> getAlbedoTexture() const
        {
                return albedoTexture;
        }

        const glm::vec4& getAlbedoFactor() const
        {
                return albedoFactor;
        }

        // METALLIC ROUGHNESS

        bool hasMetallicRoughnessTexture() const
        {
                return (metallicRoughnessTexture != nullptr);
        }

        std::shared_ptr<const RawTexture> getMetallicRoughnessTexture() const
        {
                return metallicRoughnessTexture;
        }

        const glm::vec4& getMetallicRoughnessFactor() const
        {
                return metallicRoughnessFactor;
        }

        // NORMAL

        bool hasNormalTexture() const
        {
                return (normalTexture != nullptr);
        }

        auto getNormalTexture() const
        {
                return normalTexture;
        }

        auto getNormalScale() const
        {
                return normalScale;
        }

        bool isTranslucent() const
        {
                if (!albedoTexture) { return albedoFactor.a < 1.0f; }
                return albedoTexture->getChannels() == 4;
        }

      private:
        std::shared_ptr<const RawTexture> albedoTexture;
        glm::vec4 albedoFactor;

        std::shared_ptr<const RawTexture> metallicRoughnessTexture;
        glm::vec4 metallicRoughnessFactor;

        std::shared_ptr<const RawTexture> normalTexture;
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
        RawScene();
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

      private:
        std::vector<std::shared_ptr<RawMesh>> meshes;
        std::vector<std::shared_ptr<RawMaterial>> materials;
};
