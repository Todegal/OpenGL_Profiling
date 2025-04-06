#pragma once

#include "camera.h"
#include "model.h"
#include "scene.h"
#include "shaderProgram.h"
#include "imguiWindows.h"

struct PBRRenderFlags
{
	bool normalsEnabled = true;
	bool occlusionEnabled = true;

	bool shadowsEnabled = true;
};

struct Light
{
	glm::vec3 position; // Position will act as direction if the light is directional
	glm::vec3 colour;
	float strength;
};

class PBRRenderer
{
public:
	PBRRenderer(glm::ivec2 screenSize, const Camera& camera);
	~PBRRenderer();

	// No copy/move
	PBRRenderer(const PBRRenderer&) = delete;
	PBRRenderer& operator=(const PBRRenderer) = delete;

public:
	void frame();

	void loadScene(
		std::shared_ptr<Scene> scene
	);

	void clearScene();

private:
	std::shared_ptr<Scene> scene;

private: // SHADERS

	const float lightCutoff = 1.0f / 1024.0f;

	const Camera& camera;

	glm::ivec2 dimensions;
	glm::mat4 projectionMatrix;

	const std::shared_ptr<Model> sphere;
	const std::shared_ptr<Model> quad;
	const std::shared_ptr<Model> cube;

	struct ShaderLight
	{
		glm::vec3 position; // 12 bytes
		float radius; // 4 bytes - was padding now useful
		glm::vec3 colour; // 12 bytes
		float strength; // 4 bytes
	};

	ShaderProgram depthPrepassShader;
	ShaderProgram forwardPassShader;
	ShaderProgram unlitShader;

	GLuint lightBuffer;

	std::vector<int> jointOffsets;
	GLuint jointsBuffer;

	const int environmentMapDimensions = 1024;
	GLuint environmentMap;

	ShaderProgram backgroundShader;

	const float shadowNearPlane = 0.1f;
	const float shadowFarPlane = 10.0f;
	const int shadowMapDimensions = 2048;

	ShaderProgram shadowMapShader;
	GLuint shadowMapFBO;
	GLuint shadowCubemapArray;

	ShaderProgram deferredPassShader;
	ShaderProgram gBufferShader;
	GLuint gBufferFBO;
	std::vector<GLuint> gBufferTextures;
	GLuint gBufferDepth;

	ShaderProgram hdrPassShader;
	GLuint hdrPassFBO;
	GLuint hdrPassColour;
	GLuint hdrPassDepthRBO;

public:
	enum {
		NORMALS_ENABLED = 0,
		OCCLUSION_ENABLED,
		SHADOWS_ENABLED,
		ENVIRONMENT_MAP_ENABLED,
		DEPTH_PREPASS_ENABLED,
		DEFERRED_PASS_ENABLED,
		HDR_PASS_ENABLED,
		NUM_FLAGS
	};

private:
	std::array<bool, NUM_FLAGS> flags;

public:

	void setFlag(const int flag, bool value);
	bool getFlag(const int flag) const { return flags[flag]; }

	void drawFlagsDialog(imgui_data data);

	void resize(glm::ivec2 screenSize);

private:
	void generateProjectionMatrix();
	
	void renderPrimitive(const MeshPrimitive& prim, ShaderProgram& program);

	void loadMaterialProperties(const std::vector<GLuint>& textures, const tinygltf::Material& materialDesc, ShaderProgram& shader);

private: // RENDER PASSES

	void loadEnvironmentMap();


	void initializeShadowMaps();
	void resizeShadowMaps();
	void buildShadowMaps(const std::vector<ShaderLight>& lights);
	void cleanShadowMaps();


	void initializeDeferredPass();
	void resizeDeferredPass();
	void buildGBuffer();
	void deferredPass(const std::vector<ShaderLight>& lights);
	void cleanDeferredPass();


	void initializeForwardPass();
	void resizeForwardPass();
	void forwardPass(const std::vector<ShaderLight>& lights);
	void cleanForwardPass();


	void initializeHdrPass();
	void resizeHdrPass();
	void hdrPass();
	void cleanHdrPass();
};