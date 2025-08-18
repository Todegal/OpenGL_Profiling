#include "opengl_context.h"

#include <glbinding/gl/functions.h>
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
                openglLogger->flush();
                break;
        case gl::GLenum::GL_DEBUG_SEVERITY_MEDIUM:
                openglLogger->warn("{}", message);
                openglLogger->flush();
                break;
        default:
                openglLogger->trace("{}", message);
                openglLogger->flush();
                break;
        }
}

void debugLogCallback(const glbinding::FunctionCall& call)
{
        if (gl::glGetError() == gl::GLenum::GL_NO_ERROR) { return; }

        std::ostringstream oss;
        oss << call.function->name() << "(";

        for (size_t i = 0; i < call.parameters.size(); ++i)
        {
                oss << call.parameters[i].get();
                if (i < call.parameters.size() - 1) { oss << ", "; }
        }

        oss << ")";
        if (call.returnValue.get()) { oss << " -> " << call.returnValue.get(); }

        const std::string functionCall = oss.str();

        openglLogger->error(functionCall);
}

GLContext::GLContext(const Window& window) : windowRef(window)
{
        PROFILE_FUNCTION();

        glfwMakeContextCurrent(window.getWindowPtr().get());
        glfwSwapInterval(0);

        glbinding::initialize(glfwGetProcAddress);

#ifndef NDEBUG
        glbinding::setCallbackMaskExcept(glbinding::CallbackMask::After |
                                             glbinding::CallbackMask::ParametersAndReturnValue,
                                         {"glGetError"});

        glbinding::setAfterCallback(debugLogCallback);
#endif

        // Successfully loaded OpenGL
        spdlog::info("Loaded OpenGL {}", glbinding::aux::ContextInfo::version().toString());

        spdlog::info("Renderer: {}", glbinding::aux::ContextInfo::renderer());
        spdlog::info("Vendor: {}", glbinding::aux::ContextInfo::vendor());
        spdlog::info("GLSL Version: {}",
                     reinterpret_cast<const char*>(gl::glGetString(gl::GLenum::GL_SHADING_LANGUAGE_VERSION)));

#ifndef NDEBUG
        gl::glEnable(gl::GLenum::GL_DEBUG_OUTPUT);
        gl::glEnable(gl::GLenum::GL_DEBUG_OUTPUT_SYNCHRONOUS);
        gl::glDebugMessageCallback(openglErrorCallback, nullptr);
#endif

        gl::glEnable(gl::GLenum::GL_DEPTH_TEST);
        gl::glDepthFunc(gl::GLenum::GL_LEQUAL);

        //gl::glEnable(gl::GLenum::GL_CULL_FACE);
        //gl::glCullFace(gl::GLenum::GL_BACK);

	gl::glPixelStorei(gl::GLenum::GL_UNPACK_ALIGNMENT, 1);
	gl::glPixelStorei(gl::GLenum::GL_UNPACK_ALIGNMENT, 1);
}
