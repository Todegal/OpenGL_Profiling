#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "opengl_context.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class ShaderProgram
{
      private:
        struct Shader
        {
                const gl::GLuint id;
                const std::filesystem::path path;
        };

      public:
        ShaderProgram(const GLContext&, const std::filesystem::path vertexShader,
                      const std::filesystem::path fragmentShader);
        ShaderProgram();
        ~ShaderProgram();

      public:
        gl::GLuint getProgramId() const
        {
                return programId;
        }

        void addShader(gl::GLenum stage, const std::filesystem::path shaderPath);

        void linkProgram();

        void use();

        inline gl::GLint getLocation(const std::string& name) const
        {
                return gl::glGetUniformLocation(programId, name.c_str());
        }

        inline void setBool(const std::string& name, bool value) const
        {
                gl::glUniform1i(gl::glGetUniformLocation(programId, name.c_str()), static_cast<int>(value));
        }

        inline void setInt(const std::string& name, int value) const
        {
                gl::glUniform1i(gl::glGetUniformLocation(programId, name.c_str()), value);
        }

        inline void setFloat(const std::string& name, float value) const
        {
                gl::glUniform1f(gl::glGetUniformLocation(programId, name.c_str()), value);
        }

        inline void setMat4(const std::string& name, const float* data) const
        {
                gl::glUniformMatrix4fv(gl::glGetUniformLocation(programId, name.c_str()), 1, gl::GL_FALSE, data);
        }

        inline void setMat4(const std::string& name, const glm::mat4& value) const
        {
                setMat4(name, glm::value_ptr(value));
        }

        inline void setMat3(const std::string& name, const float* data) const
        {
                gl::glUniformMatrix3fv(gl::glGetUniformLocation(programId, name.c_str()), 1, gl::GL_FALSE, data);
        }

        inline void setMat3(const std::string& name, const glm::mat3& value) const
        {
                setMat3(name, glm::value_ptr(value));
        }

        inline void setVec2(const std::string& name, const float* data) const
        {
                gl::glUniform2fv(gl::glGetUniformLocation(programId, name.c_str()), 1, data);
        }

        inline void setVec2(const std::string& name, const glm::vec2& value) const
        {
                setVec2(name, glm::value_ptr(value));
        }

        inline void setVec3(const std::string& name, const float* data) const
        {
                gl::glUniform3fv(gl::glGetUniformLocation(programId, name.c_str()), 1, data);
        }

        inline void setVec3(const std::string& name, const glm::vec3& value) const
        {
                setVec3(name, glm::value_ptr(value));
        }

        inline void setVec4(const std::string& name, const float* data) const
        {
                gl::glUniform4fv(gl::glGetUniformLocation(programId, name.c_str()), 1, data);
        }

        inline void setVec4(const std::string& name, const glm::vec4& value) const
        {
                setVec4(name, glm::value_ptr(value));
        }

      private:
        bool compileShader(gl::GLuint shader, const std::filesystem::path shaderPath);

      private:
        bool isLinked;
        gl::GLuint programId;

        std::vector<Shader> shaders;
};
