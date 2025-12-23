#include "opengl_shader.h"

#include "profiler.h"
#include <glbinding/gl/boolean.h>
#include <glbinding/gl/enum.h>

#include <glbinding/gl/functions-patches.h>
#include <glbinding/gl/functions.h>
#include <glbinding/gl/types.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <stdexcept>
#include <unordered_set>
#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL

#include <stb_include.h>

#undef STB_INCLUDE_IMPLEMENTATION

#include <filesystem>
#include <format>
// #include <fstream>

GLShader::GLShader(const GLContext&, const std::string_view shaderPath, const gl::GLenum stage)
    : absoluteShaderPath(std::filesystem::absolute(shaderPath)), stage(stage)
{
        PROFILE_FUNCTION();

        shaderId = gl::glCreateShader(stage);

        absoluteShaderPath = std::filesystem::absolute(shaderPath);
        if (!std::filesystem::exists(absoluteShaderPath))
        {
                throw std::runtime_error(std::format("Failed to load shader: {}", shaderPath));
        }

        const std::string fileString = absoluteShaderPath.string();
        const std::string dirString = absoluteShaderPath.parent_path().string();

        // const std::ifstream fileStream(absoluteShaderPath.string());
        // std::stringstream stringStream;
        // stringStream << fileStream.rdbuf();
        //
        // const std::string sourceString = stringStream.str();
        char* filepath = const_cast<char*>(fileString.c_str());
        char* dirpath = const_cast<char*>(dirString.c_str());

        char errorBuf[256];
        char* sourcePointer = stb_include_file(filepath, nullptr, dirpath, errorBuf);

        if (!sourcePointer)
        {
                throw std::runtime_error(
                    std::format("Failed to load shader: {}, stb_include_file error: {}", shaderPath, errorBuf));
        }

        gl::glShaderSource(shaderId, 1, &sourcePointer, nullptr);

        free(sourcePointer);

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
                gl::glAttachShader(programId, shader->shaderId);
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

        uniformVariableNames = getResourceNames(gl::GLenum::GL_UNIFORM);
        uniformBlockNames = getResourceNames(gl::GLenum::GL_UNIFORM_BLOCK);
        shaderStorageBlockNames = getResourceNames(gl::GLenum::GL_SHADER_STORAGE_BLOCK);

        spdlog::debug("Program Uniform Variables:");
        for (const auto& name : uniformVariableNames)
        {
                spdlog::debug("\t{}", name);
        }

        spdlog::debug("Program Uniform Blocks:");
        for (const auto& name : uniformBlockNames)
        {
                spdlog::debug("\t{}", name);
        }

        spdlog::debug("Program Shader Storage Blocks:");
        for (const auto& name : shaderStorageBlockNames)
        {
                spdlog::debug("\t{}", name);
        }
}

GLShaderProgram::~GLShaderProgram()
{
        gl::glDeleteProgram(programId);
}

void GLShaderProgram::setUniformValue(const std::string& name, const glm::vec4& value)
{
        if (!uniformVariableNames.contains(name))
        {
                throw std::runtime_error(std::format("Uniform {} not found!", name));
        }

        gl::glUniform4fv(gl::glGetUniformLocation(programId, name.data()), 1, glm::value_ptr(value));
}

void GLShaderProgram::setUniformValue(const std::string& name, const int value)
{
        if (!uniformVariableNames.contains(name))
        {
                throw std::runtime_error(std::format("Uniform {} not found!", name));
        }

        gl::glUniform1i(gl::glGetUniformLocation(programId, name.data()), value);
}

void GLShaderProgram::setUniformBlockBinding(const std::string& name, const gl::GLuint binding)
{
        if (!uniformBlockNames.contains(name))
        {
                throw std::runtime_error(std::format("Uniform block {} not found!", name));
        }

        gl::GLuint blockIndex = gl::glGetUniformBlockIndex(programId, name.data());
        gl::glUniformBlockBinding(programId, blockIndex, binding);
}

void GLShaderProgram::setShaderStorageBlockBinding(const std::string& name, const gl::GLuint binding)
{
        if (!shaderStorageBlockNames.contains(name))
        {
                throw std::runtime_error(std::format("Shader storage block {} not found!", name));
        }

        gl::GLuint blockIndex =
            gl::glGetProgramResourceIndex(programId, gl::GLenum::GL_SHADER_STORAGE_BLOCK, name.data());
        gl::glShaderStorageBlockBinding(programId, blockIndex, binding);
}

std::unordered_set<std::string> GLShaderProgram::getResourceNames(gl::GLenum resourceInterface)
{
        std::unordered_set<std::string> names;

        gl::GLint numResources{};
        gl::glGetProgramInterfaceiv(programId, resourceInterface, gl::GLenum::GL_ACTIVE_RESOURCES, &numResources);

        names.reserve(numResources);

        gl::GLint maxNameLength{};
        gl::glGetProgramInterfaceiv(programId, resourceInterface, gl::GLenum::GL_MAX_NAME_LENGTH, &maxNameLength);

        std::vector<gl::GLchar> nameBuffer(maxNameLength);
        for (gl::GLint i = 0; i < numResources; ++i)
        {
                gl::GLsizei actualLength{};
                gl::glGetProgramResourceName(programId, resourceInterface, i,
                                             static_cast<gl::GLsizei>(nameBuffer.size()), &actualLength,
                                             nameBuffer.data());

                names.emplace(nameBuffer.data(), actualLength);
        }

        return names;
}
