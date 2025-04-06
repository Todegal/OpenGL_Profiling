#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glad/glad.h>

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>

class ShaderProgram
{
private:
	struct Shader
	{
		const GLuint id;
		const std::filesystem::path path;
	};

	static inline int shouldRecompile = 0;
	int recompileCount;

public:
	ShaderProgram(const std::filesystem::path vertexShader, const std::filesystem::path fragmentShader);
	ShaderProgram();
	~ShaderProgram();

public:
	static void recompileAllPrograms() { shouldRecompile++; }

public:

	void addShader(GLenum stage, const std::filesystem::path shaderPath);
	
	void linkProgram();
	
	void recompileProgram();

	void use();

	inline const GLint getLocation(const std::string& name) const 
	{
		return glGetUniformLocation(programId, name.c_str());
	}

	inline void setBool(const std::string& name, bool value) const
	{
		glUniform1i(glGetUniformLocation(programId, name.c_str()), static_cast<int>(value));
	}

	inline void setInt(const std::string& name, int value) const
	{
		glUniform1i(glGetUniformLocation(programId, name.c_str()), value);
	}

	inline void setFloat(const std::string& name, float value) const
	{
		glUniform1f(glGetUniformLocation(programId, name.c_str()), value);
	}

	inline void setMat4(const std::string& name, const float* data) const
	{
		glUniformMatrix4fv(glGetUniformLocation(programId, name.c_str()), 1, GL_FALSE, data);
	}

	inline void setMat4(const std::string& name, const glm::mat4& value) const
	{
		setMat4(name, glm::value_ptr(value));
	}

	inline void setMat3(const std::string& name, const float* data) const
	{
		glUniformMatrix3fv(glGetUniformLocation(programId, name.c_str()), 1, GL_FALSE, data);
	}

	inline void setMat3(const std::string& name, const glm::mat3& value) const
	{
		setMat3(name, glm::value_ptr(value));
	}

	inline void setVec2(const std::string& name, const float* data) const
	{
		glUniform2fv(glGetUniformLocation(programId, name.c_str()), 1, data);
	}

	inline void setVec2(const std::string& name, const glm::vec2& value) const
	{
		setVec2(name, glm::value_ptr(value));
	}

	inline void setVec3(const std::string& name, const float* data) const
	{
		glUniform3fv(glGetUniformLocation(programId, name.c_str()), 1, data);
	}

	inline void setVec3(const std::string& name, const glm::vec3& value) const
	{
		setVec3(name, glm::value_ptr(value));
	}

	inline void setVec4(const std::string& name, const float* data) const
	{
		glUniform4fv(glGetUniformLocation(programId, name.c_str()), 1, data);
	}

	inline void setVec4(const std::string& name, const glm::vec4& value) const
	{
		setVec4(name, glm::value_ptr(value));
	}

private:
	bool compileShader(GLuint shader, const std::filesystem::path shaderPath);

private:
	bool isLinked;
	GLuint programId;

	std::vector<Shader> shaders;
};
