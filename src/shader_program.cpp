#include "shader_program.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

ShaderProgram::ShaderProgram(const GLContext&, const std::filesystem::path vertexShader,
                             const std::filesystem::path fragmentShader)
    : isLinked(false)
{
        addShader(gl::GLenum::GL_VERTEX_SHADER, vertexShader);
        addShader(gl::GLenum::GL_FRAGMENT_SHADER, fragmentShader);

        programId = gl::glCreateProgram();
}

ShaderProgram::ShaderProgram() : isLinked(false)
{
        programId = gl::glCreateProgram();
}

ShaderProgram::~ShaderProgram()
{
        for (const auto& shader : shaders)
        {
                gl::glDeleteShader(shader.id);
        }

        gl::glDeleteProgram(programId);
}

void ShaderProgram::addShader(gl::GLenum stage, const std::filesystem::path shaderPath)
{
        if (isLinked) { isLinked = false; }

        gl::GLuint shaderId = glCreateShader(stage);

        if (!compileShader(shaderId, shaderPath))
        {
                gl::glDeleteShader(shaderId);
                return;
        }

        shaders.push_back({shaderId, shaderPath});
}

bool ShaderProgram::compileShader(gl::GLuint shader, const std::filesystem::path shaderPath)
{
        spdlog::trace("Loading shader file: {}", shaderPath.filename().string());

        std::string shaderCode;
        std::ifstream shaderFile;

        try
        {
                shaderFile.open(shaderPath);
                std::stringstream shaderStream;

                // Do some simple custom preprocessor stuff
                // #include -> works exactly like in c++

                std::string line;
                while (std::getline(shaderFile, line))
                {
                        if (line.starts_with("#include \""))
                        {
                                std::string includeFile =
                                    line.substr(line.find('"') + 1, line.find_last_of('"') - line.find('"') - 1);

                                std::filesystem::path includePath = shaderPath.parent_path() / includeFile;

                                std::ifstream includeStream(includePath);

                                if (includeStream.is_open()) { shaderStream << includeStream.rdbuf() << "\n"; }
                                else { spdlog::error("Invalid #include path in: {}", shaderPath.filename().string()); }

                                includeStream.close();
                        }
                        else { shaderStream << line << "\n"; }
                }

                shaderFile.close();

                shaderCode = shaderStream.str();
        }
        catch (const std::ifstream::failure& e)
        {
                spdlog::error("Failed to load shader file: {}", shaderPath.filename().string());
                spdlog::error(e.what());
                return false;
        }

        const char* pshaderCode = shaderCode.c_str();

        gl::glShaderSource(shader, 1, &pshaderCode, NULL);

        gl::glCompileShader(shader);

        int success;
        gl::glGetShaderiv(shader, gl::GLenum::GL_COMPILE_STATUS, &success);
        if (!success)
        {
                gl::GLsizei logSize;
                glGetShaderiv(shader, gl::GLenum::GL_INFO_LOG_LENGTH, &logSize);

                std::string infoLog;
                infoLog.resize(logSize);
                gl::glGetShaderInfoLog(shader, logSize, NULL, infoLog.data());

                spdlog::critical("Failed to compile shader: {} \n\n\n{}", shaderPath.string(), infoLog);
                return false;
        };

        return static_cast<bool>(success);
}

void ShaderProgram::linkProgram()
{
        if (isLinked) return;

        if (shaders.size() < 1) return;

        for (const auto& shader : shaders)
        {
                gl::glAttachShader(programId, shader.id);
        }

        gl::glLinkProgram(programId);

        int success;
        gl::glGetProgramiv(programId, gl::GLenum::GL_LINK_STATUS, &success);
        if (!success)
        {
                constexpr gl::GLsizei logSize = 512;
                std::string infoLog;

                infoLog.resize(logSize);

                gl::glGetProgramInfoLog(programId, logSize, NULL, infoLog.data());
                spdlog::error("Failed to link shaders: \n\n\n{}", infoLog);
                return;
        };

        isLinked = true;
}

void ShaderProgram::use()
{
        if (!isLinked) linkProgram();

        gl::glUseProgram(programId);
}
