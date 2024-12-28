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
	PBRRenderer(glm::ivec2 screenSize, Camera& camera);
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

	ShaderProgram pbrShader;
	ShaderProgram unlitShader;
	ShaderProgram depthCubemapShader;

	Camera& camera;

	std::vector<bool> flags = std::vector<bool>(NUM_FLAGS, true);

	float shadowNearPlane = 0.0001f;
	float shadowFarPlane = 100.0f;
	glm::ivec2 shadowMapDimensions = { 2048, 2048 };

	glm::ivec2 dimensions;
	glm::mat4 projectionMatrix;

	std::shared_ptr<Model> sphere;

	struct ShaderLight
	{
		glm::vec3 position; // 12 bytes
		float padding; // 4 bytes
		glm::vec3 colour; // 12 bytes
		float strength; // 4 bytes
	};

private:
	GLuint lightBuffer;

	std::vector<int> jointOffsets;
	GLuint jointsBuffer;

	GLuint depthMapFBO;

	GLuint depthCubemapArray;

public:
	enum {
		NORMALS_ENABLED,
		OCCLUSION_ENABLED,
		SHADOWS_ENABLED,
		NUM_FLAGS
	};

	void setFlag(const int flag, bool value);
	bool getFlag(const int flag) const { return flags[flag]; }

	void resize(glm::ivec2 screenSize);
	void setCamera(const Camera& camera);

private:
	void generateProjectionMatrix();
	
	void renderPrimitive(const MeshPrimitive& prim, ShaderProgram program);

	void loadMaterialProperties(const std::vector<GLuint>& textures, const tinygltf::Material& materialDesc);

	void renderShadowMaps(const std::vector<ShaderLight>& lights);

};