#include "forward_render_pass.h"

ForwardRenderPass::ForwardRenderPass(GLContext& context, RenderContext& frameDesc)
    : RenderPass(context, frameDesc),
      forwardPassShader(glContext, {std::make_shared<GLShader>(glContext, "shaders/forward_pass/forward_pass.vert.glsl",
                                                               gl::GLenum::GL_VERTEX_SHADER),
                                    std::make_shared<GLShader>(glContext, "shaders/forward_pass/forward_pass.frag.glsl",
                                                               gl::GLenum::GL_FRAGMENT_SHADER)})
{
        PROFILE_FUNCTION();
        const auto& objectMatricesBuffer = renderContext.globalBuffers.at("ObjectBuffer");
        objectMatricesBuffer->bindBase(gl::GLenum::GL_UNIFORM_BUFFER, 0);
        forwardPassShader.setUniformBlockBinding("ObjectBuffer", 0);

        const auto& frameUniformsBuffer = renderContext.globalBuffers.at("FrameUniformsBuffer");
        frameUniformsBuffer->bindBase(gl::GLenum::GL_UNIFORM_BUFFER, 1);
        forwardPassShader.setUniformBlockBinding("FrameUniformsBuffer", 1);

        const auto& pointLightBuffer = renderContext.globalBuffers.at("PointLightBuffer");
        pointLightBuffer->bindBase(gl::GLenum::GL_SHADER_STORAGE_BUFFER, 0);
        forwardPassShader.setShaderStorageBlockBinding("PointLightBuffer", 0);
}

void ForwardRenderPass::frameStart()
{
        PROFILE_FUNCTION();
}

void ForwardRenderPass::frameExecute()
{
        PROFILE_FUNCTION();

        if (!renderContext.renderFlags.get<RenderFlags::FORWARD_PASS_ENABLED>()) { return; }

        renderContext.drawScene(forwardPassShader);
}

void ForwardRenderPass::frameEnd()
{
        PROFILE_FUNCTION();
}

void ForwardRenderPass::refresh()
{
}
