#include "pbrRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <spdlog/spdlog.h>

#include <stb_image.h>

#include <algorithm>
#include <numeric>
#include <execution>

PBRRenderer::PBRRenderer(glm::ivec2 screenSize, const Camera& mainCamera)
	: dimensions(screenSize), camera(mainCamera), sphere(Model::constructUnitSphere(16, 16)), quad(Model::constructUnitQuad()),
	cube(Model::constructUnitCube())
{
	generateProjectionMatrix();

	unlitShader.addShader(GL_VERTEX_SHADER, "shaders/modelView.vert.glsl");
	unlitShader.addShader(GL_FRAGMENT_SHADER, "shaders/unlit.frag.glsl");

	depthPrepassShader.addShader(GL_VERTEX_SHADER, "shaders/modelView.vert.glsl");

	glGenBuffers(1, &lightBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, lightBuffer);

	glGenBuffers(1, &jointsBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, jointsBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, jointsBuffer);

	initializeShadowMaps();
	initializeDeferredPass();
	initializeForwardPass();
	initializeHdrPass();

	flags[NORMALS_ENABLED] = true;
	flags[SHADOWS_ENABLED] = false;
	flags[ENVIRONMENT_MAP_ENABLED] = false;
	flags[HDR_PASS_ENABLED] = true;
}

PBRRenderer::~PBRRenderer()
{
	clearScene();

	cleanShadowMaps();
	cleanDeferredPass();
	cleanForwardPass();
	cleanHdrPass();
}

void PBRRenderer::loadScene(std::shared_ptr<Scene> s)
{
	scene = s;

	resizeShadowMaps();

	if (scene->enviromentMap != "")
	{
		loadEnvironmentMap();
	}
}

void PBRRenderer::clearScene()
{
	scene->sceneModels.clear();
	scene->sceneLights.clear();

	glDeleteBuffers(1, &lightBuffer);
	glDeleteBuffers(1, &jointsBuffer);
}

void PBRRenderer::frame()
{
	// Parse lights, and calculate radii
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

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ShaderLight) * lights.size(), lights.data(), GL_STATIC_DRAW);

	// Calculate all the joint matrices for all of the models
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
		std::sort(std::execution::par, model->getTranslucentPrimitives().begin(), model->getTranslucentPrimitives().end(),
			[&](const MeshPrimitive& a, const MeshPrimitive& b) -> bool {
				const float aDist = glm::length2(camera.getEye() - a.transform->getWorldPosition());
				const float bDist = glm::length2(camera.getEye() - b.transform->getWorldPosition());

				return aDist < bDist;
			}
		);

	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, jointsBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4) * jointMatrices.size(), jointMatrices.data(), GL_STATIC_DRAW);

	if (flags[SHADOWS_ENABLED])
		buildShadowMaps(lights);
	
	glBindFramebuffer(GL_FRAMEBUFFER, flags[HDR_PASS_ENABLED] ? hdrPassFBO : 0);

	glViewport(0, 0, dimensions.x, dimensions.y);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glClear(GL_DEPTH_BUFFER_BIT);

	// Depth prepass

	if (flags[DEPTH_PREPASS_ENABLED] && !flags[DEFERRED_PASS_ENABLED])
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

	// DEBUG : Render spheres for each light
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

	// Draw Background
	if (flags[ENVIRONMENT_MAP_ENABLED])
	{
		glDisable(GL_CULL_FACE);

		backgroundShader.use();

		backgroundShader.setMat4("uProjection", projectionMatrix);
		backgroundShader.setMat4("uView", camera.getViewMatrix());

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);

		backgroundShader.setBool("uEnvironmentMap", 0);

		const auto& prim = cube->getPrimitives()[0];

		glBindVertexArray(prim.vertexArray);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prim.indicesBuffer);

		glDrawElements(prim.mode, static_cast<GLsizei>(prim.count), prim.componentType, (void*)0);

		glBindVertexArray(0);

		glEnable(GL_CULL_FACE);
	}

	// Run Post-Processing - currently just a HDR pass
	if (flags[HDR_PASS_ENABLED])
		hdrPass();
}

void PBRRenderer::setFlag(const int flag, bool value)
{
	assert(flag >= 0);
	assert(flag < NUM_FLAGS);

	flags[flag] = value;
}

