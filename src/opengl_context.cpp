#include "opengl_context.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "profiler.h"

static const auto openglLogger = spdlog::stdout_color_mt("OpenGL");

// trace every gl call, and print errors when appropriate
void openglTraceAfter(const glbinding::FunctionCall& call)
{
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

        const auto error = gl::glGetError();
        if (error == gl::GLenum::GL_NO_ERROR) { }
        else
        {
                openglLogger->error(glbinding::aux::Meta::getString(error));
                openglLogger->error(functionCall);
        }
}

void GLAPIENTRY openglErrorCallback(gl::GLenum, gl::GLenum, gl::GLuint, gl::GLenum severity, gl::GLsizei,
                                    const gl::GLchar* message, const void*)
{
        switch (severity)
        {
        case gl::GL_DEBUG_SEVERITY_HIGH:
                spdlog::error("{}", message);
                break;
        case gl::GL_DEBUG_SEVERITY_MEDIUM:
                spdlog::warn("{}", message);
                break;
        default:
                spdlog::trace("{}", message);
                break;
        }
}

GLContext::GLContext(const Window& window) : windowRef(window)
{
        PROFILE_FUNCTION();

        glfwMakeContextCurrent(window.getWindowPtr().get());

        glbinding::initialize(glfwGetProcAddress);

        // Setup opengl trace callback
        glbinding::setCallbackMaskExcept(
            glbinding::CallbackMask::After | glbinding::CallbackMask::ParametersAndReturnValue, {"glGetError"});
        glbinding::setAfterCallback(openglTraceAfter);

        // Successfully loaded OpenGL
        spdlog::info("Loaded OpenGL {}", glbinding::aux::ContextInfo::version().toString());

        spdlog::info("Renderer: {}", glbinding::aux::ContextInfo::renderer());
        spdlog::info("Vendor: {}", glbinding::aux::ContextInfo::vendor());
        spdlog::info("GLSL Version: {}",
                     reinterpret_cast<const char*>(gl::glGetString(gl::GLenum::GL_SHADING_LANGUAGE_VERSION)));

        gl::glEnable(gl::GLenum::GL_DEBUG_OUTPUT);
        gl::glDebugMessageCallback(openglErrorCallback, nullptr);

        gl::glEnable(gl::GLenum::GL_DEPTH_TEST);
        gl::glDepthFunc(gl::GLenum::GL_LESS);

        gl::glEnable(gl::GLenum::GL_CULL_FACE);
        gl::glCullFace(gl::GLenum::GL_BACK);
}
