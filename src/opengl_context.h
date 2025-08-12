#pragma once

#include "profiler.h"
#include <glbinding/AbstractFunction.h>
#include <glbinding/CallbackMask.h>
#include <glbinding/FunctionCall.h>
#include <glbinding/glbinding.h>

#include <glbinding-aux/ContextInfo.h>
#include <glbinding-aux/Meta.h>
#include <glbinding-aux/debug.h>
#include <glbinding-aux/types_to_string.h>

#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <span>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

#include "window.h"

// Class which creates and maintains the opengl state
// should be passed as const reference to classes which need to modify or access the opengl state
class GLContext
{
      public:
        GLContext() = delete;
        GLContext(const Window& window);
        ~GLContext() = default;

        GLContext(const GLContext&) = delete;
        GLContext& operator=(const GLContext&) = delete;

        GLContext(const GLContext&&) = delete;
        GLContext& operator=(const GLContext&&) = delete;

        void enable(gl::GLenum cap)
        {
                if (state[cap] == false) { gl::glEnable(cap); }
                state[cap] = true;
        }

        void disable(gl::GLenum cap)
        {
                if (state[cap] == true) { gl::glDisable(cap); }
                state[cap] = false;
        }

        const Window& getWindow() const
        {
                return windowRef;
        }

      private:
        std::unordered_map<gl::GLenum, bool> state;

        const Window& windowRef;
};

class GLBuffer
{
      public:
        GLBuffer() = delete;
        GLBuffer(const GLContext&) : allocatedSize(0)
        {
		PROFILE_FUNCTION();

                gl::glCreateBuffers(1, &bufferID);
                spdlog::trace("Created Buffer: {}", bufferID);
        }

        ~GLBuffer()
        {
                gl::glDeleteBuffers(1, &bufferID);
                spdlog::trace("Destroyed Buffer: {}", bufferID);
        }

        // Uncopyable & unmovable,
        // I'm still torn on this but I HATE
        // the idea of a random uninitialized buffer
        // just existing, it kills me. So no move/copy
        GLBuffer(const GLBuffer&) = delete;
        GLBuffer& operator=(const GLBuffer&) = delete;

        GLBuffer(GLBuffer&& other) = delete;
        GLBuffer& operator=(const GLBuffer&&) = delete;

        template <gl::GLenum target>
        void bind()
        {
                static_assert(isTargetValid(target), "Must be a valid target!");

                glBindBuffer(target, bufferID);
        }

        template <gl::GLenum target>
        void bindBase(int base)
        {
                static_assert(target == gl::GLenum::GL_ATOMIC_COUNTER_BUFFER ||
                                  target == gl::GLenum::GL_TRANSFORM_FEEDBACK_BUFFER ||
                                  target == gl::GLenum::GL_UNIFORM_BUFFER ||
                                  target == gl::GLenum::GL_SHADER_STORAGE_BUFFER,
                              "Must be a valid target!");

                glBindBufferBase(target, base, bufferID);
        }

        template <typename T>
        void allocate(size_t count, gl::GLenum usage)
        {
                static_assert(std::is_trivially_copyable_v<T>);
                gl::glNamedBufferData(bufferID, static_cast<gl::GLsizeiptr>(count * sizeof(T)), nullptr, usage);
                allocatedSize = count * sizeof(T);

                spdlog::trace("Allocating: {} bytes for buffer: {}", sizeof(T) * count, bufferID);
        }

        template <typename T>
        void bufferData(std::span<const T> data, gl::GLenum usage)
        {
                static_assert(std::is_trivially_copyable<T>::value, "Buffer data must be copyable!");
                if (allocatedSize < data.size() * sizeof(T)) { allocate<T>(data.size(), usage); }

                gl::glNamedBufferSubData(bufferID, 0, static_cast<gl::GLsizeiptr>(data.size() * sizeof(T)),
                                         data.data());
        }

        template <typename T>
        void subData(gl::GLintptr offset, std::span<const T> data)
        {
                static_assert(std::is_trivially_copyable<T>::value, "Buffer data must be copyable!");
                if (offset + (data.size() * sizeof(T)) > allocatedSize)
                {
                        throw std::runtime_error(
                            "Cannot sub data into an undersized buffer! Use 'bufferData' instead!");
                }

                gl::glNamedBufferSubData(bufferID, offset, static_cast<gl::GLsizeiptr>(data.size() * sizeof(T)),
                                         data.data());
        }

      private:
        static constexpr bool isTargetValid(gl::GLenum target)
        {
                return target == gl::GLenum::GL_ARRAY_BUFFER || target == gl::GLenum::GL_ATOMIC_COUNTER_BUFFER ||
                       target == gl::GLenum::GL_COPY_READ_BUFFER || target == gl::GLenum::GL_COPY_WRITE_BUFFER ||
                       target == gl::GLenum::GL_DISPATCH_INDIRECT_BUFFER ||
                       target == gl::GLenum::GL_DRAW_INDIRECT_BUFFER || target == gl::GLenum::GL_ELEMENT_ARRAY_BUFFER ||
                       target == gl::GLenum::GL_PIXEL_PACK_BUFFER || target == gl::GLenum::GL_PIXEL_UNPACK_BUFFER ||
                       target == gl::GLenum::GL_QUERY_BUFFER || target == gl::GLenum::GL_SHADER_STORAGE_BUFFER ||
                       target == gl::GLenum::GL_TEXTURE_BUFFER || target == gl::GLenum::GL_TRANSFORM_FEEDBACK_BUFFER ||
                       target == gl::GLenum::GL_UNIFORM_BUFFER;
        }

      private:
        size_t allocatedSize;
        gl::GLuint bufferID;
};

class GLVertexArray
{
      public:
        GLVertexArray() = delete;
        GLVertexArray(const GLContext&)
        {
		PROFILE_FUNCTION();

                gl::glGenVertexArrays(1, &vaoID);
                spdlog::trace("Created VAO: {}", vaoID);
        }

        ~GLVertexArray()
        {
                gl::glDeleteVertexArrays(1, &vaoID);
                spdlog::trace("Destroyed VAO: {}", vaoID);
        }

        GLVertexArray(const GLVertexArray&) = delete;
        GLVertexArray& operator=(const GLVertexArray&) = delete;

        GLVertexArray(GLVertexArray&& other) = delete;
        GLVertexArray& operator=(const GLVertexArray&&) = delete;

        void bind()
        {
                gl::glBindVertexArray(vaoID);
        }

      private:
        gl::GLuint vaoID;
};
