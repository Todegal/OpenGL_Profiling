#include "pbrRenderer_old.h"

#include "orbitCamera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include <spdlog/spdlog.h>

#include <stb_image.h>

#include <algorithm>
#include <numeric>
#include <execution>

PBRRenderer_old::PBRRenderer_old(glm::ivec2 screenSize, std::shared_ptr<Camera> cameraPtr)
	: dimensions(screenSize), viewCameraPtr(cameraPtr)
{
	//sphere(RenderableModel::constructUnitSphere(16, 16).getPrimitives()[0]),
	//	quad(RenderableModel::constructUnitQuad().getPrimitives()[0]), cube(RenderableModel::constructUnitCube().getPrimitives()[0])

	primitiveModels.push_back(RenderableModel::constructUnitSphere(16, 16));
	sphere = primitiveModels[0]->getPrimitives()[0];

	primitiveModels.push_back(RenderableModel::constructUnitCube());
	cube = primitiveModels[1]->getPrimitives()[0];

	primitiveModels.push_back(RenderableModel::constructUnitQuad());
	quad = primitiveModels[2]->getPrimitives()[0];

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

	pointShadowMapDimensions = 1024;
	directionalShadowMapDimensions = 4096;
	initializeShadowMaps();

	initializeDeferredPass();
	initializeForwardPass();
	initializeHdrPass();

	flags[NORMALS_ENABLED] = true;
	flags[OCCLUSION_ENABLED] = true;
	flags[SHADOWS_ENABLED] = true;
	flags[ENVIRONMENT_MAP_ENABLED] = true;
	flags[EMULATE_SUN_ENABLED] = true;
	flags[DEFERRED_PASS_ENABLED] = false;
	flags[HDR_PASS_ENABLED] = true;
}

PBRRenderer_old::~PBRRenderer_old()
{
	clearScene();

	cleanShadowMaps();
	cleanDeferredPass();
	cleanForwardPass();
	cleanHdrPass();
}

void PBRRenderer_old::loadScene(std::shared_ptr<Scene> s)
{
	scene = s;

	if (scene->environmentMap != "")
	{
		loadEnvironmentMap();
	}

	loadLights();

	resizeShadowMaps();
}

void PBRRenderer_old::clearScene()
{
	scene->sceneModels.clear();
	scene->sceneLights.clear();

	glDeleteBuffers(1, &lightBuffer);
	glDeleteBuffers(1, &jointsBuffer);
}

void PBRRenderer_old::setCamera(std::shared_ptr<Camera> cameraPtr)
{
	viewCameraPtr = cameraPtr;
	generateProjectionMatrix();
}

void PBRRenderer_old::frame()
{
	loadLights();
	//resizeShadowMaps();

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
	/*	std::sort(std::execution::par, model->getTranslucentPrimitives().begin(), model->getTranslucentPrimitives().end(),
			[&](const MeshPrimitive& a, const MeshPrimitive& b) -> bool {
				const float aDist = glm::length2(viewCameraPtr->getEye() - a.transform->getWorldPosition());
				const float bDist = glm::length2(viewCameraPtr->getEye() - b.transform->getWorldPosition());

				return aDist < bDist;
			}
		);*/

	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, jointsBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4) * jointMatrices.size(), jointMatrices.data(), GL_STATIC_DRAW);

	if (flags[SHADOWS_ENABLED]) buildShadowMaps();

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
		depthPrepassShader.setMat4("uView", viewCameraPtr->getViewMatrix());
		depthPrepassShader.setVec3("uCameraPosition", viewCameraPtr->getEye());

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

		deferredPass();
	}

	// Standard forward pass 

	forwardPass();

	// Debug lights -- render sphere on each point light

	unlitShader.use();

	unlitShader.setMat4("uProjection", projectionMatrix);
	unlitShader.setMat4("uView", viewCameraPtr->getViewMatrix());

	glBindVertexArray(sphere->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere->indicesBuffer);

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

		glDrawElements(sphere->mode, static_cast<GLsizei>(sphere->count), sphere->componentType, (void*)0);
	}

	// Draw SUN

	//if (flags[EMULATE_SUN_ENABLED])
	//{
	//	for (int i = 1; i < 5; i++)
	//	{
	//		glm::mat4 transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-sunDirection * static_cast<float>(i))), glm::vec3(0.05f));

	//		unlitShader.setMat4("uModel", transform);
	//		glm::mat3 NM = transform;
	//		NM = glm::inverse(NM);
	//		NM = glm::transpose(NM);

	//		unlitShader.setMat3("uNormalMatrix", NM);

	//		unlitShader.setVec4("uBaseColour.factor", glm::vec4(glm::normalize(sunColour), 1.0f));

	//		glDrawElements(sphereMesh.mode, static_cast<GLsizei>(sphereMesh.count), sphereMesh.componentType, (void*)0);
	//	}
	//}

		
	glBindVertexArray(0);

	// Draw skybox
	if (flags[ENVIRONMENT_MAP_ENABLED])
	{
		glDisable(GL_CULL_FACE);

		skyboxShader.use();

		skyboxShader.setMat4("uProjection", projectionMatrix);
		skyboxShader.setMat4("uView", viewCameraPtr->getViewMatrix());

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);

		skyboxShader.setBool("uEnvironmentMap", 0);
		skyboxShader.setFloat("uEnvironmentMapFactor", environmentMapFactor);

		glBindVertexArray(cube->vertexArray);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube->indicesBuffer);

		glDrawElements(cube->mode, static_cast<GLsizei>(cube->count), cube->componentType, (void*)0);

		glBindVertexArray(0);

		glEnable(GL_CULL_FACE);
	}

	// Run Post-Processing - currently just a HDR pass
	if (flags[HDR_PASS_ENABLED])
		hdrPass();
}

