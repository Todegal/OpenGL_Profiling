#include "forwardRenderPass.h"

ForwardRenderPass::ForwardRenderPass(RenderContext& frameDesc)
	: RenderPass(frameDesc)
{
	forwardPassShader.addShader(GL_VERTEX_SHADER, "forward_pass/forward_pass.vert");
	forwardPassShader.addShader(GL_FRAGMENT_SHADER, "forward_pass/forward_pass.frag");
}

void ForwardRenderPass::frame()
{
	forwardPassShader.use();
	renderContext.buffers.bindBuffers(forwardPassShader);

	if (renderContext.flags[SHADOWS_ENABLED])
	{
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, renderContext.textures.at("point_shadows"));

		forwardPassShader.setInt("uPointShadowMaps", 5);

		forwardPassShader.setVec2("uPointShadowViewPlanes", { renderContext.pointShadowNearPlane, renderContext.pointShadowFarPlane });

		//glActiveTexture(GL_TEXTURE6);
		//glBindTexture(GL_TEXTURE_2D_ARRAY, renderContext.textures.at("directional_"));

		//forwardPassShader.setInt("uDirectionalShadowMaps", 6);
	}

	if (renderContext.flags[ENVIRONMENT_ENABLED])
	{
		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_CUBE_MAP, renderContext.textures.at("irradiance"));

		forwardPassShader.setInt("uIrradianceMap", 7);

		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_CUBE_MAP, renderContext.textures.at("prefilter"));

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
			const auto& model = renderContext.scene->sceneModels[i];

			loadJoints(model);

			struct alignas(16) InstanceTransform
			{
				glm::mat4 model;
				glm::mat3x4 normalMatrix;
			};

			std::vector<InstanceTransform> instances;

			for (const auto& prim : model->getPrimitives())
			{
				InstanceTransform instance;

				instance.model = prim->transform->getWorldTransform();
				glm::mat3 NM = prim->transform->getWorldTransform();
				NM = glm::inverse(NM);
				NM = glm::transpose(NM);
				instance.normalMatrix = NM;

				instances.push_back(instance);
			}

			renderContext.buffers.bufferData("instance", sizeof(InstanceTransform) * instances.size(), instances.data());

			for (int j = 0; j < model->getPrimitives().size(); j++)
			{
				const auto& prim = model->getPrimitives()[j];

				forwardPassShader.setInt("uInstanceOffset", j);

				parseMaterialProperties(model->getTextures(), prim->materialDesc, forwardPassShader);

				glBindVertexArray(prim->vertexArray);

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prim->indicesBuffer);

				glDrawElements(prim->mode, static_cast<GLsizei>(prim->count), prim->componentType, (void*)prim->byteOffset);

				glBindVertexArray(0);
			}
		}
	}
}

void ForwardRenderPass::refresh()
{
}
