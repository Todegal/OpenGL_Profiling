#pragma once

#include "renderPass.h"

class HDRRenderPass : public RenderPass
{
public:
	HDRRenderPass(RenderContext& renderContext);

public:
	// Inherited via RenderPass
	void frame() override;
	void refresh() override;

	GLuint getFramebuffer() const { return framebuffer; }

private:
	ShaderProgram hdrPassShader;

	GLuint framebuffer;
	GLuint depthRenderBuffer;
	GLuint colourTexture;

	const std::shared_ptr<RenderableModel> quad;
};