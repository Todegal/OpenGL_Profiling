#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <memory>
#include <string>

struct TransformOffset
{
	glm::vec3 translation = glm::vec3(0.0f);
	glm::quat rotation = glm::quat();
	glm::vec3 scale = glm::vec3(1.0f);
};

struct TransformNode : public std::enable_shared_from_this<TransformNode> {

public:
	void setTranslation(const glm::vec3& t) {
		translation = t;
		markDirty();
	}

	void setRotation(const glm::quat& r) {
		rotation = r;
		markDirty();
	}

	void setScale(const glm::vec3& s) {
		scale = s;
		markDirty();
	}

	glm::vec3 getWorldPosition() const {
		return glm::vec3(getWorldTransform()[3]);
	}

	const glm::mat4& getWorldTransform() const {
		if (worldDirty) {
			if (parent) {
				worldTransform = parent->getWorldTransform() * getLocalTransform();
			}
			else {
				worldTransform = getLocalTransform();
			}
			worldDirty = false;
		}
		return worldTransform;
	}

	const glm::mat4& getLocalTransform() const {
		if (localDirty) {
			glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
			glm::mat4 r = glm::toMat4(rotation);
			glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
			localTransform = t * (s * r); // note: order can vary by convention
			localDirty = false;
		}
		return localTransform;
	}

	void addChild(const std::shared_ptr<TransformNode>& child) {
		children.push_back(child);
		child->parent = shared_from_this(); // Requires enable_shared_from_this
		child->markDirty();
	}

	//std::shared_ptr<TransformNode> getParent() const {
	//	return parent;
	//}

	//void setParent(const std::shared_ptr<TransformNode>& p) {
	//	parent = p;
	//	markDirty();
	//}

	std::string name = "";

private:
	glm::vec3 translation = glm::vec3(0.0f);
	glm::quat rotation = glm::quat();
	glm::vec3 scale = glm::vec3(1.0f);

private:
	// Internal caching
	mutable glm::mat4 localTransform = glm::mat4(1.0f);
	mutable glm::mat4 worldTransform = glm::mat4(1.0f);
	mutable bool localDirty = true;
	mutable bool worldDirty = true;

	std::shared_ptr<TransformNode> parent = nullptr;
	std::vector<std::shared_ptr<TransformNode>> children;

	void markDirty() {
		localDirty = true;
		worldDirty = true;

		for (auto& child : children) {
			child->markDirty(); // propagate recursively
		}
	}
};