void PBRRenderer::drawFlagsDialog(imgui_data data)
{
	int windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

	if (!ImGui::Begin("PBR Renderer Flags", &data.showRenderFlags, windowFlags))
	{
		ImGui::End();
		return;
	}

	ImGui::Checkbox("Occlusion:", &flags[OCCLUSION_ENABLED]);
	ImGui::Checkbox("Shadows:", &flags[SHADOWS_ENABLED]);
	ImGui::Checkbox("Environment:", &flags[ENVIRONMENT_MAP_ENABLED]);
	ImGui::Separator();
	ImGui::Checkbox("Deferred Pass:", &flags[DEFERRED_PASS_ENABLED]);
	ImGui::Checkbox("HDR Pass:", &flags[HDR_PASS_ENABLED]);

	ImGui::End();
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

void PBRRenderer::renderPrimitive(const MeshPrimitive& prim, ShaderProgram& program)
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

void PBRRenderer::loadMaterialProperties(const std::vector<GLuint>& textures, const tinygltf::Material& materialDesc, ShaderProgram& shader)
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

void PBRRenderer::loadEnvironmentMap()
{
	// Load the map into a 2d texture
	stbi_set_flip_vertically_on_load(true);

	int width, height, channels;
	float* data = stbi_loadf(scene->enviromentMap.c_str(), &width, &height, &channels, 3);
	if (!data || width + height < 2 || channels < 3)
	{
		spdlog::error("Failed to load environment map: {}", scene->enviromentMap);
		flags[ENVIRONMENT_MAP_ENABLED] = false;
		return;
	}

	GLuint equirectangularMap;
	glGenTextures(1, &equirectangularMap);
	glBindTexture(GL_TEXTURE_2D, equirectangularMap);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, data);

	stbi_image_free(data);

	glGenTextures(1, &environmentMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	//glTexImage3D(GL_TEXTURE_CUBE_MAP, 0, GL_RGBA32F, 
	//	environmentMapDimensions, environmentMapDimensions, 6, 
	//	0, GL_RGBA, GL_FLOAT, 0);

	for (size_t i = 0; i < 6; i++)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA32F,
			environmentMapDimensions, environmentMapDimensions,
			0, GL_RGBA, GL_FLOAT, nullptr);
	}

	glViewport(0, 0, environmentMapDimensions, environmentMapDimensions);

	GLuint captureCubemapFBO;
	glGenFramebuffers(1, &captureCubemapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, captureCubemapFBO);

	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, environmentMap, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	assert(glIsTexture(environmentMap) == GL_TRUE);

	GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (e != GL_FRAMEBUFFER_COMPLETE)
	{
		spdlog::critical("Failed to bind FBO!");
		flags[ENVIRONMENT_MAP_ENABLED] = false;
		return;
	}

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	// test but let's see what happens
	ShaderProgram buildCubemapShader;
	buildCubemapShader.addShader(GL_VERTEX_SHADER, "shaders/screenQuad.vert.glsl");
	buildCubemapShader.addShader(GL_GEOMETRY_SHADER, "shaders/cubemapShader.geom.glsl");
	buildCubemapShader.addShader(GL_FRAGMENT_SHADER, "shaders/buildCubemap.frag.glsl");
	
	buildCubemapShader.use();

	buildCubemapShader.setInt("uIndex", 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, equirectangularMap);
	buildCubemapShader.setInt("uEquirectangularMap", 0);
	
	const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	const std::vector<glm::mat4> views =
	{
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	for (size_t i = 0; i < views.size(); i++)
	{
		buildCubemapShader.setMat4(std::format("uViewProjectMatrices[{}]", i), projection * views[i]);
	}

	glClear(GL_COLOR_BUFFER_BIT);

	const auto& prim = cube->getPrimitives()[0];

	glBindVertexArray(prim.vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prim.indicesBuffer);

	glDrawElements(prim.mode, static_cast<GLsizei>(prim.count), prim.componentType, (void*)0);

	glBindVertexArray(0);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glDeleteTextures(1, &equirectangularMap);
	glDeleteFramebuffers(1, &captureCubemapFBO);

	backgroundShader.addShader(GL_VERTEX_SHADER, "shaders/backgroundCube.vert.glsl");
	backgroundShader.addShader(GL_FRAGMENT_SHADER, "shaders/backgroundCube.frag.glsl");
}