void PBRRenderer_old::setFlag(const int flag, bool value)
{
	assert(flag >= 0);
	assert(flag < NUM_FLAGS);

	flags[flag] = value;
}

void PBRRenderer_old::drawFlagsDialog(imgui_data data)
{
	int windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

	if (!ImGui::Begin("PBR Renderer Flags", &data.showRenderFlags, windowFlags))
	{
		ImGui::End();
		return;
	}

	ImGui::Checkbox("Shadows Enabled", &flags[SHADOWS_ENABLED]);
	if (ImGui::SliderInt("Point Shadow Map Dimensions", &pointShadowMapDimensions, 32, 4096))
	{
		resizeShadowMaps();
	}

	if (ImGui::SliderInt("Directional Shadow Map Dimensions", &directionalShadowMapDimensions, 32, 4096*2))
	{
		resizeShadowMaps();
	}

	ImGui::Separator();
	ImGui::Checkbox("Normals Enabled", &flags[NORMALS_ENABLED]);
	ImGui::Checkbox("Occlusion Enabled", &flags[OCCLUSION_ENABLED]);
	ImGui::Separator();
	ImGui::Checkbox("Environment Enabled", &flags[ENVIRONMENT_MAP_ENABLED]);
	
	if (flags[ENVIRONMENT_MAP_ENABLED])
	{
		ImGui::Text("Map: %s", scene->environmentMap.c_str());
		ImGui::SliderFloat("Factor", &environmentMapFactor, 0.0f, 1.0f);

		ImGui::Checkbox("Emulate Sun Enabled", &flags[EMULATE_SUN_ENABLED]);
		ImGui::Text("\tSun Direction: %s", glm::to_string(sunDirection).c_str());
		ImGui::Text("\tSun Colour: %s", glm::to_string(sunColour).c_str());
		ImGui::SliderFloat("\tSun Factor", &sunFactor, 0.0f, 1.0f);
	}

	ImGui::Separator();
	ImGui::Checkbox("Deferred Pass Enabled", &flags[DEFERRED_PASS_ENABLED]);
	ImGui::Checkbox("HDR Pass Enabled", &flags[HDR_PASS_ENABLED]);

	ImGui::End();
}

void PBRRenderer_old::resize(glm::ivec2 screenSize)
{
	dimensions = screenSize;

	generateProjectionMatrix();

	resizeDeferredPass();

	resizeForwardPass();

	resizeHdrPass();
}

const std::vector<glm::vec4> PBRRenderer_old::getFrustumCorners(const glm::mat4& pojectionViewMatrix) const
{
	const glm::mat4 invMatrix = glm::inverse(pojectionViewMatrix);

	std::vector<glm::vec4> corners{};

	for (int i = 0; i < 8; ++i) {
		glm::vec3 ndc(
			(i & 1) ? 1.f : -1.f,
			(i & 2) ? 1.f : -1.f,
			(i & 4) ? 1.f : -1.f
		);
		glm::vec4 pt = invMatrix * glm::vec4(ndc, 1.0f);
		corners.push_back(pt / pt.w);
	}

	return corners;
}

void PBRRenderer_old::generateProjectionMatrix()
{
	constexpr float farPlane = 100.0f;
	float aspectRatio = static_cast<float>(dimensions.x) / static_cast<float>(dimensions.y);
	projectionMatrix = glm::perspective(glm::radians(viewCameraPtr->getFov()), aspectRatio, 0.1f, farPlane);

	for (size_t i = 0; i < NUM_CASCADES; i++)
	{
		cascadeFarPlanes[i] = (farPlane * cascadeRatios[i]);
	}
}

