#include "shaderProgram.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

ShaderProgram::ShaderProgram(const std::filesystem::path vertexShader, const std::filesystem::path fragmentShader)
	: isLinked(false), recompileCount(0)
{
	addShader(GL_VERTEX_SHADER, vertexShader);
    addShader(GL_FRAGMENT_SHADER, fragmentShader);

    programId = glCreateProgram();
}

ShaderProgram::ShaderProgram()
    : isLinked(false), recompileCount(0)
{
    programId = glCreateProgram();
}

ShaderProgram::~ShaderProgram()
{
    for (const auto& shader : shaders)
    {
        glDeleteShader(shader.id);
    }

    glDeleteProgram(programId);
}

void ShaderProgram::addShader(GLenum stage, const std::filesystem::path shaderPath)
{
    if (isLinked)
    {
        isLinked = false;
    }

    GLuint shaderId = glCreateShader(stage);

    if (!compileShader(shaderId, shaderPath))
    {
        glDeleteShader(shaderId);
        return;
    }

    shaders.push_back({ shaderId, shaderPath });
}

bool ShaderProgram::compileShader(GLuint shader, const std::filesystem::path shaderPath)
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

                if (includeStream.is_open())
                {
                    shaderStream << includeStream.rdbuf() << "\n";
                }
                else
                {
                    spdlog::error("Invalid #include path in: {}", shaderPath.filename().string());
                }

                includeStream.close();
            }
            else
            {
                shaderStream << line << "\n";
            }
        }

        shaderFile.close();

        shaderCode = shaderStream.str();
    }
    catch (std::ifstream::failure e)
    {
        spdlog::error("Failed to load shader file: {}", shaderPath.filename().string());
        spdlog::error(e.what());
        return false;
    }

    const char* pshaderCode = shaderCode.c_str();

    glShaderSource(shader, 1, &pshaderCode, NULL);

    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLsizei logSize;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);

        std::string infoLog;
        infoLog.resize(logSize);
        glGetShaderInfoLog(shader, logSize, NULL, infoLog.data());

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
        glAttachShader(programId, shader.id);
    }

    glLinkProgram(programId);

    int success;
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success)
    {
        constexpr GLsizei logSize = 512;
        std::string infoLog;

        infoLog.resize(logSize);

        glGetProgramInfoLog(programId, logSize, NULL, infoLog.data());
        spdlog::error("Failed to link shaders: \n\n\n{}", infoLog);
        return;
    };

    isLinked = true;
}

void ShaderProgram::recompileProgram()
{
    isLinked = false;

    for (const auto& shader : shaders)
    {
        glDetachShader(programId, shader.id);

        if (!compileShader(shader.id, shader.path))
        {
            glDeleteShader(shader.id);
            return;
        }
    }
}

void ShaderProgram::use()
{
    if (shouldRecompile > recompileCount)
    {
        recompileProgram();
        recompileCount++;
    }

    if (!isLinked)
        linkProgram();

    glUseProgram(programId);
}
