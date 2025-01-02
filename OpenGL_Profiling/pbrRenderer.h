#pragma once

#include "camera.h"
#include "model.h"
#include "scene.h"
#include "shaderProgram.h"

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

	ShaderProgram forwardPassShader;

	ShaderProgram unlitShader;
	ShaderProgram depthCubemapShader;
	ShaderProgram depthPrepassShader;

	ShaderProgram deferredPassShader;
	ShaderProgram gBufferShader;

	ShaderProgram hdrPassShader;

	float lightCutoff = 1.0f / 1024.0f;

	const Camera& camera;

	std::vector<bool> flags = std::vector<bool>(NUM_FLAGS, true);

	float shadowNearPlane = 0.01f;
	float shadowFarPlane = 100.0f;
	glm::ivec2 shadowMapDimensions = { 2048, 2048 };

	glm::ivec2 dimensions;
	glm::mat4 projectionMatrix;

	std::shared_ptr<Model> sphere;
	std::shared_ptr<Model> quad;
	std::shared_ptr<Model> cube;

	struct ShaderLight
	{
		glm::vec3 position; // 12 bytes
		float radius; // 4 bytes - was padding now useful
		glm::vec3 colour; // 12 bytes
		float strength; // 4 bytes
	};

	GLuint lightBuffer;

	std::vector<int> jointOffsets;
	GLuint jointsBuffer;

	GLuint shadowMapFBO;
	GLuint shadowCubemapArray;

	GLuint gBufferFBO;
	std::vector<GLuint> gBufferTextures;
	GLuint gBufferDepth;

	GLuint hdrPassFBO;
	GLuint hdrPassColour;
	GLuint hdrPassDepthRBO;

public:
	enum {
		NORMALS_ENABLED,
		OCCLUSION_ENABLED,
		SHADOWS_ENABLED,
		DEPTH_PREPASS_ENABLED,
		DEFERRED_PASS_ENABLED,
		HDR_PASS_ENABLED,
		NUM_FLAGS
	};

	void setFlag(const int flag, bool value);
	bool getFlag(const int flag) const { return flags[flag]; }

	void resize(glm::ivec2 screenSize);

private:
	void generateProjectionMatrix();
	
	void renderPrimitive(const MeshPrimitive& prim, ShaderProgram program);

	void loadMaterialProperties(const std::vector<GLuint>& textures, const tinygltf::Material& materialDesc, ShaderProgram shader);

	void renderShadowMaps(const std::vector<ShaderLight>& lights);

	// Deferred Pass
	void initializeDeferredPass();
	void resizeDeferredPass();
	void buildGBuffer();
	void deferredPass(const std::vector<ShaderLight>& lights);


	void initializeForwardPass();
	void resizeForwardPass();
	void forwardPass(const std::vector<ShaderLight>& lights);


	void initializeHdrPass();
	void resizeHdrPass();
	void hdrPass();
};