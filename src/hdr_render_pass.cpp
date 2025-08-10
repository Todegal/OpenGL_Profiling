#include "hdrRenderPass.h"

HDRRenderPass::HDRRenderPass(RenderContext& renderContext)
	: RenderPass(renderContext), quad(RenderableModel::constructUnitQuad())
{
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);	

	glGenRenderbuffers(1, &depthRenderBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRenderBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
		renderContext.dimensions.x, renderContext.dimensions.y);

	glGenTextures(1, &colourTexture);

	glBindTexture(GL_TEXTURE_2D, colourTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderContext.dimensions.x, renderContext.dimensions.y, 0, GL_RGBA, GL_FLOAT, 0);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderBuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colourTexture, 0);

	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw std::runtime_error("Incomplete HDR Framebuffer!");
	}

	hdrPassShader.addShader(GL_VERTEX_SHADER, "shaders/hdr_pass/hdr_pass.vert.glsl");
	hdrPassShader.addShader(GL_FRAGMENT_SHADER, "shaders/hdr_pass/hdr_pass.frag.glsl");
}

void HDRRenderPass::frame()
{
	hdrPassShader.use();

	glActiveTexture(GL_TEXTURE0);

	glBindTexture(GL_TEXTURE_2D, colourTexture);

	hdrPassShader.setInt("uColourTexture", 0);

	hdrPassShader.setVec2("uScreenDimensions", renderContext.dimensions);

	const auto& quadPrim = quad->getPrimitives()[0];

	renderPrimitive(quadPrim);
}

void HDRRenderPass::refresh()
{
	ScopedFramebufferBind framebufferBind(renderContext.framebufferStack, framebuffer);

	glBindRenderbuffer(GL_RENDERBUFFER, depthRenderBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
		renderContext.dimensions.x, renderContext.dimensions.y);

	glBindTexture(GL_TEXTURE_2D, colourTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderContext.dimensions.x, renderContext.dimensions.y, 0, GL_RGBA, GL_FLOAT, 0);
}