void PBRRenderer_old::renderPrimitive(const std::shared_ptr<MeshPrimitive> prim, ShaderProgram& program)
{
	program.setMat4("uModel", prim->transform->getWorldTransform());
	glm::mat3 NM = prim->transform->getWorldTransform();
	NM = glm::inverse(NM);
	NM = glm::transpose(NM);

	program.setMat3("uNormalMatrix", NM);

	glBindVertexArray(prim->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prim->indicesBuffer);

	glDrawElements(prim->mode, static_cast<GLsizei>(prim->count), prim->componentType, (void*)prim->byteOffset);

	glBindVertexArray(0);
}

void PBRRenderer_old::loadMaterialProperties(const std::vector<GLuint>& textures, const tinygltf::Material& materialDesc, ShaderProgram& shader)
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

void PBRRenderer_old::loadEnvironmentMap()
{
	// Load the map into a 2d texture
	stbi_set_flip_vertically_on_load(true);

	int width, height, channels;
	float* data = stbi_loadf(scene->environmentMap.c_str(), &width, &height, &channels, 3);
	if (!data || width + height < 2 || channels < 3)
	{
		spdlog::error("Failed to load environment map: {}", scene->environmentMap);
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

	if (flags[EMULATE_SUN_ENABLED]) determineSunProperties(data, width, height, channels);

	stbi_image_free(data);

	glGenTextures(1, &environmentMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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


	GLuint captureCubemapFBO;
	glGenFramebuffers(1, &captureCubemapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, captureCubemapFBO);

	glViewport(0, 0, environmentMapDimensions, environmentMapDimensions);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	// test but let's see what happens
	ShaderProgram buildCubemapShader;
	buildCubemapShader.addShader(GL_VERTEX_SHADER, "shaders/modelView.vert.glsl");
	buildCubemapShader.addShader(GL_FRAGMENT_SHADER, "shaders/buildCubemap.frag.glsl");
	
	buildCubemapShader.use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, equirectangularMap);
	buildCubemapShader.setInt("uEquirectangularMap", 0);

	buildCubemapShader.setMat4("uModel", glm::mat4(1.0f));
	
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

	for (size_t i = 0; i < 6; i++)
	{
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, environmentMap, 0, i);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);

		assert(glIsTexture(environmentMap) == GL_TRUE);

		GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (e != GL_FRAMEBUFFER_COMPLETE)
		{
			spdlog::critical("Failed to bind FBO!");
			flags[ENVIRONMENT_MAP_ENABLED] = false;
			return;
		}

		buildCubemapShader.setMat4("uView", views[i]);
		buildCubemapShader.setMat4("uProjection", projection);
	
		glClear(GL_COLOR_BUFFER_BIT);

		glBindVertexArray(cube->vertexArray);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube->indicesBuffer);

		glDrawElements(cube->mode, static_cast<GLsizei>(cube->count), cube->componentType, (void*)0);

		glBindVertexArray(0);
	}

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glDeleteTextures(1, &equirectangularMap);
	glDeleteFramebuffers(1, &captureCubemapFBO);

	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	skyboxShader.addShader(GL_VERTEX_SHADER, "shaders/backgroundCube.vert.glsl");
	skyboxShader.addShader(GL_FRAGMENT_SHADER, "shaders/backgroundCube.frag.glsl");

	environmentMapFactor = 1.0f;

	prefilterEnvironmentMap();
}

// the plan is:
// find the brightest pixel, in a stupid naive way
// find it's direction in 3d space
// calculate the colour of said pixel (& normalize it, nonHDR)
// calculate the brightness of said pixel
// multiply by (1 - environmentMapFactor) to try and account for the remaining light...
// let's go!
void PBRRenderer_old::determineSunProperties(float* data, int width, int height, int channels)
{
	float maxLuminance = -1.0f;
	int maxX = 0, maxY = 0;

	for (int i = 0; i < width * height * channels; i += channels)
	{
		int pixel_index = i / channels;
		int x = pixel_index % width;
		int y = pixel_index / width;

		float r = data[i];
		float g = data[i + 1];
		float b = data[i + 2];

		float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;

		if (luminance > maxLuminance) {
			maxLuminance = luminance;
			maxX = x;
			maxY = y;
		}
	}

	int kernelSize = 512; // adjustable radius
	glm::vec3 sumColour(0.0f);

	for (int dy = -kernelSize; dy <= kernelSize; ++dy)
	{
		int y = maxY + dy;
		if (y < 0 || y >= height) continue;

		for (int dx = -kernelSize; dx <= kernelSize; ++dx)
		{
			int x = maxX + dx;
			if (x < 0 || x >= width) continue;

			int idx = (y * width + x) * channels;
			float r = data[idx];
			float g = data[idx + 1];
			float b = data[idx + 2];

			glm::vec3 rgb(r, g, b);

			sumColour += rgb;
		}
	}

	glm::vec3 averageColor = sumColour / static_cast<float>(kernelSize * kernelSize);

	float u = static_cast<float>(maxX) / static_cast<float>(width);
	float v = static_cast<float>(maxY) / static_cast<float>(height);

	float phi = u * 2.0f * glm::pi<float>();
	float theta = v * glm::pi<float>();

	sunDirection = glm::vec3(
		sinf(theta) * cosf(phi),
		cosf(theta),
		sinf(theta) * sinf(phi)
	);

	sunColour = averageColor;
	sunFactor = 1.0f; 

}

