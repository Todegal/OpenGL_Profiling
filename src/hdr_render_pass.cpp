#include "hdr_render_pass.h"

HDRRenderPass::HDRRenderPass(GLContext& context, RenderContext& renderContext)
    : RenderPass(context, renderContext),
      hdrPassShader(
          glContext,
          {std::make_shared<GLShader>(context, "shaders/hdr_pass/hdr_pass.vert.glsl", gl::GLenum::GL_VERTEX_SHADER),
           std::make_shared<GLShader>(context, "shaders/hdr_pass/hdr_pass.frag.glsl", gl::GLenum::GL_FRAGMENT_SHADER)})
{
        const auto& screenDimensions = glContext.getWindow().getFramebufferSize();
        renderContext.globalTextures["hdr_colour_target"] =
            std::make_unique<GLTexture2D>(glContext, screenDimensions.x, screenDimensions.y, gl::GLenum::GL_RGB32F, 1);
}

void HDRRenderPass::frameStart()
{
        gl::glNamedFramebufferTexture(renderContext.framebuffer, gl::GLenum::GL_COLOR_ATTACHMENT0,
                                      renderContext.globalTextures.at("hdr_colour_target")->getID(), 0);
}

void HDRRenderPass::frameExecute()
{
        gl::glNamedFramebufferTexture(renderContext.framebuffer, gl::GLenum::GL_COLOR_ATTACHMENT0,
                                      renderContext.defaultColourTarget, 0);

        hdrPassShader.useProgram();

        renderContext.globalTextures.at("hdr_colour_target")->bindUnit(0);
        hdrPassShader.setUniformValue("uColourTexture", 0);

        const auto& screenDimensions = glContext.getWindow().getFramebufferSize();
        hdrPassShader.setUniformValue("uScreenDimensions", screenDimensions);


        renderContext.drawFullscreen();
}

void HDRRenderPass::refresh()
{
}
