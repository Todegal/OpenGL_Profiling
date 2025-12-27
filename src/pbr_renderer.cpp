#include "pbr_renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

PBRRenderer::PBRRenderer(GLContext& context, const RawScene& initialScene, std::shared_ptr<Camera> initialCamera)
    : glContext(context), renderContext(context, initialScene, initialCamera)
{
        PROFILE_FUNCTION();

        // define uniform buffers
        renderContext.globalBuffers["ObjectBuffer"] = std::make_unique<GLBuffer>(
            glContext, sizeof(RenderContext::ObjectMatrices), gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);

        renderContext.globalBuffers["FrameUniformsBuffer"] = std::make_unique<GLBuffer>(
            glContext, sizeof(RenderContext::FrameUniforms), gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);

        // setup lights
        renderContext.pointLights.resize(4);

        renderContext.pointLights[0].radiance = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        renderContext.pointLights[0].position = glm::vec4(2.0f, 1.0f, 0.0f, 0.0f);

        renderContext.pointLights[1].radiance = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        renderContext.pointLights[1].position = glm::vec4(0.0f, 3.0f, 0.0f, 0.0f);

        renderContext.pointLights[2].radiance = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        renderContext.pointLights[2].position = glm::vec4(0.0f, 1.0f, 2.0f, 0.0f);

        renderContext.pointLights[3].radiance = glm::vec4(5.0f, 5.0f, 5.0f, 0.0f);
        renderContext.pointLights[3].position = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);

        renderContext.globalBuffers["PointLightBuffer"] = std::make_unique<GLBuffer>(
            glContext, sizeof(renderContext.pointLights[0]) * renderContext.pointLights.size(),
            gl::BufferStorageMask::GL_DYNAMIC_STORAGE_BIT);

        renderContext.globalBuffers.at("PointLightBuffer")
            ->subData<RenderContext::PointLight>(0, renderContext.pointLights);

        // declare passes
        forwardPass = std::make_unique<ForwardRenderPass>(context, renderContext);
        hdrPass = std::make_unique<HDRRenderPass>(context, renderContext);
}

void PBRRenderer::frame()
{
        PROFILE_FUNCTION();

        const auto& screenDimensions = glContext.getWindow().getFramebufferSize();

        renderContext.frameUniforms.projectionMatrix = glm::perspective(
            glm::radians(renderContext.camera->getFov()),
            static_cast<float>(screenDimensions.x) / static_cast<float>(screenDimensions.y), 0.01f, 100000.0f);
        renderContext.frameUniforms.viewMatrix = renderContext.camera->getViewMatrix();
        renderContext.frameUniforms.cameraPosition = renderContext.camera->getEye();
        renderContext.frameUniforms.numPointLights = static_cast<int>(renderContext.pointLights.size());

        renderContext.globalBuffers.at("FrameUniformsBuffer")
            ->subData<RenderContext::FrameUniforms>(0, std::span(&renderContext.frameUniforms, 1));

        renderContext.framebuffer->bindDraw();

        forwardPass->frameStart();
        hdrPass->frameStart();

        const static auto clearColour = glm::vec4(glm::vec3(0.0f), 1.0f);
        const static auto clearDepth = 1.0f;

        glContext.enable(gl::GLenum::GL_CULL_FACE);
        glContext.enable(gl::GLenum::GL_DEPTH_TEST);

        gl::glViewport(0, 0, screenDimensions.x, screenDimensions.y);

        renderContext.framebuffer->clearBuffer(gl::GLenum::GL_COLOR, 0, glm::value_ptr(clearColour));
        renderContext.framebuffer->clearBuffer(gl::GLenum::GL_DEPTH, 0, &clearDepth);

        forwardPass->frameExecute();
        hdrPass->frameExecute();

        // present to default framebuffer todo: fix this do better please
        renderContext.framebuffer->blitToScreen(screenDimensions.x, screenDimensions.y);

        forwardPass->frameEnd();
        hdrPass->frameEnd();

        gl::glBindFramebuffer(gl::GL_FRAMEBUFFER, 0);
}