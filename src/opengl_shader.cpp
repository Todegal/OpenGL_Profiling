#include "opengl_shader.h"

#include <glbinding/gl/boolean.h>
#include <glbinding/gl/enum.h>

#include <spdlog/spdlog.h>

// #define STB_INCLUDE_IMPLEMENTATION
// #define STB_INCLUDE_LINE_GLSL
//
// #include <stb_include.h>
//
// #undef STB_INCLUDE_IMPLEMENTATION

#include <filesystem>
#include <format>
#include <fstream>

GLShader::GLShader(const GLContext&, const std::string_view shaderPath, const gl::GLenum stage)
    : absoluteShaderPath(std::filesystem::absolute(shaderPath)), stage(stage)
{
        shaderId = gl::glCreateShader(stage);

        if (!std::filesystem::exists(absoluteShaderPath))
        {
                throw std::runtime_error(std::format("Failed to load shader: {}", shaderPath));
        }

        const std::ifstream fileStream(absoluteShaderPath.string());
        std::stringstream stringStream;
        stringStream << fileStream.rdbuf();

        const std::string sourceString = stringStream.str();
        const char* sourcePointer = sourceString.c_str();

        gl::glShaderSource(shaderId, 1, &sourcePointer, nullptr);

        gl::glCompileShader(shaderId);

        gl::GLboolean compileStatus;
        gl::glGetShaderiv(shaderId, gl::GL_COMPILE_STATUS, &compileStatus);

        gl::GLsizei compileLogLength;
        std::string compileLog;
        gl::glGetShaderiv(shaderId, gl::GL_INFO_LOG_LENGTH, &compileLogLength);

        if (compileLogLength > 0)
        {
                std::vector<char> compileLogBuffer(compileLogLength);
                gl::glGetShaderInfoLog(shaderId, compileLogLength, nullptr, compileLogBuffer.data());

                compileLog = std::string(compileLogBuffer.data());
        }

        if (compileStatus != gl::GL_TRUE)
        {
                throw std::runtime_error(std::format("Failed to compile shader{}, {}", shaderPath, compileLog));
        }
        else
        {
                // Log it as a trace
                spdlog::trace("Compiled shader: {}; Log: \n{}\n", shaderPath, compileLog);
        }
}

GLShader::~GLShader()
{
        gl::glDeleteShader(shaderId);
}

GLShaderProgram::GLShaderProgram(const GLContext&, const std::vector<std::shared_ptr<GLShader>> shaders)
{
        programId = gl::glCreateProgram();

        for (const auto& shader : shaders)
        {
                gl::glAttachShader(programId, shader->getId());
        }

        gl::glLinkProgram(programId);

        gl::GLboolean linkStatus;
        gl::glGetProgramiv(programId, gl::GL_LINK_STATUS, &linkStatus);

        gl::GLsizei linkLogLength;
        std::string linkLog;
        gl::glGetProgramiv(programId, gl::GL_INFO_LOG_LENGTH, &linkLogLength);

        if (linkLogLength > 0)
        {
                std::vector<char> linkLogBuffer(linkLogLength);
                gl::glGetProgramInfoLog(programId, linkLogLength, nullptr, linkLogBuffer.data());

                linkLog = std::string(linkLogBuffer.data());
        }

        if (linkStatus != gl::GL_TRUE) { throw std::runtime_error(std::format("Failed to link program: {}", linkLog)); }
        else { spdlog::trace("Linked program; Log: {}", linkLog); }
}

GLShaderProgram::~GLShaderProgram()
{
        gl::glDeleteProgram(programId);
}
