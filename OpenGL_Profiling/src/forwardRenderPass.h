#pragma once

#include "renderPass.h"

class ForwardRenderPass : public RenderPass
{
public:
	ForwardRenderPass(RenderContext& renderContext);

public:
	void frame() override;
	void refresh() override;

private:
	ShaderProgram forwardPassShader;
};