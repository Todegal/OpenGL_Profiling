#pragma once

#include "renderPass.h"

class EnvironmentPass : public RenderPass
{
public:
	EnvironmentPass(RenderContext& renderContext);
	~EnvironmentPass();

	// Inherited via RenderPass
	void frame() override;
	void refresh() override;

private:
	void loadEquirectangularMap();
	void prefilterEnvironment();

	GLuint environmentCubeMap;

	ShaderProgram skyboxShader;

	const unsigned int environmentMapDimensions;
	const std::shared_ptr<RenderableModel> cube;
	const std::shared_ptr<RenderableModel> quad;
};