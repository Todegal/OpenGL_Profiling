#include "pbrRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <numeric>

PBRRenderer::PBRRenderer(glm::ivec2 screenSize, const Camera& mainCamera)
	: dimensions(screenSize), camera(mainCamera)
{
	generateProjectionMatrix();

	unlitShader.addShader(GL_VERTEX_SHADER, "shaders/modelView.vert.glsl");
	unlitShader.addShader(GL_FRAGMENT_SHADER, "shaders/unlit.frag.glsl");

	depthCubemapShader.addShader(GL_VERTEX_SHADER, "shaders/model.vert.glsl");
	depthCubemapShader.addShader(GL_GEOMETRY_SHADER, "shaders/depthShader.geom.glsl");
	depthCubemapShader.addShader(GL_FRAGMENT_SHADER, "shaders/depthShader.frag.glsl");

	depthPrepassShader.addShader(GL_VERTEX_SHADER, "shaders/modelView.vert.glsl");

	glGenBuffers(1, &lightBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, lightBuffer);

	glGenBuffers(1, &jointsBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, jointsBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, jointsBuffer);

	glGenFramebuffers(1, &shadowMapFBO);

	sphere = Model::constructUnitSphere(16, 16);
	quad = Model::constructUnitQuad();
	cube = Model::constructUnitCube();

	initializeDeferredPass();
	initializeForwardPass();
	initializeHdrPass();
}

PBRRenderer::~PBRRenderer()
{
	clearScene();
}

void PBRRenderer::frame()
{
	std::vector<ShaderLight> lights(scene->sceneLights.size());
	for (size_t i = 0; i < scene->sceneLights.size(); i++)
	{
		lights[i] = {
			scene->sceneLights[i].position,
			std::sqrtf(scene->sceneLights[i].strength / lightCutoff),
			scene->sceneLights[i].colour,
			scene->sceneLights[i].strength
		};
	}

	std::vector<glm::mat4> jointMatrices;
	for (const auto& model : scene->sceneModels)
	{
		jointOffsets.push_back(jointMatrices.size());

		for (const auto& t : model->getJoints())
		{
			auto w = model->getNodes()[t.nodeIdx]->getWorldTransform();
			w[3] -= glm::vec4(model->getTransform()->getWorldPosition(), 0.0f);

			auto iB = t.inverseBindMatrix;

			jointMatrices.push_back(w * iB);
		}
		
		// while we're here let's sort all of the translucent meshes
		std::sort(model->getTranslucentPrimitives().begin(), model->getTranslucentPrimitives().end(),
			[&](const MeshPrimitive& a, const MeshPrimitive& b) -> bool {
				const float aDist = glm::length2(camera.getEye() - a.transform->getWorldPosition());
				const float bDist = glm::length2(camera.getEye() - b.transform->getWorldPosition());

				return aDist < bDist;
			}
		);

	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ShaderLight) * lights.size(), lights.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, jointsBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4) * jointMatrices.size(), jointMatrices.data(), GL_STATIC_DRAW);

	if (flags[SHADOWS_ENABLED])
		renderShadowMaps(lights);
	
	glBindFramebuffer(GL_FRAMEBUFFER, flags[HDR_PASS_ENABLED] ? hdrPassFBO : 0);

	glViewport(0, 0, dimensions.x, dimensions.y);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Depth prepass

	if (flags[DEPTH_PREPASS_ENABLED])
	{
		depthPrepassShader.use();

		depthPrepassShader.setMat4("uProjection", projectionMatrix);
		depthPrepassShader.setMat4("uView", camera.getViewMatrix());
		depthPrepassShader.setVec3("uCameraPosition", camera.getEye());

		for (size_t i = 0; i < scene->sceneModels.size(); i++)
		{
			depthPrepassShader.setInt("uJointsOffset", jointOffsets[i]);

			for (const auto& prim : scene->sceneModels[i]->getOpaquePrimitives())
			{
				renderPrimitive(prim, depthPrepassShader);
			}
		}
	}

	// Deferred pass

	if (flags[DEFERRED_PASS_ENABLED])
	{
		// Render Opaque objects

		deferredPass(lights);
	}

	// Standard forward pass 

	forwardPass(lights);

	// Debug lights -- render sphere on each point light

	const auto& sphereMesh = sphere->getPrimitives()[0];
	
	unlitShader.use();

	unlitShader.setMat4("uProjection", projectionMatrix);
	unlitShader.setMat4("uView", camera.getViewMatrix());

	glBindVertexArray(sphereMesh.vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereMesh.indicesBuffer);

	// Render spheres for each light
	for (const auto& light : scene->sceneLights)
	{
		glm::mat4 transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(light.position)), glm::vec3(0.05f));

		unlitShader.setMat4("uModel", transform);
		glm::mat3 NM = transform;
		NM = glm::inverse(NM);
		NM = glm::transpose(NM);

		unlitShader.setMat3("uNormalMatrix", NM);

		unlitShader.setVec4("uBaseColour.factor", glm::vec4(light.colour, 1.0f));

		glDrawElements(sphereMesh.mode, static_cast<GLsizei>(sphereMesh.count), sphereMesh.componentType, (void*)0);
	}
		
	glBindVertexArray(0);

	// Run Post-Processing - currently just a HDR pass
	if (flags[HDR_PASS_ENABLED])
		hdrPass();
}

