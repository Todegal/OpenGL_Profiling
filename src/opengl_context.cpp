#include "opengl_context.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "profiler.h"

static const auto openglLogger = spdlog::stdout_color_mt("OpenGL");

void GLAPIENTRY openglErrorCallback(gl::GLenum, gl::GLenum, gl::GLuint, gl::GLenum severity, gl::GLsizei,
                                    const gl::GLchar* message, const void*)
{
        switch (severity)
        {
        case gl::GLenum::GL_DEBUG_SEVERITY_HIGH:
                openglLogger->error("{}", message);
                break;
        case gl::GLenum::GL_DEBUG_SEVERITY_MEDIUM:
                openglLogger->warn("{}", message);
                break;
        default:
                openglLogger->trace("{}", message);
                break;
        }
}

GLContext::GLContext(const Window& window) : windowRef(window)
{
        PROFILE_FUNCTION();

        glfwMakeContextCurrent(window.getWindowPtr().get());
        glfwSwapInterval(0);

        glbinding::initialize(glfwGetProcAddress);

        // Successfully loaded OpenGL
        spdlog::info("Loaded OpenGL {}", glbinding::aux::ContextInfo::version().toString());

        spdlog::info("Renderer: {}", glbinding::aux::ContextInfo::renderer());
        spdlog::info("Vendor: {}", glbinding::aux::ContextInfo::vendor());
        spdlog::info("GLSL Version: {}",
                     reinterpret_cast<const char*>(gl::glGetString(gl::GLenum::GL_SHADING_LANGUAGE_VERSION)));

#ifndef NDEBUG
        gl::glEnable(gl::GLenum::GL_DEBUG_OUTPUT);
        gl::glDebugMessageCallback(openglErrorCallback, nullptr);
#endif

        gl::glEnable(gl::GLenum::GL_DEPTH_TEST);
        gl::glDepthFunc(gl::GLenum::GL_LESS);

        gl::glEnable(gl::GLenum::GL_CULL_FACE);
        gl::glCullFace(gl::GLenum::GL_BACK);
}
