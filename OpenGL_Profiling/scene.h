#pragma once

#include <memory>
#include <vector>

class Model;
struct Light;

struct Scene
{
	std::vector<Light> sceneLights;
	std::vector<std::shared_ptr<Model>> sceneModels;
	std::string enviromentMap = "";
};