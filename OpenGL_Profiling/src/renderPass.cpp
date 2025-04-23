#include "renderPass.h"

ShaderBufferManager::~ShaderBufferManager()
{
	for (const auto& [name, buffer] : buffers)
	{
		glDeleteBuffers(1, &buffer.id);
	}
}

void ShaderBufferManager::addBuffer(const std::string& name, const GLenum target, const std::string& blockName)
{
	if (target != GL_SHADER_STORAGE_BUFFER && target != GL_UNIFORM_BUFFER)
	{
		throw std::invalid_argument("Invalid type!");
	}

	GLuint buffer;
	glGenBuffers(1, &buffer);
	glBindBuffer(target, buffer);
	glBufferData(target, 0, nullptr, GL_DYNAMIC_DRAW);

	ShaderBuffer shaderBuffer{ buffer, target, blockName };

	buffers[name] = shaderBuffer;
}

void ShaderBufferManager::bindBuffers(ShaderProgram& program) const
{
	int bindingIndex = 0;
	for (const auto& [name, buffer] : buffers)
	{
		glBindBuffer(buffer.target, buffer.id);
		glBindBufferBase(buffer.target, bindingIndex, buffer.id);

		GLuint blockIndex = 0;
		if (buffer.target == GL_UNIFORM_BUFFER)
		{
			blockIndex = glGetUniformBlockIndex(program.getProgramId(), buffer.blockName.c_str());
			if (blockIndex == GL_INVALID_INDEX) throw std::runtime_error("Invalid block name!");

			glUniformBlockBinding(program.getProgramId(), blockIndex, bindingIndex);
		}
		else
		{
			blockIndex = glGetProgramResourceIndex(program.getProgramId(), GL_SHADER_STORAGE_BLOCK, buffer.blockName.c_str());
			if (blockIndex == GL_INVALID_INDEX) throw std::runtime_error("Invalid block name!");

			glShaderStorageBlockBinding(program.getProgramId(), blockIndex, bindingIndex);
		}

		bindingIndex++;
	}
}

void ShaderBufferManager::bufferData(const std::string& name, size_t size, const void* data)
{
	const auto& buffer = buffers.at(name);

	glBindBuffer(buffer.target, buffer.id);

	int bufferSize;
	glGetBufferParameteriv(buffer.target, GL_BUFFER_SIZE, &bufferSize);

	if (bufferSize >= size)
	{
		glBufferSubData(buffer.target, 0, size, data);
	}
	else
	{
		glBufferData(buffer.target, size, data, GL_DYNAMIC_DRAW);
	}
}

void FramebufferStack::push(GLuint framebuffer)
{
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw std::runtime_error("Incomplete Framebuffer!");
	}

	framebufferStack.push(framebuffer);
}

GLuint FramebufferStack::pop()
{
	framebufferStack.pop();

	GLuint fb = 0;
	if (!framebufferStack.empty())
	{
		fb = framebufferStack.top();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fb);

	return fb;
}

ScopedFramebufferBind::ScopedFramebufferBind(FramebufferStack& framebufferStack, GLuint framebuffer)
	: framebufferStack(framebufferStack)
{
	this->framebufferStack.push(framebuffer);
}

ScopedFramebufferBind::~ScopedFramebufferBind()
{
	framebufferStack.pop();
}

void RenderPass::loadJoints(const std::shared_ptr<RenderableModel>& model)
{
	std::vector<glm::mat4> jointMatrices;
	for (const auto& t : model->getJoints())
	{
		auto w = model->getNodes()[t.nodeIdx]->getWorldTransform();
		w[3] -= glm::vec4(model->getTransform()->getWorldPosition(), 0.0f);

		auto iB = t.inverseBindMatrix;

		jointMatrices.push_back(w * iB);
	}

	renderContext.buffers.bufferData("joints", sizeof(glm::mat4) * jointMatrices.size(), jointMatrices.data());
}

void RenderPass::renderPrimitive(const std::shared_ptr<MeshPrimitive>& prim)
{
	struct alignas(16) ObjectUniforms
	{
		glm::mat4 model;
		glm::mat3x4 normalMatrix;
	} objectUniforms;

	objectUniforms.model = prim->transform->getWorldTransform();
	objectUniforms.normalMatrix = glm::transpose(glm::inverse(glm::mat3(objectUniforms.model)));

	renderContext.buffers.bufferData("object", sizeof(ObjectUniforms), &objectUniforms);

	glBindVertexArray(prim->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prim->indicesBuffer);

	glDrawElements(prim->mode, static_cast<GLsizei>(prim->count), prim->componentType, (void*)prim->byteOffset);

	glBindVertexArray(0);
}

void RenderPass::parseMaterialProperties(const std::vector<GLuint>& textures, const tinygltf::Material& materialDesc)
{

}