void PBRRenderer::loadScene(std::shared_ptr<Scene> s)
{
	scene = s;

	glGenTextures(1, &shadowCubemapArray);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, shadowCubemapArray);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexImage3D(
		GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT,
		shadowMapDimensions.x, shadowMapDimensions.x, 6 * scene->sceneLights.size(), 0,
		GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);

	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
}

void PBRRenderer::clearScene()
{
	scene->sceneModels.clear();
	scene->sceneLights.clear();

	glDeleteFramebuffers(1, &shadowMapFBO);
	glDeleteTextures(1, &shadowCubemapArray);
	glDeleteBuffers(1, &lightBuffer);
}

void PBRRenderer::setFlag(const int flag, bool value)
{
	assert(flag >= 0);
	assert(flag < NUM_FLAGS);

	flags[flag] = value;
}

void PBRRenderer::resize(glm::ivec2 screenSize)
{
	dimensions = screenSize;

	generateProjectionMatrix();

	resizeDeferredPass();

	resizeForwardPass();

	resizeHdrPass();
}

void PBRRenderer::generateProjectionMatrix()
{
	float aspectRatio = static_cast<float>(dimensions.x) / static_cast<float>(dimensions.y);
	projectionMatrix = glm::perspective(glm::radians(camera.getFov()), aspectRatio, 0.1f, 1000.0f);
}

void PBRRenderer::renderPrimitive(const MeshPrimitive& prim, ShaderProgram program)
{
	program.setMat4("uModel", prim.transform->getWorldTransform());
	glm::mat3 NM = prim.transform->getWorldTransform();
	NM = glm::inverse(NM);
	NM = glm::transpose(NM);

	program.setMat3("uNormalMatrix", NM);

	glBindVertexArray(prim.vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prim.indicesBuffer);

	glDrawElements(prim.mode, static_cast<GLsizei>(prim.count), prim.componentType, (void*)prim.byteOffset);

	glBindVertexArray(0);
}

void PBRRenderer::loadMaterialProperties(const std::vector<GLuint>& textures, const tinygltf::Material& materialDesc, ShaderProgram shader)
{
	const tinygltf::PbrMetallicRoughness& pbr = materialDesc.pbrMetallicRoughness;

	shader.setVec4("uBaseColour.factor", { pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3] });
	if (pbr.baseColorTexture.index >= 0)
	{
		shader.setBool("uBaseColour.isTextureEnabled", true);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textures[pbr.baseColorTexture.index]);
		shader.setInt("uBaseColour.textureMap", 0);
	}
	else
	{
		shader.setBool("uBaseColour.isTextureEnabled", false);
	}

	shader.setVec4("uMetallicRoughness.factor", { 0, pbr.roughnessFactor, pbr.metallicFactor, 0 });
	if (pbr.metallicRoughnessTexture.index >= 0)
	{
		shader.setBool("uMetallicRoughness.isTextureEnabled", true);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, textures[pbr.metallicRoughnessTexture.index]);
		shader.setInt("uMetallicRoughness.textureMap", 1);
	}
	else
	{
		shader.setBool("uMetallicRoughness.isTextureEnabled", false);
	}

	if (materialDesc.normalTexture.index >= 0 && flags[NORMALS_ENABLED])
	{
		shader.setBool("uNormal.isTextureEnabled", true);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, textures[materialDesc.normalTexture.index]);
		shader.setInt("uNormal.textureMap", 2);

		shader.setVec4("uNormal.factor", { materialDesc.normalTexture.scale, 0, 0, 0 });
	}
	else
	{
		shader.setBool("uNormal.isTextureEnabled", false);
	}

	if (materialDesc.occlusionTexture.index >= 0 && flags[OCCLUSION_ENABLED])
	{
		shader.setBool("uOcclusion.isTextureEnabled", true);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, textures[materialDesc.occlusionTexture.index]);
		shader.setInt("uOcclusion.textureMap", 3);

		shader.setVec4("uOcclusion.factor", { materialDesc.occlusionTexture.strength, 0, 0, 0 });
	}
	else
	{
		shader.setBool("uOcclusion.isTextureEnabled", false);
	}
}

