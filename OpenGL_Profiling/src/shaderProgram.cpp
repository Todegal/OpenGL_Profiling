#include "shaderProgram.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

ShaderProgram::ShaderProgram(const std::filesystem::path vertexShader, const std::filesystem::path fragmentShader)
	: isLinked(false)
{
	addShader(GL_VERTEX_SHADER, vertexShader);
    addShader(GL_FRAGMENT_SHADER, fragmentShader);

    programId = glCreateProgram();
}

ShaderProgram::ShaderProgram()
    : isLinked(false)
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

    // plan:
    // take the name of the shader source as input
    // look for the .spv file in the binary directory
    // if it exists load it
    // otherwise load the source file

    std::filesystem::path binaryPath = shaderBinaryDirectory / shaderPath;
    const std::string extension = binaryPath.extension().string();

    binaryPath.replace_extension(extension + ".spv");

    if (std::filesystem::exists(binaryPath))
    {
        const std::vector<char> data = loadShaderBinary(binaryPath);
        
        glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, data.data(), static_cast<GLsizei>(data.size()));
        glSpecializeShader(shader, "main", 0, nullptr, nullptr);
    }
    else
    {
        const std::filesystem::path sourcePath = shaderSourceDirectory / shaderPath;
        if (!std::filesystem::exists(sourcePath)) {
            spdlog::error("Failed to locate shader: {}", sourcePath.string());
        }

        const std::string source = loadShaderSource(sourcePath);
        const char* sourcePtr = source.c_str();

        glShaderSource(shader, 1, &sourcePtr, nullptr);

        glCompileShader(shader);
    }

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

std::vector<char> ShaderProgram::loadShaderBinary(const std::filesystem::path path)
{
    std::ifstream istream(path, std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(istream), {});
}

void ShaderProgram::loadFile(const std::filesystem::path& path, std::unordered_set<std::filesystem::path>& processedFiles, std::stringstream& shaderStream)
{
    const std::filesystem::path absolutePath = std::filesystem::absolute(path);

    if (processedFiles.find(absolutePath) != processedFiles.end())
    {
        throw std::runtime_error("Cyclic include!");
    }

    processedFiles.insert(absolutePath);

    std::ifstream fileStream(absolutePath);
    if (!fileStream.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path.string());
    }

    std::string line;
    while (std::getline(fileStream, line)) {
        if (line.find("#include") == 0)
        {
            const size_t start = line.find("\"") + 1;
            const size_t end = line.find("\"", start);
            if (start != std::string::npos && end != std::string::npos)
            {
                const std::filesystem::path includePath = path.parent_path() / line.substr(start, end - start);  
                loadFile(includePath, processedFiles, shaderStream);
                continue;
            }
        }

        shaderStream << line << "\n";
    }
}

std::string ShaderProgram::loadShaderSource(const std::filesystem::path path)
{
    std::stringstream shaderStream;
    std::unordered_set<std::filesystem::path> processedFiles;

    try
    {
        loadFile(path, processedFiles, shaderStream);
    }
    catch (const std::runtime_error& e)
    {
        spdlog::error("Error loading file {} : {}", path.string(), e.what());
        return "";
    }

    return shaderStream.str();
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
        spdlog::error("Failed to link shaders: \n\n{}", infoLog);
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
