#pragma once

#include <glad/glad.h>

#include <tiny_gltf.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <memory>
#include <filesystem>
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

class Model
{
private: // Containers
	std::vector<GLuint> buffers; 
	std::vector<MeshPrimitive> primitives;

	std::vector<MeshPrimitive> opaquePrimitives;
	std::vector<MeshPrimitive> translucentPrimitives;

	std::vector<GLuint> textures;

	std::shared_ptr<TransformNode> transformation;

	std::vector<std::shared_ptr<TransformNode>> nodes;
	std::vector<Joint> joints;

	std::string rootNode;

	std::unordered_map<std::string, Animation> animations;

// Animation Handlers
private:
	std::string currentAnimation;
	std::string nextAnimation;

	float blendDuration = 0.0f;
	float blendElapsed = 0.0f;
	float staticBlend = -1.0f;
	bool fit = false;

public: // Animation Accessors
	// Snap straight into new animation
	void selectAnimation(std::string animationName);

	// Start new animation with a timed blend
	// If lockstep is true it will start at the time proportionate to where the current animation is
	void selectAnimationBlendInto(std::string animationName, float blendDuration, bool lockstep);

	// Blend two animations by some factor
	// If fit is true it will fit Animation B to the length of Animation A, otherwise they will both continue independently
	// If lockstep is true it will start both at the time proportionate to where the current animation is 
	void selectAnimationStaticBlend(std::string animationA, std::string animationB, float staticFactor, bool fit, bool lockstep);

	void advanceAnimation(float deltaTime);

	const std::string getAnimation() const { return currentAnimation; }
	const std::string getNextAnimation() const { return nextAnimation; }

	const std::unordered_map<std::string, Animation> getAnimations() const { return animations; }
	
	void setJoints(const std::unordered_map<int, TransformOffset>& offsets);

public:
	Model(std::filesystem::path gltfPath, std::string root = "");
	Model(const std::vector<GLuint>& buffers, const std::vector<MeshPrimitive>& primitives, const std::vector<GLuint>& textures)
		: buffers(buffers), primitives(primitives), opaquePrimitives(primitives), translucentPrimitives(), textures(textures), 
		transformation(std::make_shared<TransformNode>()), currentAnimation(""), nextAnimation(""), rootNode("")
	{
		for (auto& prim : primitives)
		{
			prim.transform->parent = transformation;
		}
	}

	~Model();

	Model(const Model&) = delete;
	Model& operator=(const Model&) = delete;

	Model(Model&&) = delete;
	Model& operator=(Model&&) = delete;

	static std::shared_ptr<Model> constructUnitSphere(unsigned int columns = 64, unsigned int rows = 64);
	static std::shared_ptr<Model> constructUnitCube();
	static std::shared_ptr<Model> constructUnitQuad();

	std::vector<MeshPrimitive>& getPrimitives() { return primitives; }
	std::vector<MeshPrimitive>& getOpaquePrimitives() { return opaquePrimitives; }
	std::vector<MeshPrimitive>& getTranslucentPrimitives() { return translucentPrimitives; }


	const std::vector<GLuint>& getTextures() const { return textures; }

	const std::vector<Joint>& getJoints() const { return joints; }

	const std::shared_ptr<TransformNode> getTransform() const { return transformation; }
	const std::vector<std::shared_ptr<TransformNode>> getNodes() const { return nodes; }
	std::shared_ptr<TransformNode> getNode(const std::string name);

	const glm::vec3 getVelocity();

	const std::string getRootNode() { return rootNode; }

	const bool isRigged() { return joints.size() > 0; }

private:
	// returns the transform values of the joints array, at a given time in any animation
	std::unordered_map<int, TransformOffset> getFrame(std::string animation, float t);

private:
	void loadBuffers(const tinygltf::Model& model); // Model is passed through member functions so that it can go out of scope and be destroyed inside the constructor
	
	void loadTextures(const tinygltf::Model& model);

	void loadNodes(const tinygltf::Model& model);
	void loadAnimations(const tinygltf::Model& model);
	void loadSkins(const tinygltf::Model& model);

	void loadPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::shared_ptr<TransformNode> transformNode);

	std::vector<float> accessorToFloats(const tinygltf::Model& model, const tinygltf::Accessor& accessor);

	bool isLoaded = false;
};

