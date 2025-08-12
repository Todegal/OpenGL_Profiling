#pragma once

#include <glbinding/gl/types.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <filesystem>

#include "opengl_context.h"

// Individual shader in the pipline, RAII bound
class GLShader
{
      public:
        GLShader(const GLContext&, const std::string_view shaderSource, const gl::GLenum stage);

        ~GLShader();

        GLShader(const GLShader&) = delete;
        GLShader& operator=(const GLShader&) = delete;

        gl::GLuint getId() const
        {
                return shaderId;
        }

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

        gl::GLuint getProgramId() const
        {
                return programId;
        }

      private:
        gl::GLuint programId;
};
