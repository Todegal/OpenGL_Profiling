#include "forwardRenderPass.h"

ForwardRenderPass::ForwardRenderPass(RenderContext& frameDesc)
	: RenderPass(frameDesc)
{
	forwardPassShader.addShader(GL_VERTEX_SHADER, "shaders/forward_pass/forward_pass.vert.glsl");
	forwardPassShader.addShader(GL_FRAGMENT_SHADER, "shaders/forward_pass/forward_pass.frag.glsl");
}

void ForwardRenderPass::frame()
{
	forwardPassShader.use();
	renderContext.buffers.bindBuffers(forwardPassShader);

	if (renderContext.flags[SHADOWS_ENABLED])
	{
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, renderContext.textures.at("pointShadowMaps"));

		forwardPassShader.setInt("uPointShadowMaps", 5);

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D_ARRAY, renderContext.textures.at("directionalShadowMaps"));

		forwardPassShader.setInt("uDirectionalShadowMaps", 6);
	}

	if (renderContext.flags[ENVIRONMENT_MAP_ENABLED])
	{
		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_CUBE_MAP, renderContext.textures.at("irradianceMap"));

		forwardPassShader.setInt("uIrradianceMap", 7);

		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_CUBE_MAP, renderContext.textures.at("prefilteredMap"));

		forwardPassShader.setInt("uPrefilteredMap", 8);

		glActiveTexture(GL_TEXTURE9);
		glBindTexture(GL_TEXTURE_2D, renderContext.textures.at("brdf"));

		forwardPassShader.setInt("uBRDF", 9);
	}

	// Render opaque primitives - only if there is no deferred pass running
	if (!renderContext.flags[DEFERRED_PASS_ENABLED])
	{
		for (size_t i = 0; i < renderContext.scene->sceneModels.size(); i++)
		{
			loadJoints(renderContext.scene->sceneModels[i]);

			for (const auto& prim : renderContext.scene->sceneModels[i]->getOpaquePrimitives())
			{
				parseMaterialProperties(renderContext.scene->sceneModels[i]->getTextures(), prim->materialDesc);

				renderPrimitive(prim);
			}
		}
	}

	// Enable blending and render the translucent primitives

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	for (size_t i = 0; i < renderContext.scene->sceneModels.size(); i++)
	{
		loadJoints(renderContext.scene->sceneModels[i]);

		for (const auto& prim : renderContext.scene->sceneModels[i]->getTranslucentPrimitives())
		{
			parseMaterialProperties(renderContext.scene->sceneModels[i]->getTextures(), prim->materialDesc);

			renderPrimitive(prim);
		}
	}

	glDisable(GL_BLEND);
}

void ForwardRenderPass::refresh()
{
}