void PBRRenderer_old::prefilterEnvironmentMap()
{
	// First generate the irradiance map
	glGenTextures(1, &irradianceMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
	
	constexpr int IRRADIANCE_DIM = 32;

	for (size_t i = 0; i < 6; i++)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB32F,
			IRRADIANCE_DIM, IRRADIANCE_DIM,
			0, GL_RGB, GL_FLOAT, nullptr);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glViewport(0, 0, IRRADIANCE_DIM, IRRADIANCE_DIM);

	GLuint captureCubemapFBO;
	glGenFramebuffers(1, &captureCubemapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, captureCubemapFBO);

	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, irradianceMap, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (e != GL_FRAMEBUFFER_COMPLETE)
	{
		spdlog::critical("Failed to bind FBO!");
		flags[ENVIRONMENT_MAP_ENABLED] = false;
		return;
	}

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	ShaderProgram buildIrradianceShader;
	buildIrradianceShader.addShader(GL_VERTEX_SHADER, "shaders/screenQuad.vert.glsl");
	buildIrradianceShader.addShader(GL_GEOMETRY_SHADER, "shaders/cubemapShader.geom.glsl");
	buildIrradianceShader.addShader(GL_FRAGMENT_SHADER, "shaders/buildIrradiance.frag.glsl");

	buildIrradianceShader.use();

	buildIrradianceShader.setInt("uIndex", 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
	buildIrradianceShader.setInt("uEnvironmentMap", 0);

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
		buildIrradianceShader.setMat4(std::format("uViewProjectMatrices[{}]", i), projection * views[i]);
	}

	glBindVertexArray(cube->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube->indicesBuffer);

	glDrawElements(cube->mode, static_cast<GLsizei>(cube->count), cube->componentType, (void*)0);

	glBindVertexArray(0);

	// Generate Prefiltered Map

	glGenTextures(1, &prefilteredMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, prefilteredMap);

	constexpr int PREFILTERED_DIMENSIONS = 256;

	for (size_t i = 0; i < 6; i++)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB32F,
			PREFILTERED_DIMENSIONS, PREFILTERED_DIMENSIONS,
			0, GL_RGB, GL_FLOAT, nullptr);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	ShaderProgram buildPrefilteredShader;
	buildPrefilteredShader.addShader(GL_VERTEX_SHADER, "shaders/screenQuad.vert.glsl");
	buildPrefilteredShader.addShader(GL_GEOMETRY_SHADER, "shaders/cubemapShader.geom.glsl");
	buildPrefilteredShader.addShader(GL_FRAGMENT_SHADER, "shaders/buildPrefiltered.frag.glsl");

	buildPrefilteredShader.use();

	buildPrefilteredShader.setInt("uIndex", 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
	buildPrefilteredShader.setInt("uEnvironmentMap", 0);
	buildPrefilteredShader.setInt("uEnvironmentMapDimensions", environmentMapDimensions);

	for (size_t i = 0; i < views.size(); i++)
	{
		buildPrefilteredShader.setMat4(std::format("uViewProjectMatrices[{}]", i), projection * views[i]);
	}

	glBindVertexArray(cube->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube->indicesBuffer);

	constexpr int MAX_MIPMAP_LEVEL = 5;

	for (int i = 0; i < MAX_MIPMAP_LEVEL; i++)
	{
		const int mipSize = PREFILTERED_DIMENSIONS * glm::pow(0.5f, i);

		glViewport(0, 0, mipSize, mipSize);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, prefilteredMap, i);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);

		e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (e != GL_FRAMEBUFFER_COMPLETE)
		{
			spdlog::critical("Failed to bind FBO!");
			flags[ENVIRONMENT_MAP_ENABLED] = false;
			return;
		}

		buildPrefilteredShader.setFloat("uRoughness", static_cast<float>(i) / static_cast<float>(MAX_MIPMAP_LEVEL - 1));
		
		glDrawElements(cube->mode, static_cast<GLsizei>(cube->count), cube->componentType, (void*)0);
	}

	glBindVertexArray(0);

	// Generate BRDF LUT

	constexpr int LUT_DIMENSIONS = 512;

	glGenTextures(1, &brdfLUT);
	glBindTexture(GL_TEXTURE_2D, brdfLUT);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, LUT_DIMENSIONS, LUT_DIMENSIONS, 0, GL_RG, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, brdfLUT, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (e != GL_FRAMEBUFFER_COMPLETE)
	{
		spdlog::critical("Failed to bind FBO!");
		flags[ENVIRONMENT_MAP_ENABLED] = false;
		return;
	}

	ShaderProgram buildLUTShader;
	buildLUTShader.addShader(GL_VERTEX_SHADER, "shaders/screenQuad.vert.glsl");
	buildLUTShader.addShader(GL_FRAGMENT_SHADER, "shaders/buildBRDF.frag.glsl");

	buildLUTShader.use();

	glViewport(0, 0, LUT_DIMENSIONS, LUT_DIMENSIONS);

	glBindVertexArray(quad->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad->indicesBuffer);

	glDrawElements(quad->mode, static_cast<GLsizei>(quad->count), quad->componentType, (void*)0);

	glBindVertexArray(0);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glDeleteFramebuffers(1, &captureCubemapFBO);
}

