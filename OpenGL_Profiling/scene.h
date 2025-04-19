#pragma once

#include "model.h"

#include <memory>
#include <vector>

struct Light;

struct Scene
{
	std::vector<Light> sceneLights;
	std::vector<std::shared_ptr<RenderableModel>> sceneModels;
	std::string enviromentMap = "";
};