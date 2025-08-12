#pragma once

#include "renderPass.h"

class DebugPass : public RenderPass
{
public:
	DebugPass(RenderContext& context);

	// Inherited via RenderPass
	void frame() override;
	void refresh() override;

private:
	ShaderProgram debugShaderProgram;

	const std::shared_ptr<RenderableModel> sphere;
};