void PBRRenderer_old::loadLights()
{
	// Parse lights, and calculate radii, and split them based on type (only for shadows)

	directionalLights.clear();
	pointLights.clear();

	for (const auto& l : scene->sceneLights)
	{
		ShaderLight light = {
			l.position,
			std::sqrtf(l.strength / lightCutoff),
			l.colour * l.strength,
			l.type
		};

		switch (l.type)
		{
		case Light::POINT:
			pointLights.push_back(light);
			break;
		case Light::DIRECTIONAL:
			directionalLights.push_back(light);
			break;
		default:
			break;
		}
	}

	if (flags[EMULATE_SUN_ENABLED] && flags[ENVIRONMENT_MAP_ENABLED])
	{
		ShaderLight l;
		l.position = sunDirection;
		l.radius = shadowFarPlane;
		l.radiance = sunColour * sunFactor;
		l.type = Light::DIRECTIONAL;

		directionalLights.push_back(l);
	}

	numLights = pointLights.size() + directionalLights.size();

	// If shadows are enabled, calculate cascade matrices for every directional light
	if (flags[SHADOWS_ENABLED])
	{
		for (auto& light : directionalLights)
		{
			const glm::vec3 lightDirection = glm::vec3(light.position);

			for (size_t j = 0; j < NUM_CASCADES; j++)
			{
				const float farPlane = cascadeFarPlanes[j];
				const float nearPlane = (j == 0) ? 0.0001f : cascadeFarPlanes[j - 1];

				// Determine bounds
				float aspectRatio = static_cast<float>(dimensions.x) / static_cast<float>(dimensions.y);
				glm::mat4 cutProjection = glm::perspective(glm::radians(viewCameraPtr->getFov()), aspectRatio, nearPlane, farPlane);

				// get world-space frustum coordinates
				const std::vector<glm::vec4> corners = getFrustumCorners(cutProjection * viewCameraPtr->getViewMatrix());

				glm::vec3 center = glm::vec3(0.0f);
				for (const auto& v : corners)
				{
					center += glm::vec3(v);
				}
				center /= 8;

				glm::mat4 view = glm::lookAt(center - lightDirection, center, glm::vec3(0.0f, 1.0f, 0.0f));

				float minX = std::numeric_limits<float>::max();
				float maxX = std::numeric_limits<float>::lowest();
				float minY = std::numeric_limits<float>::max();
				float maxY = std::numeric_limits<float>::lowest();
				for (const auto& v : corners)
				{
					const auto trf = view * v;
					minX = std::min(minX, trf.x);
					maxX = std::max(maxX, trf.x);
					minY = std::min(minY, trf.y);
					maxY = std::max(maxY, trf.y);
				}

				glm::mat4 projection = glm::ortho(minX, maxX, minY, maxY, -shadowFarPlane, shadowFarPlane);

				light.lightSpaceMatrices[j] = projection * view;
			}
		}
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ShaderLight) * numLights, 0, GL_STATIC_DRAW); // Resize the buffer
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
		sizeof(ShaderLight) * directionalLights.size(), directionalLights.data()); // First the directional lights

	glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(ShaderLight) * directionalLights.size(),
		sizeof(ShaderLight) * pointLights.size(), pointLights.data()); // ... Point lights next

	resizeShadowMaps();
}