void PBRRenderer::initializeShadowMaps()
{
	shadowMapShader.addShader(GL_VERTEX_SHADER, "shaders/model.vert.glsl");
	shadowMapShader.addShader(GL_GEOMETRY_SHADER, "shaders/cubemapShader.geom.glsl");
	shadowMapShader.addShader(GL_FRAGMENT_SHADER, "shaders/depthShader.frag.glsl");

	glGenTextures(1, &shadowCubemapArray);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, shadowCubemapArray);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_GEQUAL);

	glTexImage3D(
		GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT32F,
		shadowMapDimensions, shadowMapDimensions, 0, 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, 0);

	glGenFramebuffers(1, &shadowMapFBO);
}

void PBRRenderer::resizeShadowMaps()
{
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, shadowCubemapArray);

	glTexImage3D(
		GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT32F,
		shadowMapDimensions, shadowMapDimensions, 6 * scene->sceneLights.size(), 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, 0);

	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
}

void PBRRenderer::buildShadowMaps(const std::vector<ShaderLight>& lights)
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

	glViewport(0, 0, shadowMapDimensions, shadowMapDimensions);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_DEPTH_BUFFER_BIT);

	glCullFace(GL_FRONT);

	shadowMapShader.use();

	for (int i = 0; i < lights.size(); i++)
	{
		const ShaderLight& light = lights[i];
		const glm::vec3 lightPosition = glm::vec3(light.position);

		glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f),
			static_cast<float>(shadowMapDimensions) / static_cast<float>(shadowMapDimensions),
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
			shadowMapShader.setMat4(std::format("uViewProjectMatrices[{}]", j), shadowTransforms[j]);
		}

		shadowMapShader.setInt("uIndex", i);

		shadowMapShader.setVec2("uShadowMapViewPlanes", { shadowNearPlane, shadowFarPlane });
		shadowMapShader.setVec3("uLightPos", lightPosition);

		for (size_t k = 0; k < scene->sceneModels.size(); k++)
		{
			const auto& model = scene->sceneModels[k];

			for (const auto& prim : model->getPrimitives())
			{
				renderPrimitive(prim, shadowMapShader);
			}
		}
	}

	glCullFace(GL_BACK);
}

void PBRRenderer::cleanShadowMaps()
{
	glDeleteTextures(1, &shadowCubemapArray);
	glDeleteFramebuffers(1, &shadowMapFBO);
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

		deferredPassShader.setVec2("uShadowMapViewPlanes", { shadowNearPlane, shadowFarPlane });
	}

	for (size_t i = 0; i < gBufferTextures.size(); i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, gBufferTextures[i]);

		deferredPassShader.setInt(std::format("g{}", i), i);
	}

	deferredPassShader.setVec2("uScreenDimensions", dimensions);

	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);

	deferredPassShader.setInt("uEnvironmentMap", 6);

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

void PBRRenderer::cleanDeferredPass()
{
	glDeleteTextures(gBufferTextures.size(), gBufferTextures.data());
	glDeleteRenderbuffers(1, &gBufferDepth);
	glDeleteFramebuffers(1, &gBufferFBO);
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
		forwardPassShader.setVec2("uShadowMapViewPlanes", { shadowNearPlane, shadowFarPlane });
	}

	// Render opaque primitives - only if there is no deferred pass running
	if (!flags[DEFERRED_PASS_ENABLED])
	{
		for (size_t i = 0; i < scene->sceneModels.size(); i++)
		{
			forwardPassShader.setInt("uJointsOffset", jointOffsets[i]);
		
			for (const auto& prim : scene->sceneModels[i]->getOpaquePrimitives())
			{
				loadMaterialProperties(scene->sceneModels[i]->getTextures(), prim.materialDesc, forwardPassShader);

				renderPrimitive(prim, forwardPassShader);
			}
		}
	}

	// Enable blending and render the translucent primitives

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	for (size_t i = 0; i < scene->sceneModels.size(); i++)
	{
		forwardPassShader.setInt("uJointsOffset", jointOffsets[i]);

		for (const auto& prim : scene->sceneModels[i]->getTranslucentPrimitives())
		{
			loadMaterialProperties(scene->sceneModels[i]->getTextures(), prim.materialDesc, forwardPassShader);

			renderPrimitive(prim, forwardPassShader);
		}

	}
		
	glDisable(GL_BLEND);
}

void PBRRenderer::cleanForwardPass()
{
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

void PBRRenderer::cleanHdrPass()
{
	glDeleteTextures(1, &hdrPassColour);
	glDeleteRenderbuffers(1, &hdrPassDepthRBO);
	glDeleteFramebuffers(1, &hdrPassFBO);
}

