#include "pbrRenderer.h"
#include "forwardRenderPass.h"
#include "hdrRenderPass.h"

#include <glm/gtc/matrix_transform.hpp>

PBRRenderer::PBRRenderer(glm::ivec2 screenSize, std::shared_ptr<Camera> camera)
	: renderContext()
{
	renderContext.nearPlane = 0.001f;
	renderContext.farPlane = 100.0f;
	renderContext.dimensions = screenSize;
	renderContext.scene = std::make_shared<Scene>();

	this->camera = camera;

	renderContext.flags[RenderFlags::NORMALS_ENABLED] = true;
	renderContext.flags[RenderFlags::OCCLUSION_ENABLED] = true;
	renderContext.flags[RenderFlags::SHADOWS_ENABLED] = false;
	renderContext.flags[RenderFlags::ENVIRONMENT_MAP_ENABLED] = false;
	renderContext.flags[RenderFlags::EMULATE_SUN_ENABLED] = false;
	renderContext.flags[RenderFlags::DEFERRED_PASS_ENABLED] = false;
	renderContext.flags[RenderFlags::HDR_PASS_ENABLED] = false;

	forwardPass = std::make_shared<ForwardRenderPass>(renderContext);
	hdrPass = std::make_shared<HDRRenderPass>(renderContext);	

	renderPasses.resize(NUM_PASSES);
	renderPasses[FORWARD_PASS] = forwardPass;
	renderPasses[HDR_PASS] = hdrPass;

	// create the required uniform buffers
	renderContext.buffers.addBuffer("flags", GL_UNIFORM_BUFFER, "FlagsBuffer");
	renderContext.buffers.addBuffer("frame_uniforms", GL_UNIFORM_BUFFER, "FrameUniformsBuffer");
	renderContext.buffers.addBuffer("point_lights", GL_SHADER_STORAGE_BUFFER, "PointLightBuffer");
	renderContext.buffers.addBuffer("directional_lights", GL_SHADER_STORAGE_BUFFER, "DirectionalLightBuffer");
	renderContext.buffers.addBuffer("joints", GL_SHADER_STORAGE_BUFFER, "JointsBuffer");
	renderContext.buffers.addBuffer("object", GL_UNIFORM_BUFFER, "ObjectBuffer");
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
}

void PBRRenderer::clearScene()
{
	renderContext.scene = std::make_shared<Scene>();
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

	ImGui::Text("NEW RENDERER!");

	ImGui::Separator();

	ImGui::Text("Flags");
	ImGui::Checkbox("HDR Pass Enabled", &renderContext.flags[RenderFlags::HDR_PASS_ENABLED]);

	ImGui::End();
}

void PBRRenderer::frame()
{
	{
		ScopedFramebufferBind framebufferBind(renderContext.framebufferStack,
			renderContext.flags[HDR_PASS_ENABLED] ? hdrPass->getFramebuffer() : 0);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glViewport(0, 0, renderContext.dimensions.x, renderContext.dimensions.y);

		buildBuffers();

		forwardPass->frame();
	}

	glViewport(0, 0, renderContext.dimensions.x, renderContext.dimensions.y);

	if (renderContext.flags[HDR_PASS_ENABLED])
	{ hdrPass->frame(); }
}

void PBRRenderer::buildBuffers()
{
	std::vector<PointLight> pointLights;
	std::vector<DirectionalLight> directionalLights;

	for (const auto& light : renderContext.scene->sceneLights)
	{
		if (light.type == Light::LIGHT_TYPE::POINT)
		{
			pointLights.push_back(
				{
					glm::vec4(light.position, 1.0f),
					glm::vec4(light.colour, light.strength)
				}
			);
		}
		else if (light.type == Light::LIGHT_TYPE::DIRECTIONAL)
		{
			directionalLights.push_back(
				{
					glm::vec4(light.position, 0.0f),
					glm::vec4(light.colour, light.strength)
					// todo: calculate cascade light space matrices
				}
			);
		}
	}

	std::array<uint32_t, RenderFlags::NUM_FLAGS> spacedFlags;
	for (int i = 0; i < RenderFlags::NUM_FLAGS; i++) spacedFlags[i] = static_cast<uint32_t>(renderContext.flags[i]);

	renderContext.buffers.bufferData("flags", sizeof(uint32_t) * RenderFlags::NUM_FLAGS, spacedFlags.data());

	FrameUniforms frameUniforms = { };

	frameUniforms.projectionMatrix = glm::perspective(
		glm::radians(camera->getFov()),
		static_cast<float>(renderContext.dimensions.x) / static_cast<float>(renderContext.dimensions.y),
		renderContext.nearPlane, renderContext.farPlane);

	frameUniforms.viewMatrix = camera->getViewMatrix();
	frameUniforms.cameraPosition = camera->getEye();
	frameUniforms.directionalShadowCascadePlanes = { 0.05f, 0.1f, 0.25f, 0.5f }; // todo: add cascade shadow planes
	frameUniforms.pointShadowNearPlane = renderContext.nearPlane;
	frameUniforms.pointShadowFarPlane = renderContext.farPlane;
	frameUniforms.numPointLights = static_cast<int>(pointLights.size());
	frameUniforms.numDirectionalLights = static_cast<int>(directionalLights.size());

	renderContext.buffers.bufferData("frame_uniforms", sizeof(FrameUniforms), &frameUniforms);
	renderContext.buffers.bufferData("point_lights", sizeof(PointLight) * pointLights.size(), pointLights.data());
	renderContext.buffers.bufferData("directional_lights", sizeof(DirectionalLight) * directionalLights.size(), directionalLights.data());
}
