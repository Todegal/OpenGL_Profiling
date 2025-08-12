#pragma once

#include "renderPass.h"
#include "environmentPass.h"
#include "forwardRenderPass.h"
#include "hdrPass.h"
#include "debugPass.h"
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
	RenderContext renderContext;

	std::shared_ptr<Camera> camera;

	enum : uint8_t
	{
		//DEFERRED_PASS,
		ENVIRONMENT_PASS = 0,
		SHADOW_PASS,
		FORWARD_PASS,
		HDR_PASS,
		DEBUG_PASS,
		NUM_PASSES
	};

	std::array<std::unique_ptr<RenderPass>, NUM_PASSES> renderPasses;
};