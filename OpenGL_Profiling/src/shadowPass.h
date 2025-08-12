#pragma once

#include "renderPass.h"

class ShadowPass : public RenderPass
{
public:
	ShadowPass(RenderContext& renderContext);
	~ShadowPass();

	// Inherited via RenderPass
	void frame() override;

	void refresh() override;

private:
	ShaderProgram pointShadowMapShader;

	GLuint shadowPassFBO;
};