void PBRRenderer_old::initializeShadowMaps()
{
	pointShadowMapShader.addShader(GL_VERTEX_SHADER, "shaders/model.vert.glsl");
	pointShadowMapShader.addShader(GL_GEOMETRY_SHADER, "shaders/cubemapShader.geom.glsl");
	pointShadowMapShader.addShader(GL_FRAGMENT_SHADER, "shaders/linearDepthShader.frag.glsl");

	directionalShadowMapShader.addShader(GL_VERTEX_SHADER, "shaders/model.vert.glsl");
	directionalShadowMapShader.addShader(GL_GEOMETRY_SHADER, "shaders/buildCascades.geom.glsl");
	directionalShadowMapShader.addShader(GL_FRAGMENT_SHADER, "shaders/depthShader.frag.glsl");

	glGenTextures(1, &pointShadowCubemapArray);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, pointShadowCubemapArray);

	glTexImage3D(
		GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT32F,
		pointShadowMapDimensions, pointShadowMapDimensions, 0, 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, 0);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);

	glGenTextures(1, &directionalShadowMapArray);
	glBindTexture(GL_TEXTURE_2D_ARRAY, directionalShadowMapArray);

	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
		directionalShadowMapDimensions, directionalShadowMapDimensions, 0, 0, 
		GL_DEPTH_COMPONENT, GL_FLOAT, 0);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);

	glGenFramebuffers(1, &shadowMapFBO);
}

void PBRRenderer_old::resizeShadowMaps()
{
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, pointShadowCubemapArray);

	glTexImage3D(
		GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT32F,
		pointShadowMapDimensions, pointShadowMapDimensions, 6 * pointLights.size(), 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, 0);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);

	glBindTexture(GL_TEXTURE_2D_ARRAY, directionalShadowMapArray);

	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
		directionalShadowMapDimensions, directionalShadowMapDimensions, directionalLights.size() * NUM_CASCADES, 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, 0);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_GREATER);

	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
}

void PBRRenderer_old::buildShadowMaps()
{
	buildPointShadowMaps();
	buildDirectionalShadowMaps();
}

void PBRRenderer_old::buildPointShadowMaps()
{
	if (pointLights.size() == 0) return;

	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, pointShadowCubemapArray, 0);

	assert(glIsTexture(pointShadowCubemapArray) == GL_TRUE);

	GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (e != GL_FRAMEBUFFER_COMPLETE)
	{
		spdlog::critical("Failed to bind shadow map FBO!");
		flags[SHADOWS_ENABLED] = false;
		return;
	}

	glViewport(0, 0, pointShadowMapDimensions, pointShadowMapDimensions);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_DEPTH_BUFFER_BIT);

	glCullFace(GL_FRONT);

	pointShadowMapShader.use();

	for (int i = 0; i < pointLights.size(); i++)
	{
		const ShaderLight& light = pointLights[i];
		const glm::vec3 lightPosition = glm::vec3(light.position);

		glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f),
			static_cast<float>(pointShadowMapDimensions) / static_cast<float>(pointShadowMapDimensions),
			shadowNearPlane, light.radius);

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

		pointShadowMapShader.setVec2("uShadowMapViewPlanes", { shadowNearPlane, shadowFarPlane });
		pointShadowMapShader.setVec3("uLightPos", lightPosition);

		for (size_t k = 0; k < scene->sceneModels.size(); k++)
		{
			const auto& model = scene->sceneModels[k];

			for (const auto& prim : model->getPrimitives())
			{
				renderPrimitive(prim, pointShadowMapShader);
			}
		}
	}

	glCullFace(GL_BACK);
}

