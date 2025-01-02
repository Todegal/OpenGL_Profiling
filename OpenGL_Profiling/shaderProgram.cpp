#include "shaderProgram.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

ShaderProgram::ShaderProgram(const std::filesystem::path& vertexShader, const std::filesystem::path& fragmentShader)
	: programId(0), isLinked(false)
{
	addShader(GL_VERTEX_SHADER, vertexShader);
    addShader(GL_FRAGMENT_SHADER, fragmentShader);
}

ShaderProgram::ShaderProgram()
    : programId(0), isLinked(false)
{
}

ShaderProgram::~ShaderProgram()
{
    for (const auto& shader : shaders)
    {
        glDeleteShader(shader);
    }
}

void ShaderProgram::addShader(GLenum type, const std::filesystem::path& shaderPath)
{
    if (isLinked)
    {
        spdlog::error("Unable to add shader to pre-linked shader program!");
    }

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
        while(std::getline(shaderFile, line))
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
        return;
    }

    const char* pshaderCode = shaderCode.c_str();

    GLuint shaderId = glCreateShader(type);
    glShaderSource(shaderId, 1, &pshaderCode, NULL);
    
    if (!compileShader(shaderId, shaderPath))
        return;

    shaders.push_back(shaderId);
}

bool ShaderProgram::compileShader(GLuint shader, const std::filesystem::path& shaderPath)
{
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

    programId = glCreateProgram();
    for (const auto& shader : shaders)
    {
        glAttachShader(programId, shader);
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

void ShaderProgram::use()
{
    if (!isLinked)
        linkProgram();

    glUseProgram(programId);
}
