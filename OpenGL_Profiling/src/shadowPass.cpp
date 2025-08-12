#include "shadowPass.h"

#include <spdlog/spdlog.h>

ShadowPass::ShadowPass(RenderContext& renderContext)
	: RenderPass(renderContext)
{
	renderContext.pointShadowNearPlane = 0.01f;
	renderContext.pointShadowFarPlane = 100.0f;
	renderContext.pointShadowMapDimensions = 2048;

	glGenTextures(1, &renderContext.textures["point_shadows"]);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, renderContext.textures.at("point_shadows"));

	glTexImage3D(
		GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT32F,
		renderContext.pointShadowMapDimensions, renderContext.pointShadowMapDimensions, 0, 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, 0);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);

	glGenFramebuffers(1, &shadowPassFBO);

	pointShadowMapShader.addShader(GL_VERTEX_SHADER, "shadow_pass/point_shadow.vert");
	pointShadowMapShader.addShader(GL_GEOMETRY_SHADER, "shadow_pass/point_shadow.geom");
	pointShadowMapShader.addShader(GL_FRAGMENT_SHADER, "shadow_pass/point_shadow.frag");

}

ShadowPass::~ShadowPass()
{

}

void ShadowPass::frame()
{
	const auto& pointLights = renderContext.scene->scenePointLights;

	if (pointLights.size() == 0) return;

	ScopedFramebufferBind framebufferBind(renderContext.framebufferStack, shadowPassFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, renderContext.textures.at("point_shadows"), 0);

	GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (e != GL_FRAMEBUFFER_COMPLETE)
	{
		spdlog::critical("Failed to bind shadow map FBO!");
		renderContext.flags[SHADOWS_ENABLED] = false;
		return;
	}

	glViewport(0, 0, renderContext.pointShadowMapDimensions, renderContext.pointShadowMapDimensions);
	glClear(GL_DEPTH_BUFFER_BIT);

	glCullFace(GL_FRONT);

	pointShadowMapShader.use();
	renderContext.buffers.bindBuffers(pointShadowMapShader);

	for (int i = 0; i < pointLights.size(); i++)
	{
		const auto& light = pointLights[i];
		const glm::vec3 lightPosition = glm::vec3(light.position);

		glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f),
			1.0f, renderContext.pointShadowNearPlane, renderContext.pointShadowFarPlane);

		std::vector<glm::mat4> shadowTransforms;
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));

		for (unsigned int j = 0; j < 6; ++j)
		{
			pointShadowMapShader.setMat4(std::format("uViewProjectMatrices[{}]", j), shadowTransforms[j]);
		}

		pointShadowMapShader.setInt("uIndex", i);

		pointShadowMapShader.setVec2("uPointShadowViewPlanes", { renderContext.pointShadowNearPlane, renderContext.pointShadowFarPlane });
		pointShadowMapShader.setVec3("uLightPos", lightPosition);

		for (size_t j = 0; j < renderContext.scene->sceneModels.size(); j++)
		{
			const auto& model = renderContext.scene->sceneModels[j];

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

			for (int k = 0; k < model->getPrimitives().size(); k++)
			{
				const auto& prim = model->getPrimitives()[k];

				pointShadowMapShader.setInt("uInstanceOffset", k);

				glBindVertexArray(prim->vertexArray);

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prim->indicesBuffer);

				glDrawElements(prim->mode, static_cast<GLsizei>(prim->count), prim->componentType, (void*)prim->byteOffset);

				glBindVertexArray(0);
			}
		}
	}

	glCullFace(GL_BACK);
}

void ShadowPass::refresh()
{
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, renderContext.textures.at("point_shadows"));

	glTexImage3D(
		GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT32F,
		renderContext.pointShadowMapDimensions, renderContext.pointShadowMapDimensions, 
		6 * static_cast<GLsizei>(renderContext.scene->scenePointLights.size()), 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, 0);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);

	glBindFramebuffer(GL_FRAMEBUFFER, shadowPassFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, renderContext.textures.at("point_shadows"), 0);
}
