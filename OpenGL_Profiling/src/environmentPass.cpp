#include "environmentPass.h"

#include <spdlog/spdlog.h>

#include <stb_image.h>

EnvironmentPass::EnvironmentPass(RenderContext& renderContext)
	: RenderPass(renderContext), environmentCubeMap(), environmentMapDimensions(2048), 
	cube(RenderableModel::constructUnitCube()), quad(RenderableModel::constructUnitQuad())
{
	loadEquirectangularMap();

	skyboxShader.addShader(GL_VERTEX_SHADER, "environment_pass/skybox.vert");
	skyboxShader.addShader(GL_FRAGMENT_SHADER, "environment_pass/skybox.frag");

	prefilterEnvironment();
}

EnvironmentPass::~EnvironmentPass()
{
	glDeleteTextures(1, &environmentCubeMap);
}

void EnvironmentPass::frame()
{
	glDisable(GL_CULL_FACE);

	skyboxShader.use();
	renderContext.buffers.bindBuffers(skyboxShader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentCubeMap);

	skyboxShader.setBool("uEnvironmentMap", 0);

	const auto& cubePrim = cube->getPrimitives()[0];
	glBindVertexArray(cubePrim->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubePrim->indicesBuffer);

	glDrawElements(cubePrim->mode, static_cast<GLsizei>(cubePrim->count), cubePrim->componentType, (void*)0);

	glBindVertexArray(0);

	glEnable(GL_CULL_FACE);
}

void EnvironmentPass::refresh()
{
}

void EnvironmentPass::loadEquirectangularMap()
{
	// Load the map into a 2d texture
	stbi_set_flip_vertically_on_load(true);

	int width, height, channels;
	float* data = stbi_loadf(renderContext.scene->environmentMap.c_str(), &width, &height, &channels, 3);
	if (!data || width + height < 2 || channels < 3)
	{
		spdlog::error("Failed to load environment map: {}", renderContext.scene->environmentMap);
		renderContext.flags[ENVIRONMENT_ENABLED] = false;
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

	glGenTextures(1, &environmentCubeMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentCubeMap);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

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

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	// test but let's see what happens
	ShaderProgram buildCubemapShader;
	buildCubemapShader.addShader(GL_VERTEX_SHADER, "environment_pass/build_cubemap.vert");
	buildCubemapShader.addShader(GL_FRAGMENT_SHADER, "environment_pass/build_cubemap.frag");

	buildCubemapShader.use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, equirectangularMap);
	buildCubemapShader.setInt("uEquirectangularMap", 0);

	const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	buildCubemapShader.setMat4("uProjectionMatrix", projection);

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
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, environmentCubeMap, 0, i);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);

		GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (e != GL_FRAMEBUFFER_COMPLETE)
		{
			spdlog::critical("Failed to bind FBO!");
			renderContext.flags[ENVIRONMENT_ENABLED] = false;
			return;
		}

		buildCubemapShader.setMat4("uViewMatrix", views[i]);

		const auto& cubePrim = cube->getPrimitives()[0];

		glClear(GL_COLOR_BUFFER_BIT);

		glBindVertexArray(cubePrim->vertexArray);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubePrim->indicesBuffer);

		glDrawElements(cubePrim->mode, static_cast<GLsizei>(cubePrim->count), cubePrim->componentType, (void*)0);

		glBindVertexArray(0);
	}

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glDeleteTextures(1, &equirectangularMap);
	glDeleteFramebuffers(1, &captureCubemapFBO);

	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentCubeMap);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

