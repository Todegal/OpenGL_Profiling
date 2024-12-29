#pragma once

#include <glad/glad.h>

#include <tiny_gltf.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <memory>
#include <filesystem>
#include <unordered_map>

struct TransformNode
{
	inline const glm::mat4 getWorldTransform() const
	{
		if (parent)
		{
			return parent->getWorldTransform() * getLocalTransform();
		}
		else
		{
			return getLocalTransform();
		}
	}

	inline const glm::mat4 getLocalTransform() const 
	{

		glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
		glm::mat4 r = glm::toMat4(rotation);
		glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);

		glm::mat4 transformation = t * (s * r);

		return transformation;
	}

	inline const glm::vec3 getWorldPosition() const
	{
		return glm::vec3(getWorldTransform()[3]);
	}

	glm::vec3 translation = glm::vec3(0.0f);
	glm::quat rotation = glm::quat();
	glm::vec3 scale = glm::vec3(1.0f);

	std::shared_ptr<TransformNode> parent = nullptr;

	std::string name = "";
};

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
	std::vector<GLuint> textures;

	std::shared_ptr<TransformNode> transformation;

	std::vector<std::shared_ptr<TransformNode>> nodes;
	std::vector<Joint> joints;

	std::string rootNode;

	std::unordered_map<std::string, Animation> animations;
	std::string currentAnimation;
	std::string nextAnimation;

	float blendDuration = 0.0f;
	float blendElapsed = 0.0f;

public:
	void selectAnimation(std::string animationName, float blendDuration = 0.0f, bool lockstep = false);

	void advanceAnimation(float deltaTime);

public:
	Model(std::filesystem::path gltfPath, std::string root = "");
	Model(const std::vector<GLuint>& buffers, const std::vector<MeshPrimitive>& primitives, const std::vector<GLuint>& textures)
		: buffers(buffers), primitives(primitives), textures(textures), 
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

	const std::vector<GLuint>& getTextures() const { return textures; }

	const std::vector<Joint>& getJoints() const { return joints; }

	const std::shared_ptr<TransformNode> getTransform() const { return transformation; }
	const std::vector<std::shared_ptr<TransformNode>> getNodes() const { return nodes; }
	std::shared_ptr<TransformNode> getNode(const std::string name);

	const glm::vec3 getVelocity();

	const std::string getRootNode() { return rootNode; }

	const bool isRigged() { return joints.size() > 0; }

private:
	// Decoupled transform data which doesn't make me cry trying to parse through a linked list
	struct TransformFrame
	{
		glm::vec3 translation = glm::vec3(0.0f);
		glm::quat rotation = glm::quat();
		glm::vec3 scale = glm::vec3(1.0f);
	};

	// returns the transform values of the joints array, at a given time in any animation
	std::unordered_map<int, TransformFrame> getFrame(std::string animation, float t);

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

