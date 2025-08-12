#include "pbrRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "shadowPass.h"

PBRRenderer::PBRRenderer(glm::ivec2 screenSize, std::shared_ptr<Camera> camera)
	: renderContext()
{
	renderContext.nearPlane = 0.001f;
	renderContext.farPlane = 100.0f;
	renderContext.dimensions = screenSize;
	renderContext.scene = std::make_shared<Scene>();

	this->camera = camera;

	renderContext.flags[RenderFlags::NORMALS_ENABLED] = true;
	renderContext.flags[RenderFlags::OCCLUSION_ENABLED] = false;
	renderContext.flags[RenderFlags::SHADOWS_ENABLED] = true;
	renderContext.flags[RenderFlags::ENVIRONMENT_ENABLED] = false;
	renderContext.flags[RenderFlags::EMULATE_SUN_ENABLED] = false;
	renderContext.flags[RenderFlags::DEFERRED_PASS_ENABLED] = false;
	renderContext.flags[RenderFlags::HDR_PASS_ENABLED] = true;
	renderContext.flags[RenderFlags::DEBUG_PASS_ENABLED] = true;

	renderPasses[SHADOW_PASS] = std::make_unique<ShadowPass>(renderContext);
	renderPasses[FORWARD_PASS] = std::make_unique<ForwardRenderPass>(renderContext);

	if (renderContext.flags[RenderFlags::DEBUG_PASS_ENABLED])
	{
		renderPasses[DEBUG_PASS] = std::make_unique<DebugPass>(renderContext);
	}

	if (renderContext.flags[RenderFlags::HDR_PASS_ENABLED])
	{
		renderPasses[HDR_PASS] = std::make_unique<HDRPass>(renderContext);
	}

	// create the required uniform buffers
	renderContext.buffers.addBuffer("flags", GL_UNIFORM_BUFFER, "FlagUniforms");
	renderContext.buffers.addBuffer("frame_uniforms", GL_UNIFORM_BUFFER, "FrameUniforms");
	renderContext.buffers.addBuffer("point_lights", GL_SHADER_STORAGE_BUFFER, "PointLightBuffer");
	renderContext.buffers.addBuffer("directional_lights", GL_SHADER_STORAGE_BUFFER, "DirectionalLightBuffer");
	renderContext.buffers.addBuffer("joints", GL_SHADER_STORAGE_BUFFER, "JointsBuffer");
	renderContext.buffers.addBuffer("instance", GL_SHADER_STORAGE_BUFFER, "InstanceBuffer");
}

PBRRenderer::~PBRRenderer()
{
	for (auto t : renderContext.textures)
	{
		glDeleteTextures(1, &t.second);
	}
}

void PBRRenderer::loadScene(std::shared_ptr<Scene> scene)
{
	renderContext.scene = scene;

	if (renderContext.scene->environmentMap != "")
	{
		renderPasses[ENVIRONMENT_PASS] = std::make_unique<EnvironmentPass>(renderContext);

		renderContext.flags[ENVIRONMENT_ENABLED] = true;
	}

	for (auto& pass : renderPasses)
	{
		pass->refresh();
	}
}

void PBRRenderer::clearScene()
{
	renderContext.scene = std::make_shared<Scene>();

	renderContext.flags[ENVIRONMENT_ENABLED] = false;
	renderPasses[ENVIRONMENT_PASS].reset();

	for (auto& pass : renderPasses)
	{
		pass->refresh();
	}
}

void PBRRenderer::setCamera(std::shared_ptr<Camera> camera)
{
	this->camera = camera;
}

void PBRRenderer::resize(glm::ivec2 screenSize)
{
	renderContext.dimensions = screenSize;

	for (auto& pass : renderPasses)
	{
		pass->refresh();
	}
}

void PBRRenderer::imguiFrame(imgui_data& data)
{
	int windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
	if (!ImGui::Begin("-- PBR Renderer --", &data.showRenderDialog, windowFlags))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Flags");
	ImGui::Checkbox("HDR Pass Enabled", &renderContext.flags[RenderFlags::HDR_PASS_ENABLED]);
	ImGui::Checkbox("Normal Mapping Enabled", &renderContext.flags[RenderFlags::NORMALS_ENABLED]);
	ImGui::Checkbox("Occlusion Mapping Enabled", &renderContext.flags[RenderFlags::OCCLUSION_ENABLED]);
	ImGui::Checkbox("Environment Enabled", &renderContext.flags[RenderFlags::ENVIRONMENT_ENABLED]);
	ImGui::Checkbox("Shadows Enabled", &renderContext.flags[RenderFlags::SHADOWS_ENABLED]);


	bool refresh = false;
	refresh |= ImGui::SliderInt("Point Shadow Map Dimensions", &renderContext.pointShadowMapDimensions, 4, 4096);
	refresh |= ImGui::SliderFloat("Point Shadow Near Plane", &renderContext.pointShadowNearPlane, 0, 1);
	refresh |= ImGui::SliderFloat("Point Shadow Far Plane", &renderContext.pointShadowFarPlane, 1, 1000);

	if (refresh) resize(renderContext.dimensions);

	ImGui::End();
}

