#pragma once

#include <glbinding/gl/functions.h>
#include <glbinding/gl/types.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <filesystem>
#include <unordered_set>

#include "opengl_context.h"

// Individual shader in the pipline, RAII bound
class GLShader
{
      public:
        GLShader(const GLContext&, const std::string_view shaderSource, const gl::GLenum stage);

        ~GLShader();

        GLShader(const GLShader&) = delete;
        GLShader& operator=(const GLShader&) = delete;

        gl::GLenum getStage() const
        {
                return stage;
        }

        const std::filesystem::path& getPath() const
        {
                return absoluteShaderPath;
        }

      private:
        gl::GLuint shaderId;
        std::filesystem::path absoluteShaderPath;
        gl::GLenum stage;

        friend class GLShaderProgram;
};

class GLShaderProgram
{
      public:
        GLShaderProgram(const GLContext&, const std::vector<std::shared_ptr<GLShader>> shaders);
        ~GLShaderProgram();

        GLShaderProgram(const GLShaderProgram&) = delete;
        GLShaderProgram& operator=(const GLShaderProgram&) = delete;

        GLShaderProgram(GLShaderProgram&&) = delete;
        GLShaderProgram& operator=(GLShaderProgram&&) = delete;

        void setUniformValue(const std::string& name, const glm::vec4& value);
        void setUniformValue(const std::string& name, const int value);

        void setUniformBlockBinding(const std::string& name, const gl::GLuint binding);
        void setShaderStorageBlockBinding(const std::string& name, const gl::GLuint binding);

        void useProgram()
        {
                gl::glUseProgram(programId);
        }

        gl::GLuint getProgramId() const
        {
                return programId;
        }

      private:
        gl::GLuint programId;

        std::unordered_set<std::string> uniformVariableNames;
        std::unordered_set<std::string> uniformBlockNames;
        std::unordered_set<std::string> shaderStorageBlockNames;

        std::unordered_set<std::string> getResourceNames(gl::GLenum resourceInterface);
};