void PBRRenderer_old::buildDirectionalShadowMaps()
{
	if (directionalLights.size() == 0) return;

	glCullFace(GL_FRONT);

	directionalShadowMapShader.use();

	for (int i = 0; i < directionalLights.size(); i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, directionalShadowMapArray, 0);

		assert(glIsTexture(directionalShadowMapArray) == GL_TRUE);

		GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (e != GL_FRAMEBUFFER_COMPLETE)
		{
			spdlog::critical("Failed to bind shadow map FBO!");
			flags[SHADOWS_ENABLED] = false;
			return;
		}

		glViewport(0, 0, directionalShadowMapDimensions, directionalShadowMapDimensions);
		
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_DEPTH_BUFFER_BIT);

		ShaderLight& light = directionalLights[i];
		
		for (size_t j = 0; j < light.lightSpaceMatrices.size(); j++)
		{
			directionalShadowMapShader.setMat4(std::format("uLightSpaceMatrices[{}]", j), light.lightSpaceMatrices[j]);
		}

		// now render the scene for each cascade, using geometry shader

		directionalShadowMapShader.setInt("uIndex", i);

		for (size_t k = 0; k < scene->sceneModels.size(); k++)
		{
			const auto& model = scene->sceneModels[k];

			for (const auto& prim : model->getPrimitives())
			{
				renderPrimitive(prim, directionalShadowMapShader);
			}
		}
	}

	glCullFace(GL_BACK);
}

void PBRRenderer_old::cleanShadowMaps()
{
	glDeleteTextures(1, &pointShadowCubemapArray);
	glDeleteTextures(1, &directionalShadowMapArray);
	glDeleteFramebuffers(1, &shadowMapFBO);
}

void PBRRenderer_old::initializeDeferredPass()
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

void PBRRenderer_old::resizeDeferredPass()
{
	for (size_t i = 0; i < gBufferTextures.size(); i++)
	{
		glBindTexture(GL_TEXTURE_2D, gBufferTextures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, dimensions.x, dimensions.y, 0, GL_RGBA, GL_FLOAT, nullptr);
	}

	glBindRenderbuffer(GL_RENDERBUFFER, gBufferDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, dimensions.x, dimensions.y);
}

