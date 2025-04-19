#pragma once

#include "camera.h"
#include "model.h"
#include "scene.h"
#include "shaderProgram.h"
#include "imguiWindows.h"

struct Light
{
	glm::vec3 position; // Position will act as direction if the light is directional
	glm::vec3 colour;
	float strength;

	enum LIGHT_TYPE {
		DIRECTIONAL = 0, 
		POINT
	} type;
};

class PBRRenderer
{
public:
	PBRRenderer(glm::ivec2 screenSize, std::shared_ptr<Camera> cameraPtr);
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

	void setCamera(std::shared_ptr<Camera> cameraPtr);

private:
	std::shared_ptr<Scene> scene;

	std::shared_ptr<Camera> viewCameraPtr;

private:
	const float lightCutoff = 1.0f / 1024.0f;

	glm::ivec2 dimensions;
	glm::mat4 projectionMatrix;

	std::vector<std::shared_ptr<RenderableModel>> primitiveModels;
	std::shared_ptr<MeshPrimitive> sphere;
	std::shared_ptr<MeshPrimitive> quad;
	std::shared_ptr<MeshPrimitive> cube;

	std::vector<int> jointOffsets;
	GLuint jointsBuffer;

	// --	  LIGHTS	-- 

	static constexpr int NUM_CASCADES = 5;

	struct ShaderLight // Information that will go to the shader
	{
		glm::vec3 position; // 12 bytes -- OR direction
		float radius; // 4 bytes - was padding now useful
		glm::vec3 radiance; // 12 bytes -- now HDR!
		int type; // 4 bytes
		std::array<glm::mat4, NUM_CASCADES> lightSpaceMatrices;
	};

	GLuint lightBuffer;

	int numLights;

	std::vector<ShaderLight> pointLights;
	std::vector<ShaderLight> directionalLights;

	glm::mat4 lightSpaceMatrix;

	// -----------------------

	// --	 ENVIRONMENT	--

	const int environmentMapDimensions = 1024;
	float environmentMapFactor;

	GLuint localCubemap;
	GLuint environmentMap;
	GLuint irradianceMap;
	GLuint prefilteredMap;
	GLuint brdfLUT;

	glm::vec3 sunDirection;
	glm::vec3 sunColour; // HDR !!!
	float sunFactor;
	 
	ShaderProgram skyboxShader;

	// -----------------------

	// --	   SHADOWS		--

	const float shadowNearPlane = 0.1f;
	const float shadowFarPlane = 1000.0f;
	
	int pointShadowMapDimensions;
	int directionalShadowMapDimensions;

	std::array<float, NUM_CASCADES> cascadeFarPlanes;
	std::array<float, NUM_CASCADES> cascadeRatios = { 1.0f / 20.0f, 1.0f / 10.0f, 1.0f / 5.0f, 1 / 2.0f, 1.0f };

	ShaderProgram pointShadowMapShader;
	ShaderProgram directionalShadowMapShader;

	GLuint directionalShadowMapArray;
	GLuint pointShadowCubemapArray;

	GLuint shadowMapFBO;

	// -----------------------

	// --	DEFERRED PASS	--

	ShaderProgram deferredPassShader;
	ShaderProgram gBufferShader;
	GLuint gBufferFBO;
	std::vector<GLuint> gBufferTextures;
	GLuint gBufferDepth;

	// -----------------------

	// --	FORWARD PASS	--

	ShaderProgram depthPrepassShader;
	ShaderProgram forwardPassShader;
	ShaderProgram unlitShader;

	// -----------------------

	//	--		 HDR		--

	ShaderProgram hdrPassShader;
	GLuint hdrPassFBO;
	GLuint hdrPassColour;
	GLuint hdrPassDepthRBO;

	// -----------------------

public:
	enum {
		NORMALS_ENABLED = 0,
		OCCLUSION_ENABLED,
		SHADOWS_ENABLED,
		ENVIRONMENT_MAP_ENABLED,
		EMULATE_SUN_ENABLED,
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
	const std::vector<glm::vec4> getFrustumCorners(const glm::mat4& pojectionViewMatrix) const;
	void generateProjectionMatrix();
	
	void renderPrimitive(const std::shared_ptr<MeshPrimitive> prim, ShaderProgram& program);

	void loadMaterialProperties(const std::vector<GLuint>& textures, const tinygltf::Material& materialDesc, ShaderProgram& shader);

private: // RENDER PASSES

	// --	 ENVIRONMENT	--

	void loadEnvironmentMap();
	void determineSunProperties(float* data, int width, int height, int channels);
	void prefilterEnvironmentMap();

	// --	   SHADOWS		--

	void loadLights();

	void initializeShadowMaps();
	void resizeShadowMaps();
	void buildShadowMaps();
	void buildPointShadowMaps();
	void buildDirectionalShadowMaps();
	void cleanShadowMaps();

	// --	DEFERRED PASS	--

	void initializeDeferredPass();
	void resizeDeferredPass();
	void buildGBuffer();
	void deferredPass();
	void cleanDeferredPass();

	// --	FORWARD PASS	--

	void initializeForwardPass();
	void resizeForwardPass();
	void forwardPass();
	void cleanForwardPass();

	//	--		 HDR		--

	void initializeHdrPass();
	void resizeHdrPass();
	void hdrPass();
	void cleanHdrPass();
};