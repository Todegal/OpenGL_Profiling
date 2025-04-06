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