void PBRRenderer_old::buildGBuffer()
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

	 //If we already have a depth pass blit that and save some overdraw
	 //Sounds good but really doesn't work :(
	if (flags[DEPTH_PREPASS_ENABLED])
	{
		glBlitNamedFramebuffer(0, gBufferFBO,
			0, 0, dimensions.x, dimensions.y,
			0, 0, dimensions.x, dimensions.y,
			GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}

	gBufferShader.use();

	gBufferShader.setMat4("uProjection", projectionMatrix);
	gBufferShader.setMat4("uView", viewCameraPtr->getViewMatrix());
	gBufferShader.setVec3("uCameraPosition", viewCameraPtr->getEye());

	for (size_t i = 0; i < scene->sceneModels.size(); i++)
	{
		gBufferShader.setInt("uJointsOffset", jointOffsets[i]);

		// ONLY OPAQUE
		for (const auto& prim : scene->sceneModels[i]->getOpaquePrimitives())
		{
			loadMaterialProperties(scene->sceneModels[i]->getTextures(), prim->materialDesc, gBufferShader);
			renderPrimitive(prim, gBufferShader);
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, flags[HDR_PASS_ENABLED] ? hdrPassFBO : 0);
}

void PBRRenderer_old::deferredPass()
{
	buildGBuffer();

	//glEnable(GL_BLEND);
	//glBlendEquation(GL_FUNC_ADD);
	//glBlendFunc(GL_ONE, GL_ONE);

	//glDisable(GL_DEPTH_TEST);

	deferredPassShader.use();

	deferredPassShader.setVec3("uCameraPosition", viewCameraPtr->getEye());

	deferredPassShader.setBool("uShadowsEnabled", flags[SHADOWS_ENABLED]);
	deferredPassShader.setInt("uNumLights", scene->sceneLights.size());

	if (flags[SHADOWS_ENABLED])
	{
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, pointShadowCubemapArray);

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
	//deferredPassShader.setMat4("uView", cameraPtr.getViewMatrix());

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

	glBindVertexArray(quad->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad->indicesBuffer);

	glDrawElements(quad->mode, static_cast<GLsizei>(quad->count), quad->componentType, (void*)0);

	glBindVertexArray(0);
	
	//glCullFace(GL_BACK);

	//glEnable(GL_DEPTH_TEST);
	//glDepthMask(GL_TRUE);
	//glDepthFunc(GL_LEQUAL);

	//glDisable(GL_BLEND);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, gBufferFBO);
	glBlitFramebuffer(0, 0, dimensions.x, dimensions.y, 0, 0, dimensions.x, dimensions.y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

}

void PBRRenderer_old::cleanDeferredPass()
{
	glDeleteTextures(gBufferTextures.size(), gBufferTextures.data());
	glDeleteRenderbuffers(1, &gBufferDepth);
	glDeleteFramebuffers(1, &gBufferFBO);
}

void PBRRenderer_old::initializeForwardPass()
{
	forwardPassShader.addShader(GL_VERTEX_SHADER, "shaders/modelView.vert.glsl");
	forwardPassShader.addShader(GL_FRAGMENT_SHADER, "shaders/pbr.frag.glsl");
}

void PBRRenderer_old::resizeForwardPass()
{
}

void PBRRenderer_old::forwardPass()
{
	forwardPassShader.use();

	forwardPassShader.setMat4("uProjection", projectionMatrix);
	forwardPassShader.setMat4("uView", viewCameraPtr->getViewMatrix());
	forwardPassShader.setVec3("uCameraPosition", viewCameraPtr->getEye());

	forwardPassShader.setBool("uShadowsEnabled", flags[SHADOWS_ENABLED]);
	forwardPassShader.setBool("uEnvironmentEnabled", flags[ENVIRONMENT_MAP_ENABLED]);
	forwardPassShader.setBool("uSunEnabled", flags[EMULATE_SUN_ENABLED] && flags[ENVIRONMENT_MAP_ENABLED]);
	forwardPassShader.setInt("uNumLights", numLights);
	forwardPassShader.setInt("uNumDirectionalLights", directionalLights.size());
	forwardPassShader.setInt("uNumPointLights", pointLights.size());

	if (flags[SHADOWS_ENABLED])
	{
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, pointShadowCubemapArray);

		forwardPassShader.setInt("uPointShadowMaps", 5);

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D_ARRAY, directionalShadowMapArray);

		forwardPassShader.setInt("uDirectionalShadowMaps", 6);

		forwardPassShader.setMat4("uLightSpaceMatrix", lightSpaceMatrix);

		forwardPassShader.setVec2("uShadowMapViewPlanes", { shadowNearPlane, shadowFarPlane });

		for (size_t i = 0; i < cascadeFarPlanes.size(); i++)
		{
			forwardPassShader.setFloat(std::format("uCascadeFarPlanes[{}]", i), cascadeFarPlanes[i]);
		}
	}

	if (flags[ENVIRONMENT_MAP_ENABLED])
	{
		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);

		forwardPassShader.setInt("uIrradianceMap", 7);

		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_CUBE_MAP, prefilteredMap);

		forwardPassShader.setInt("uPrefilteredMap", 8);

		glActiveTexture(GL_TEXTURE9);
		glBindTexture(GL_TEXTURE_2D, brdfLUT);

		forwardPassShader.setInt("uBRDF", 9);

		forwardPassShader.setFloat("uEnvironmentMapFactor", environmentMapFactor);

		if (flags[EMULATE_SUN_ENABLED])
		{
			forwardPassShader.setVec3("uSunDirection", sunDirection);
			forwardPassShader.setVec3("uSunColour", sunColour * sunFactor);
		}

	}

	// Render opaque primitives - only if there is no deferred pass running
	if (!flags[DEFERRED_PASS_ENABLED])
	{
		for (size_t i = 0; i < scene->sceneModels.size(); i++)
		{
			forwardPassShader.setInt("uJointsOffset", jointOffsets[i]);
		
			for (const auto& prim : scene->sceneModels[i]->getOpaquePrimitives())
			{
				loadMaterialProperties(scene->sceneModels[i]->getTextures(), prim->materialDesc, forwardPassShader);

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
			loadMaterialProperties(scene->sceneModels[i]->getTextures(), prim->materialDesc, forwardPassShader);

			renderPrimitive(prim, forwardPassShader);
		}

	}
		
	glDisable(GL_BLEND);
}

void PBRRenderer_old::cleanForwardPass()
{
}

void PBRRenderer_old::initializeHdrPass()
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

void PBRRenderer_old::resizeHdrPass()
{
	glBindRenderbuffer(GL_RENDERBUFFER, hdrPassDepthRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, dimensions.x, dimensions.y);

	glBindTexture(GL_TEXTURE_2D, hdrPassColour);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, dimensions.x, dimensions.y, 0, GL_RGBA, GL_FLOAT, 0);
}

void PBRRenderer_old::hdrPass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, dimensions.x, dimensions.y);

	glDisable(GL_DEPTH_TEST);

	hdrPassShader.use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrPassColour);
	hdrPassShader.setInt("uColour", 0);

	hdrPassShader.setVec2("uScreenDimensions", dimensions);

	glBindVertexArray(quad->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad->indicesBuffer);

	glDrawElements(quad->mode, static_cast<GLsizei>(quad->count), quad->componentType, (void*)0);

	glBindVertexArray(0);

	glEnable(GL_DEPTH_TEST);
}

void PBRRenderer_old::cleanHdrPass()
{
	glDeleteTextures(1, &hdrPassColour);
	glDeleteRenderbuffers(1, &hdrPassDepthRBO);
	glDeleteFramebuffers(1, &hdrPassFBO);
}