void PBRRenderer::renderShadowMaps(const std::vector<ShaderLight>& lights)
{
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowCubemapArray, 0);

	assert(glIsTexture(shadowCubemapArray) == GL_TRUE);

	GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (e != GL_FRAMEBUFFER_COMPLETE)
	{
		spdlog::critical("Failed to bind shadow map FBO!");
		flags[SHADOWS_ENABLED] = false;
		return;
	}

	glViewport(0, 0, shadowMapDimensions.x, shadowMapDimensions.y);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_DEPTH_BUFFER_BIT);

	glCullFace(GL_FRONT);

	depthCubemapShader.use();

	for (int i = 0; i < lights.size(); i++)
	{
		const ShaderLight& light = lights[i];
		const glm::vec3 lightPosition = glm::vec3(light.position);

		glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f),
			static_cast<float>(shadowMapDimensions.x) / static_cast<float>(shadowMapDimensions.y),
			shadowNearPlane, shadowFarPlane);

		std::vector<glm::mat4> shadowTransforms;
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
		shadowTransforms.push_back(shadowProj * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)));
	
		for (unsigned int j = 0; j < 6; ++j)
		{
			depthCubemapShader.setMat4(std::format("uShadowMatrices[{}]", j), shadowTransforms[j]);
		}

		depthCubemapShader.setFloat("uShadowMapDepth", (shadowFarPlane - shadowNearPlane));
		depthCubemapShader.setInt("uIndex", i);
		depthCubemapShader.setVec3("uLightPos", lightPosition);

		for (size_t k = 0; k < scene->sceneModels.size(); k++)
		{
			//pbrShader.setInt("uJointsOffset", jointOffsets[k]);

			const auto& model = scene->sceneModels[k];

			for (const auto& prim : model->getPrimitives())
			{
				renderPrimitive(prim, depthCubemapShader);
			}
		}
	}

	glCullFace(GL_BACK);
}

void PBRRenderer::initializeDeferredPass()
{
	glGenFramebuffers(1, &gBufferFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);

	glGenRenderbuffers(1, &gBufferDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, gBufferDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, dimensions.x, dimensions.y);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gBufferDepth);

	gBufferShader.addShader(GL_VERTEX_SHADER, "shaders/modelView.vert.glsl");
	gBufferShader.addShader(GL_FRAGMENT_SHADER, "shaders/gBuffer.frag.glsl");

	gBufferTextures.resize(3);
	glGenTextures(gBufferTextures.size(), gBufferTextures.data());

	std::vector<GLenum> attachments;

	for (size_t i = 0; i < gBufferTextures.size(); i++)
	{
		glBindTexture(GL_TEXTURE_2D, gBufferTextures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, dimensions.x, dimensions.y, 0, GL_RGBA, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, gBufferTextures[i], 0);

		attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
	}

	glDrawBuffers(gBufferTextures.size(), attachments.data());

	assert(glIsTexture(gBufferTextures[0]) == GL_TRUE);

	GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (e != GL_FRAMEBUFFER_COMPLETE)
	{
		spdlog::critical("Failed to bind G-Buffer FBO! Falling back to forward rendering!");
		flags[DEFERRED_PASS_ENABLED] = false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	deferredPassShader.addShader(GL_VERTEX_SHADER, "shaders/screenQuad.vert.glsl");
	deferredPassShader.addShader(GL_FRAGMENT_SHADER, "shaders/deferredShader.frag.glsl");
}

