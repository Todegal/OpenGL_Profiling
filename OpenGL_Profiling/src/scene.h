#pragma once

#include "model.h"

#include <memory>
#include <vector>

#include <glm/vec3.hpp>

struct PointLight
{
	glm::vec3 position; // Position will act as direction if the light is directional
	glm::vec3 colour;
	float strength;
};

struct DirectionalLight
{
	glm::vec3 direction; // Position will act as direction if the light is directional
	glm::vec3 colour;
	float strength;
};

struct Scene
{
	std::vector<PointLight> scenePointLights;
	std::vector<DirectionalLight> sceneDirectionalLights;
	std::vector<std::shared_ptr<RenderableModel>> sceneModels;
	std::string environmentMap = "";

	Scene() : scenePointLights(), sceneDirectionalLights(), sceneModels(), environmentMap("") { }
};