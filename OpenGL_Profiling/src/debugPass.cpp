#include "debugPass.h"

DebugPass::DebugPass(RenderContext& context)
	: RenderPass(context), sphere(RenderableModel::constructUnitSphere(8, 8))
{
	debugShaderProgram.addShader(GL_VERTEX_SHADER, "debug_pass/debug_pass.vert");
	debugShaderProgram.addShader(GL_FRAGMENT_SHADER, "debug_pass/debug_pass.frag");
}

void DebugPass::frame()
{
	debugShaderProgram.use();
	renderContext.buffers.bindBuffers(debugShaderProgram);

	struct alignas(16) InstanceTransform
	{
		glm::mat4 model;
		glm::mat3x4 normalMatrix;
	};

	std::vector<InstanceTransform> instanceTransforms;
	instanceTransforms.reserve(renderContext.scene->scenePointLights.size());

	for (const auto& l : renderContext.scene->scenePointLights)
	{
		InstanceTransform instance;
		instance.model = glm::translate(glm::mat4(1.0f), l.position);
		instance.model = glm::scale(instance.model, glm::vec3(0.1f));
		instance.normalMatrix = glm::transpose(glm::inverse(glm::mat3(instance.model)));
		instanceTransforms.push_back(instance);
	}

	renderContext.buffers.bufferData("instance", sizeof(InstanceTransform) * instanceTransforms.size(), instanceTransforms.data());

	const auto& spherePrim = sphere->getPrimitives()[0];

	glBindVertexArray(spherePrim->vertexArray);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, spherePrim->indicesBuffer);

	glDrawElementsInstanced(spherePrim->mode, static_cast<GLsizei>(spherePrim->count), 
		spherePrim->componentType, (void*)spherePrim->byteOffset, static_cast<GLsizei>(instanceTransforms.size()));

	glBindVertexArray(0);
}

void DebugPass::refresh()
{
}