void PBRRenderer::resizeDeferredPass()
{
	for (size_t i = 0; i < gBufferTextures.size(); i++)
	{
		glBindTexture(GL_TEXTURE_2D, gBufferTextures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, dimensions.x, dimensions.y, 0, GL_RGBA, GL_FLOAT, nullptr);
	}

	glBindRenderbuffer(GL_RENDERBUFFER, gBufferDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, dimensions.x, dimensions.y);
}

void PBRRenderer::buildGBuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, gBufferFBO);

	GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (e != GL_FRAMEBUFFER_COMPLETE)
	{
		spdlog::critical("Failed to bind G-Buffer FBO! Falling back to forward rendering!");
		flags[DEFERRED_PASS_ENABLED] = false;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		return;
	}

	glViewport(0, 0, dimensions.x, dimensions.y);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// If we already have a depth pass blit that and save some overdraw
	// Sounds good but really doesn't work :(
	//if (flags[DEPTH_PREPASS_ENABLED])
	//{
	//	glBlitNamedFramebuffer(0, gBufferFBO,
	//		0, 0, dimensions.x, dimensions.y,
	//		0, 0, dimensions.x, dimensions.y,
	//		GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	//}

	gBufferShader.use();

	gBufferShader.setMat4("uProjection", projectionMatrix);
	gBufferShader.setMat4("uView", camera.getViewMatrix());
	gBufferShader.setVec3("uCameraPosition", camera.getEye());

	for (size_t i = 0; i < scene->sceneModels.size(); i++)
	{
		gBufferShader.setInt("uJointsOffset", jointOffsets[i]);

		// ONLY OPAQUE
		for (const auto& prim : scene->sceneModels[i]->getOpaquePrimitives())
		{
			loadMaterialProperties(scene->sceneModels[i]->getTextures(), prim.materialDesc, gBufferShader);
			renderPrimitive(prim, gBufferShader);
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, flags[HDR_PASS_ENABLED] ? hdrPassFBO : 0);
}

void PBRRenderer::deferredPass(const std::vector<ShaderLight>& lights)
{
	buildGBuffer();

	//glEnable(GL_BLEND);
	//glBlendEquation(GL_FUNC_ADD);
	//glBlendFunc(GL_ONE, GL_ONE);

	//glDisable(GL_DEPTH_TEST);

	deferredPassShader.use();

	deferredPassShader.setVec3("uCameraPosition", camera.getEye());

	deferredPassShader.setBool("uShadowsEnabled", flags[SHADOWS_ENABLED]);
	deferredPassShader.setInt("uNumLights", scene->sceneLights.size());

	if (flags[SHADOWS_ENABLED])
	{
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, shadowCubemapArray);

		deferredPassShader.setInt("uShadowMaps", 5);
		deferredPassShader.setFloat("uShadowMapDepth", shadowFarPlane - shadowNearPlane);
	}

	for (size_t i = 0; i < gBufferTextures.size(); i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, gBufferTextures[i]);

		deferredPassShader.setInt(std::format("g{}", i), i);
	}

	deferredPassShader.setVec2("uScreenDimensions", dimensions);

	// New plan, instead of rendering a screen wide quad we're going to render a cube for each light 
	// whose radius is the radius of the light, and only applying the lighting inside that cube

	//const auto& cubePrim = cube->getPrimitives()[0];

	//deferredPassShader.setMat4("uProjection", projectionMatrix);
	//deferredPassShader.setMat4("uView", camera.getViewMatrix());

	//glBindVertexArray(cubePrim.vertexArray);

	//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubePrim.indicesBuffer);

	//glCullFace(GL_FRONT);

	//std::vector<glm::mat4> modelMatrices(lights.size());

	//for (size_t i = 0; i < lights.size(); i++)
	//{
	//	const auto& light = lights[i];

	//	glm::mat4 transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(light.position)), glm::vec3(light.radius));

	//	modelMatrices[i] = transform;
	//}

	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, deferredPassModelBuffer);
	//glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4) * modelMatrices.size(), modelMatrices.data(), GL_STATIC_DRAW);

	//glDrawElementsInstanced(cubePrim.mode, static_cast<GLsizei>(cubePrim.count), cubePrim.componentType, (void*)0, lights.size());
	//
	//glBindVertexArray(0);

	const auto& prim = quad->getPrimitives()[0];

	glBindVertexArray(prim.vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prim.indicesBuffer);

	glDrawElements(prim.mode, static_cast<GLsizei>(prim.count), prim.componentType, (void*)0);

	glBindVertexArray(0);
	
	//glCullFace(GL_BACK);

	//glEnable(GL_DEPTH_TEST);
	//glDepthMask(GL_TRUE);
	//glDepthFunc(GL_LEQUAL);

	//glDisable(GL_BLEND);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, gBufferFBO);
	glBlitFramebuffer(0, 0, dimensions.x, dimensions.y, 0, 0, dimensions.x, dimensions.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

}

