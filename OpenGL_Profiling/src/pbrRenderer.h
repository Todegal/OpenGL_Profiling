#pragma once

#include "renderPass.h"
#include "forwardRenderPass.h"
#include "hdrRenderPass.h"
#include "camera.h"
#include "imguiWindows.h"

class PBRRenderer
{
public:
	PBRRenderer(glm::ivec2 screenSize, std::shared_ptr<Camera> camera);
	~PBRRenderer();

	// No copy/move
	PBRRenderer(const PBRRenderer&) = delete;
	PBRRenderer& operator=(const PBRRenderer) = delete;

public:
	void loadScene(std::shared_ptr<Scene> scene);
	void clearScene();

	void setCamera(std::shared_ptr<Camera> camera);

	void resize(glm::ivec2 screenSize);

	void imguiFrame(imgui_data& data);
	void frame();

private:
	void buildBuffers();

private:
	struct PointLight
	{
		glm::vec4 position; // X Y Z + padding
		glm::vec4 radiance; // R G B + padding
	};

	struct DirectionalLight
	{
		glm::vec4 direction; // X Y Z + padding
		glm::vec4 radiance; // R G B + padding
		std::array<glm::vec4, NUM_CASCADES> lightSpaceMatrices; // 4 * 4 * 5 = 80 bytes
	};

	struct alignas(16) FrameUniforms
	{
		glm::mat4 projectionMatrix;
		glm::mat4 viewMatrix;
		glm::vec3 cameraPosition;
		float pad0;

		glm::vec4 directionalShadowCascadePlanes;

		float pointShadowNearPlane;
		float pointShadowFarPlane;

		int numPointLights;
		int numDirectionalLights;
	};

private:
	RenderContext renderContext;

	std::shared_ptr<Camera> camera;

	enum : uint8_t
	{
		//ENVIRONMENT_PASS = 0,
		//SHADOW_PASS,
		//DEFERRED_PASS,
		FORWARD_PASS = 0,
		HDR_PASS,
		NUM_PASSES
	};

	std::shared_ptr<HDRRenderPass> hdrPass;
	std::shared_ptr<ForwardRenderPass> forwardPass;

	std::vector<std::shared_ptr<RenderPass>> renderPasses;
};