void PBRRenderer::frame()
{
	ScopedFramebufferBind defaultFramebuffer(renderContext.framebufferStack, 0);

	glViewport(0, 0, renderContext.dimensions.x, renderContext.dimensions.y);

	buildBuffers();

	if (renderContext.flags[SHADOWS_ENABLED])
	{ renderPasses[SHADOW_PASS]->frame(); }

	{ // Bind hdr framebuffer inside this scope
		HDRPass* hdrPass = dynamic_cast<HDRPass*>(renderPasses[HDR_PASS].get());

		ScopedFramebufferBind framebufferBind(renderContext.framebufferStack,
			renderContext.flags[HDR_PASS_ENABLED] ? hdrPass->getFramebuffer() : 0);

		glViewport(0, 0, renderContext.dimensions.x, renderContext.dimensions.y);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderPasses[FORWARD_PASS]->frame();

		if (renderContext.flags[ENVIRONMENT_ENABLED])
		{ renderPasses[ENVIRONMENT_PASS]->frame(); }
	}

	if (renderContext.flags[HDR_PASS_ENABLED])
	{ renderPasses[HDR_PASS]->frame(); }

	if (renderContext.flags[DEBUG_PASS_ENABLED])
	{ renderPasses[DEBUG_PASS]->frame(); }
}

void PBRRenderer::buildBuffers()
{
	struct ShaderPointLight
	{
		glm::vec4 position; // X Y Z + padding
		glm::vec4 radiance; // R G B + padding
	};

	struct ShaderDirectionalLight
	{
		glm::vec4 direction; // X Y Z + padding
		glm::vec4 radiance; // R G B + padding
		std::array<glm::vec4, 5> lightSpaceMatrices; // 4 * 4 * 5 = 80 bytes
	};

	std::vector<ShaderPointLight> pointLights;
	std::vector<ShaderDirectionalLight> directionalLights;

	for (const auto& pointLight : renderContext.scene->scenePointLights)
	{
			pointLights.push_back(
				{
					glm::vec4(pointLight.position, 1.0f),
					glm::vec4(pointLight.colour * pointLight.strength, 0.0f)
				}
			);
	}

	for (const auto& directionalLight : renderContext.scene->sceneDirectionalLights)
	{
		directionalLights.push_back(
			{
				glm::vec4(directionalLight.direction, 0.0f),
				glm::vec4(directionalLight.colour * directionalLight.strength, 0.0f)
				// todo: calculate cascade light space matrices
			}
		);
	}

	// todo: add sun light here if necessary!

	//std::bitset<RenderFlags::NUM_FLAGS> flagBits;
	//for (int i = 0; i < RenderFlags::NUM_FLAGS; i++)
	//{
	//	flagBits[i] = renderContext.flags[i];
	//}

	uint32_t flagBits = 1;
	for (size_t i = 0; i < RenderFlags::NUM_FLAGS; i++)
	{
		flagBits |= (renderContext.flags[i] * (1 << i));
	}

	renderContext.buffers.bufferData("flags", sizeof(uint32_t), &flagBits);

	struct alignas(16) FrameUniforms
	{
		glm::mat4 projectionMatrix;
		glm::mat4 viewMatrix;
		glm::vec3 cameraPosition;
		float pad0;

		glm::vec4 directionalShadowCascadePlanes;

		int numPointLights;
		int numDirectionalLights;
	} frameUniforms = { };

	frameUniforms.projectionMatrix = glm::perspective(
		glm::radians(camera->getFov()),
		static_cast<float>(renderContext.dimensions.x) / static_cast<float>(renderContext.dimensions.y),
		renderContext.nearPlane, renderContext.farPlane);

	frameUniforms.viewMatrix = camera->getViewMatrix();
	frameUniforms.cameraPosition = camera->getEye();
	frameUniforms.directionalShadowCascadePlanes = { 0.05f, 0.1f, 0.25f, 0.5f }; // todo: add cascade shadow planes
	frameUniforms.numPointLights = static_cast<int>(pointLights.size());
	frameUniforms.numDirectionalLights = static_cast<int>(directionalLights.size());

	renderContext.buffers.bufferData("frame_uniforms", sizeof(FrameUniforms), &frameUniforms);
	renderContext.buffers.bufferData("point_lights", sizeof(ShaderPointLight) * pointLights.size(), pointLights.data());
	renderContext.buffers.bufferData("directional_lights", sizeof(ShaderDirectionalLight) * directionalLights.size(), directionalLights.data());
}