void PBRRenderer::initializeForwardPass()
{
	forwardPassShader.addShader(GL_VERTEX_SHADER, "shaders/modelView.vert.glsl");
	forwardPassShader.addShader(GL_FRAGMENT_SHADER, "shaders/pbr.frag.glsl");
}

void PBRRenderer::resizeForwardPass()
{
}

void PBRRenderer::forwardPass(const std::vector<ShaderLight>& lights)
{
	forwardPassShader.use();

	forwardPassShader.setMat4("uProjection", projectionMatrix);
	forwardPassShader.setMat4("uView", camera.getViewMatrix());
	forwardPassShader.setVec3("uCameraPosition", camera.getEye());

	forwardPassShader.setBool("uShadowsEnabled", flags[SHADOWS_ENABLED]);
	forwardPassShader.setInt("uNumLights", scene->sceneLights.size());

	if (flags[SHADOWS_ENABLED])
	{
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, shadowCubemapArray);

		forwardPassShader.setInt("uShadowMaps", 5);
		forwardPassShader.setFloat("uShadowMapDepth", shadowFarPlane - shadowNearPlane);
	}

	for (size_t i = 0; i < scene->sceneModels.size(); i++)
	{
		forwardPassShader.setInt("uJointsOffset", jointOffsets[i]);
		
		// Render opaque primitives - only if there is no deferred pass running
		if (!flags[DEFERRED_PASS_ENABLED])
		{
			for (const auto& prim : scene->sceneModels[i]->getOpaquePrimitives())
			{
				loadMaterialProperties(scene->sceneModels[i]->getTextures(), prim.materialDesc, forwardPassShader);

				renderPrimitive(prim, forwardPassShader);
			}
		}

		// Enable blending and render the translucent primitives

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		for (const auto& prim : scene->sceneModels[i]->getTranslucentPrimitives())
		{
			loadMaterialProperties(scene->sceneModels[i]->getTextures(), prim.materialDesc, forwardPassShader);

			renderPrimitive(prim, forwardPassShader);
		}

		glDisable(GL_BLEND);
	}
}

void PBRRenderer::initializeHdrPass()
{
	glGenFramebuffers(1, &hdrPassFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, hdrPassFBO);

	glGenRenderbuffers(1, &hdrPassDepthRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, hdrPassDepthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, dimensions.x, dimensions.y);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, hdrPassDepthRBO);

	glGenTextures(1, &hdrPassColour);
	glBindTexture(GL_TEXTURE_2D, hdrPassColour);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, dimensions.x, dimensions.y, 0, GL_RGBA, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrPassColour, 0);

	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (e != GL_FRAMEBUFFER_COMPLETE)
	{
		spdlog::critical("Failed to bind HDR FBO!");
		
		flags[HDR_PASS_ENABLED] = false;

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	
	hdrPassShader.addShader(GL_VERTEX_SHADER, "shaders/screenQuad.vert.glsl");
	hdrPassShader.addShader(GL_FRAGMENT_SHADER, "shaders/hdr.frag.glsl");
}

void PBRRenderer::resizeHdrPass()
{
	glBindRenderbuffer(GL_RENDERBUFFER, hdrPassDepthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, dimensions.x, dimensions.y);

	glBindTexture(GL_TEXTURE_2D, hdrPassColour);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, dimensions.x, dimensions.y, 0, GL_RGBA, GL_FLOAT, 0);
}

void PBRRenderer::hdrPass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, dimensions.x, dimensions.y);

	glDisable(GL_DEPTH_TEST);

	hdrPassShader.use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrPassColour);
	hdrPassShader.setInt("uColour", 0);

	hdrPassShader.setVec2("uScreenDimensions", dimensions);

	const auto& prim = quad->getPrimitives()[0];

	glBindVertexArray(prim.vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prim.indicesBuffer);

	glDrawElements(prim.mode, static_cast<GLsizei>(prim.count), prim.componentType, (void*)0);

	glBindVertexArray(0);

	glEnable(GL_DEPTH_TEST);
}

