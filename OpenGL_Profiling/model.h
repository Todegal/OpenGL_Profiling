#pragma once

#include <glad/glad.h>

#include <tiny_gltf.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <memory>
#include <filesystem>
#include <optional>
#include <unordered_map>

#include "transform.h"

struct MeshPrimitive
{
	GLuint indicesBuffer;

	GLuint vertexArray;

	// Accessor Details
	int mode;
	size_t count;
	int componentType;
	size_t byteOffset;

	// Bounding box
	glm::vec3 min, max;

	tinygltf::Material materialDesc; // TODO: Materials System

	std::shared_ptr<TransformNode> transform = nullptr;
};

struct AnimationChannel
{
	std::vector<float> keyframeTimes;
	std::vector<float> values;

	int target; // Node Idx
	std::string path; // "rotation" || "translation" || "scale" -> todo: morph targets
};

struct Animation
{
	std::vector<AnimationChannel> channels;
	float duration;
	float elapsed = 0.0f;

	bool loop = true;

	// Verrry specific use case, not generalized at all! todo: fix!
	glm::vec3 rootOffset = glm::vec3(0.0f);
	glm::vec3 velocity = glm::vec3(0.0f);
};

struct Joint
{
	int nodeIdx;
	glm::mat4 inverseBindMatrix = glm::mat4(1.0f);
};

struct LoadedModel
{
	tinygltf::Model model;
	std::string rootNode;
};

class RawModel
{
public:
	RawModel(std::filesystem::path gltfPath, std::string rootNode = "");

	LoadedModel extract() 
	{
		if (!model.has_value())
		{
			throw std::logic_error("RawModel has already been extracted!");
		}

		return { std::move(model.value()), std::move(rootNode) };
	}

private:
	std::optional<tinygltf::Model> model;
	std::string rootNode;
};

class RenderableModel
{
public:
	RenderableModel(const LoadedModel& modelData, bool isStatic = true);

	RenderableModel(const std::vector<GLuint>& buffers, const std::vector<std::shared_ptr<MeshPrimitive>>& primitives, const std::vector<GLuint>& textures)
		: buffers(buffers), primitives(primitives), opaquePrimitives(primitives), translucentPrimitives(), textures(textures),
		transformation(std::make_shared<TransformNode>()), isStatic(true)
	{
		for (auto& prim : primitives)
		{
			transformation->addChild(prim->transform);
		}
	}

	static std::shared_ptr<RenderableModel> constructUnitSphere(unsigned int columns = 64, unsigned int rows = 64);
	static std::shared_ptr<RenderableModel> constructUnitCube();
	static std::shared_ptr<RenderableModel> constructUnitQuad();

private: // Containers filled on init(), released on destructor()
	std::vector<GLuint> buffers;
	std::vector<std::shared_ptr<MeshPrimitive>> primitives;

	std::vector<std::shared_ptr<MeshPrimitive>> opaquePrimitives;
	std::vector<std::shared_ptr<MeshPrimitive>> translucentPrimitives;

	std::vector<GLuint> textures;

	std::shared_ptr<TransformNode> transformation;

	std::vector<std::shared_ptr<TransformNode>> nodes;
	std::vector<Joint> joints;

	std::string rootNode;

	std::unordered_map<std::string, Animation> animations;

	bool isStatic;

public:
	~RenderableModel();

	RenderableModel(const RenderableModel&) = delete;
	RenderableModel& operator=(const RenderableModel&) = delete;
	
	RenderableModel(RenderableModel&&) = delete;
	RenderableModel& operator=(RenderableModel&&) = delete;

	const std::vector<std::shared_ptr<MeshPrimitive>>& getPrimitives() const { return primitives; }
	const std::vector<std::shared_ptr<MeshPrimitive>>& getOpaquePrimitives() const { return opaquePrimitives; }
	const std::vector<std::shared_ptr<MeshPrimitive>>& getTranslucentPrimitives() const { return translucentPrimitives; }

	const std::vector<GLuint>& getTextures() const { return textures; }

	const std::vector<Joint>& getJoints() const { return joints; }

	const std::shared_ptr<TransformNode> getTransform() const { return transformation; }
	const std::vector<std::shared_ptr<TransformNode>> getNodes() const { return nodes; }

	const std::unordered_map<std::string, Animation>& getAnimations() const { return animations; }

	const bool getIsStatic() const { return isStatic; }

	void setJoints(const std::unordered_map<int, TransformOffset>& offsets);

private:
	void loadBuffers(const tinygltf::Model& model); // Model is passed through member functions so that it can go out of scope and be destroyed inside the constructor

	void loadTextures(const tinygltf::Model& model);

	void loadNodes(const tinygltf::Model& model);

	void loadSkins(const tinygltf::Model& model);

	void loadAnimations(const tinygltf::Model& model);

	void loadPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::shared_ptr<TransformNode> transformNode);

	void calculateBounds();

	void calculateSDF();
	
	std::vector<float> accessorToFloats(const tinygltf::Model& model, const tinygltf::Accessor& accessor);
};