void EnvironmentPass::prefilterEnvironment()
{
	// First generate the irradiance map
	glGenTextures(1, &renderContext.textures["irradiance"]);
	glBindTexture(GL_TEXTURE_CUBE_MAP, renderContext.textures.at("irradiance"));

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

	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderContext.textures.at("irradiance"), 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	GLenum e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (e != GL_FRAMEBUFFER_COMPLETE)
	{
		spdlog::critical("Failed to bind FBO!");
		renderContext.flags[RenderFlags::ENVIRONMENT_ENABLED] = false;
		return;
	}

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	ShaderProgram buildIrradianceShader;
	buildIrradianceShader.addShader(GL_VERTEX_SHADER, "environment_pass/screen_quad.vert");
	buildIrradianceShader.addShader(GL_GEOMETRY_SHADER, "environment_pass/cubemap_views.geom");
	buildIrradianceShader.addShader(GL_FRAGMENT_SHADER, "environment_pass/ibl_irradiance.frag");

	buildIrradianceShader.use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentCubeMap);
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

	const auto& cubePrim = cube->getPrimitives()[0];

	glBindVertexArray(cubePrim->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubePrim->indicesBuffer);

	glDrawElements(cubePrim->mode, static_cast<GLsizei>(cubePrim->count), cubePrim->componentType, (void*)0);

	glBindVertexArray(0);

	// Generate Prefiltered Map

	glGenTextures(1, &renderContext.textures["prefilter"]);
	glBindTexture(GL_TEXTURE_CUBE_MAP, renderContext.textures.at("prefilter"));

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
	buildPrefilteredShader.addShader(GL_VERTEX_SHADER, "environment_pass/screen_quad.vert");
	buildPrefilteredShader.addShader(GL_GEOMETRY_SHADER, "environment_pass/cubemap_views.geom");
	buildPrefilteredShader.addShader(GL_FRAGMENT_SHADER, "environment_pass/ibl_prefilter.frag");

	buildPrefilteredShader.use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentCubeMap);
	buildPrefilteredShader.setInt("uEnvironmentMap", 0);
	buildPrefilteredShader.setInt("uEnvironmentMapDimensions", environmentMapDimensions);

	for (size_t i = 0; i < views.size(); i++)
	{
		buildPrefilteredShader.setMat4(std::format("uViewProjectMatrices[{}]", i), projection * views[i]);
	}

	glBindVertexArray(cubePrim->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubePrim->indicesBuffer);

	constexpr int MAX_MIPMAP_LEVEL = 5;

	for (int i = 0; i < MAX_MIPMAP_LEVEL; i++)
	{
		const int mipSize = PREFILTERED_DIMENSIONS * glm::pow(0.5f, i);

		glViewport(0, 0, mipSize, mipSize);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderContext.textures.at("prefilter"), i);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);

		e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (e != GL_FRAMEBUFFER_COMPLETE)
		{
			spdlog::critical("Failed to bind FBO!");
			renderContext.flags[RenderFlags::ENVIRONMENT_ENABLED] = false;
			return;
		}

		buildPrefilteredShader.setFloat("uRoughness", static_cast<float>(i) / static_cast<float>(MAX_MIPMAP_LEVEL - 1));

		glDrawElements(cubePrim->mode, static_cast<GLsizei>(cubePrim->count), cubePrim->componentType, (void*)0);
	}

	glBindVertexArray(0);

	// Generate BRDF LUT

	constexpr int LUT_DIMENSIONS = 512;

	glGenTextures(1, &renderContext.textures["brdf"]);
	glBindTexture(GL_TEXTURE_2D, renderContext.textures.at("brdf"));

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, LUT_DIMENSIONS, LUT_DIMENSIONS, 0, GL_RG, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderContext.textures.at("brdf"), 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (e != GL_FRAMEBUFFER_COMPLETE)
	{
		spdlog::critical("Failed to bind FBO!");
		renderContext.flags[RenderFlags::ENVIRONMENT_ENABLED] = false;
		return;
	}

	ShaderProgram buildLUTShader;
	buildLUTShader.addShader(GL_VERTEX_SHADER, "environment_pass/screen_quad.vert");
	buildLUTShader.addShader(GL_FRAGMENT_SHADER, "environment_pass/ibl_brdf.frag");

	buildLUTShader.use();

	buildLUTShader.setInt("uScreenDimensions", LUT_DIMENSIONS);

	glViewport(0, 0, LUT_DIMENSIONS, LUT_DIMENSIONS);

	const auto& quadPrim = quad->getPrimitives()[0];

	glBindVertexArray(quadPrim->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadPrim->indicesBuffer);

	glDrawElements(quadPrim->mode, static_cast<GLsizei>(quadPrim->count), quadPrim->componentType, (void*)0);

	glBindVertexArray(0);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glDeleteFramebuffers(1, &captureCubemapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
