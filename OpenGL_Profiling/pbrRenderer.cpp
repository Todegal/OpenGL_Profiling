#include "pbrRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <numeric>

PBRRenderer::PBRRenderer(glm::ivec2 screenSize, Camera& mainCamera)
	: dimensions(screenSize), camera(mainCamera)
{
	generateProjectionMatrix();

	pbrShader.addShader(GL_VERTEX_SHADER, "modelView.vert");
	pbrShader.addShader(GL_FRAGMENT_SHADER, "pbr.frag");

	pbrShader.linkProgram();

	unlitShader.addShader(GL_VERTEX_SHADER, "modelView.vert");
	unlitShader.addShader(GL_FRAGMENT_SHADER, "unlit.frag");

	depthCubemapShader.addShader(GL_VERTEX_SHADER, "model.vert");
	depthCubemapShader.addShader(GL_GEOMETRY_SHADER, "depthShader.geom");
	depthCubemapShader.addShader(GL_FRAGMENT_SHADER, "depthShader.frag");

	glGenBuffers(1, &lightBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, lightBuffer);

	glGenBuffers(1, &jointsBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, jointsBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, jointsBuffer);

	glGenFramebuffers(1, &depthMapFBO);

	sphere = Model::constructUnitSphere(16, 16);
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
			0.0f,
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
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, jointsBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4) * jointMatrices.size(), jointMatrices.data(), GL_STATIC_DRAW);

	if (flags[SHADOWS_ENABLED])
		renderShadowMaps(lights);

	glViewport(0, 0, dimensions.x, dimensions.y);

	glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	pbrShader.use();

	pbrShader.setMat4("uProjection", projectionMatrix);
	pbrShader.setMat4("uView", camera.getViewMatrix());
	pbrShader.setVec3("uCameraPosition", camera.getEye());

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ShaderLight) * lights.size(), lights.data(), GL_STATIC_DRAW);


	pbrShader.setBool("uShadowsEnabled", flags[SHADOWS_ENABLED]);

	if (flags[SHADOWS_ENABLED])
	{
		pbrShader.setInt("uNumLights", scene->sceneLights.size());

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, depthCubemapArray);

		pbrShader.setInt("uDepthMaps", 5);
		pbrShader.setFloat("uDepth", shadowFarPlane - shadowNearPlane);
	}

	for (size_t i = 0; i < scene->sceneModels.size(); i++)
	{
		pbrShader.setInt("uJointsOffset", jointOffsets[i]);

		for (const auto& prim : scene->sceneModels[i]->getPrimitives())
		{
			loadMaterialProperties(scene->sceneModels[i]->getTextures(), prim.materialDesc);

			renderPrimitive(prim, pbrShader);
		}
	} 

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
}

void PBRRenderer::loadScene(std::shared_ptr<Scene> s)
{
	scene = s;

	glGenTextures(1, &depthCubemapArray);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, depthCubemapArray);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexImage3D(
		GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT,
		shadowMapDimensions.x, shadowMapDimensions.x, 6 * scene->sceneLights.size(), 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	//size_t maxNumBones = std::accumulate(scene->sceneModels.begin(), scene->sceneModels.end(), 0,
	//	[](size_t max, const std::shared_ptr<Model> m)
	//	{
	//		return std::max(max, m->getJoints().size());
	//	}
	//);

	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, jointsBuffer);
	//glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4) * maxNumBones, nullptr, GL_STATIC_DRAW);
}

void PBRRenderer::clearScene()
{
	scene->sceneModels.clear();
	scene->sceneLights.clear();

	glDeleteFramebuffers(1, &depthMapFBO);
	glDeleteTextures(1, &depthCubemapArray);
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
}

void PBRRenderer::setCamera(const Camera& cam)
{
	camera = cam;
	generateProjectionMatrix();
}

void PBRRenderer::generateProjectionMatrix()
{
	float aspectRatio = static_cast<float>(dimensions.x) / static_cast<float>(dimensions.y);
	projectionMatrix = glm::perspective(glm::radians(camera.getFov()), aspectRatio, 0.0001f, 10000.0f);
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

void PBRRenderer::loadMaterialProperties(const std::vector<GLuint>& textures, const tinygltf::Material& materialDesc)
{
	const tinygltf::PbrMetallicRoughness& pbr = materialDesc.pbrMetallicRoughness;

	pbrShader.setVec4("uBaseColour.factor", { pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3] });
	if (pbr.baseColorTexture.index >= 0)
	{
		pbrShader.setBool("uBaseColour.isTextureEnabled", true);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textures[pbr.baseColorTexture.index]);
		pbrShader.setInt("uBaseColour.textureMap", 0);
	}
	else
	{
		pbrShader.setBool("uBaseColour.isTextureEnabled", false);
	}

	pbrShader.setVec4("uMetallicRoughness.factor", { 0, pbr.roughnessFactor, pbr.metallicFactor, 0 });
	if (pbr.metallicRoughnessTexture.index >= 0)
	{
		pbrShader.setBool("uMetallicRoughness.isTextureEnabled", true);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, textures[pbr.metallicRoughnessTexture.index]);
		pbrShader.setInt("uMetallicRoughness.textureMap", 1);
	}
	else
	{
		pbrShader.setBool("uMetallicRoughness.isTextureEnabled", false);
	}

	if (materialDesc.normalTexture.index >= 0 && flags[NORMALS_ENABLED])
	{
		pbrShader.setBool("uNormal.isTextureEnabled", true);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, textures[materialDesc.normalTexture.index]);
		pbrShader.setInt("uNormal.textureMap", 2);

		pbrShader.setVec4("uNormal.factor", { materialDesc.normalTexture.scale, 0, 0, 0 });
	}
	else
	{
		pbrShader.setBool("uNormal.isTextureEnabled", false);
	}

	if (materialDesc.occlusionTexture.index >= 0 && flags[OCCLUSION_ENABLED])
	{
		pbrShader.setBool("uOcclusion.isTextureEnabled", true);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, textures[materialDesc.occlusionTexture.index]);
		pbrShader.setInt("uOcclusion.textureMap", 3);

		pbrShader.setVec4("uOcclusion.factor", { materialDesc.occlusionTexture.strength, 0, 0, 0 });
	}
	else
	{
		pbrShader.setBool("uOcclusion.isTextureEnabled", false);
	}
}

void PBRRenderer::renderShadowMaps(const std::vector<ShaderLight>& lights)
{
	glViewport(0, 0, shadowMapDimensions.x, shadowMapDimensions.y);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemapArray, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

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

		depthCubemapShader.setFloat("uDepth", (shadowFarPlane - shadowNearPlane));
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

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glCullFace(GL_BACK);
}
