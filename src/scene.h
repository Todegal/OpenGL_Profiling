#pragma once

#include "model.h"

#include <memory>
#include <vector>

#include <glm/vec3.hpp>

struct Light
{
	glm::vec3 position; // Position will act as direction if the light is directional
	glm::vec3 colour;
	float strength;

	enum LIGHT_TYPE {
		DIRECTIONAL = 0,
		POINT
	} type;
};

struct Scene
{
	std::vector<Light> sceneLights;
	std::vector<std::shared_ptr<RenderableModel>> sceneModels;
	std::string environmentMap = "";

	Scene() : sceneLights(), sceneModels(), environmentMap("") { }
};