#pragma once

#include "camera.h"
#include "scene.h"
#include "shaderProgram.h"

#include <stack>

constexpr inline uint8_t NUM_CASCADES = 5;

enum RenderFlags : uint32_t {
	NORMALS_ENABLED = 0,
	OCCLUSION_ENABLED,
	SHADOWS_ENABLED,
	ENVIRONMENT_MAP_ENABLED,
	EMULATE_SUN_ENABLED,
	DEFERRED_PASS_ENABLED,
	HDR_PASS_ENABLED,
	NUM_FLAGS
};

class ShaderBufferManager
{
private:
	struct ShaderBuffer
	{
		GLuint id; // buffer
		GLenum target; 
		std::string blockName; // name IN SHADER
	};

	std::unordered_map<std::string, ShaderBuffer> buffers;

public:
	ShaderBufferManager()
		: buffers() { }
	~ShaderBufferManager();

	ShaderBufferManager(const ShaderBufferManager&) = delete;
	ShaderBufferManager& operator=(const ShaderBufferManager&) = delete;

	void addBuffer(const std::string& name, const GLenum target, const std::string& blockName);
	void bindBuffers(ShaderProgram& program) const;
	void bufferData(const std::string& name, size_t size, const void* data);
};


class FramebufferStack
{
public:
	FramebufferStack() : framebufferStack() {}

	FramebufferStack(const FramebufferStack&) = delete;
	FramebufferStack& operator=(const FramebufferStack&) = delete;

private:
	std::stack<GLuint> framebufferStack;

	void push(GLuint framebuffer);
	GLuint pop();

	GLuint current() const { return framebufferStack.top(); }

	friend class ScopedFramebufferBind;
};

class ScopedFramebufferBind
{
public:
	ScopedFramebufferBind(FramebufferStack& framebufferStack, GLuint framebuffer);
	~ScopedFramebufferBind();

	// Delete copy constructor and copy assignment operator
	ScopedFramebufferBind(const ScopedFramebufferBind&) = delete;
	ScopedFramebufferBind& operator=(const ScopedFramebufferBind&) = delete;

private:
	FramebufferStack& framebufferStack;
};

struct RenderContext
{
	std::array<bool, RenderFlags::NUM_FLAGS> flags;
	glm::ivec2 dimensions;
	glm::mat4 projectionMatrix;
	float nearPlane, farPlane;

	std::map<std::string, GLuint> textures;
	ShaderBufferManager buffers;

	std::shared_ptr<Scene> scene;

	FramebufferStack framebufferStack;

	RenderContext() : 
		flags(), 
		dimensions(), 
		projectionMatrix(), 
		nearPlane(), farPlane(),
		textures(), 
		buffers(),
		scene(nullptr),
		framebufferStack()
	{ }
};

class RenderPass
{
public:
	RenderPass(RenderContext& renderContext) : renderContext(renderContext) { };

protected:
	RenderContext& renderContext;

protected:
	void loadJoints(const std::shared_ptr<RenderableModel>& model);
	
	void renderPrimitive(const std::shared_ptr<MeshPrimitive>& prim);

	void parseMaterialProperties(const std::vector<GLuint>& textures, const tinygltf::Material& materialDesc);

public:
	virtual void frame() = 0;

public:
	virtual void refresh() = 0;
};