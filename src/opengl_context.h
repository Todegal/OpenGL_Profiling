#pragma once

#include <glbinding/AbstractFunction.h>
#include <glbinding/CallbackMask.h>
#include <glbinding/FunctionCall.h>
#include <glbinding/gl/bitfield.h>
#include <glbinding/gl/boolean.h>
#include <glbinding/gl/types.h>
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

#include "profiler.h"
#include "window.h"

#include <span>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

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
        GLBuffer() = default;
        // ~GLBuffer() = delete;

        GLBuffer(const GLBuffer&) = delete;
        GLBuffer& operator=(const GLBuffer&) = delete;
        GLBuffer(GLBuffer&& other) = delete;
        GLBuffer& operator=(GLBuffer&&) = delete;

        virtual void bind(gl::GLenum target) = 0;
        virtual void bindBase(gl::GLenum target, int base) = 0;

        gl::GLuint getID() const
        {
                return bufferID;
        }

        size_t getAllocatedSize() const
        {
                return allocatedSizeBytes;
        }

      protected:
        size_t allocatedSizeBytes = 0;
        gl::GLuint bufferID = 0;
};

class GLMutableBuffer : public GLBuffer
{
      public:
        GLMutableBuffer() = delete;

        template <typename T>
        GLMutableBuffer(const GLContext&, std::span<const T> data, gl::GLenum usage)
        {
                static_assert(std::is_trivially_copyable_v<T>, "Buffer data must be copyable!");

                allocatedSizeBytes = data.size() * sizeof(T);

                gl::glCreateBuffers(1, &bufferID);
                gl::glNamedBufferData(bufferID, static_cast<gl::GLsizeiptr>(allocatedSizeBytes), data.data(), usage);

                spdlog::trace("Created mutable buffer: {}, with {} bytes", bufferID, allocatedSizeBytes);
        }

        GLMutableBuffer(const GLContext&, size_t sizeBytes, gl::GLenum usage)
        {
                allocatedSizeBytes = sizeBytes;

                gl::glCreateBuffers(1, &bufferID);
                gl::glNamedBufferData(bufferID, static_cast<gl::GLsizeiptr>(allocatedSizeBytes), nullptr, usage);

                spdlog::trace("Created mutable buffer: {}, with {} bytes", bufferID, allocatedSizeBytes);
        }

        ~GLMutableBuffer()
        {
                gl::glDeleteBuffers(1, &bufferID);
                spdlog::trace("Destroyed Buffer: {}", bufferID);
        }

        // Uncopyable & unmovable,
        // I'm still torn on this but I HATE
        // the idea of a random uninitialized buffer
        // just existing, it kills me. So no move/copy
        GLMutableBuffer(const GLMutableBuffer&) = delete;
        GLMutableBuffer& operator=(const GLMutableBuffer&) = delete;
        GLMutableBuffer(GLMutableBuffer&& other) = delete;
        GLMutableBuffer& operator=(GLMutableBuffer&&) = delete;

        virtual void bind(gl::GLenum target) override
        {
                glBindBuffer(target, bufferID);
        }

        virtual void bindBase(gl::GLenum target, int base) override
        {
                glBindBufferBase(target, base, bufferID);
        }

        template <typename T>
        void setData(std::span<const T> data, gl::GLenum usage)
        {
                static_assert(std::is_trivially_copyable<T>::value, "Buffer data must be copyable!");

                allocatedSizeBytes = data.size() * sizeof(T);

                gl::glNamedBufferData(bufferID, static_cast<gl::GLsizeiptr>(allocatedSizeBytes), data.data(), usage);
        }

        template <typename T>
        void subData(gl::GLintptr offsetBytes, std::span<const T> data)
        {
                static_assert(std::is_trivially_copyable<T>::value, "Buffer data must be copyable!");

                const size_t dataSizeBytes = sizeof(T) * data.size();
                if (offsetBytes + dataSizeBytes > allocatedSizeBytes)
                {
                        throw std::runtime_error("Cannot sub data into an undersized buffer!");
                }

                gl::glNamedBufferSubData(bufferID, offsetBytes, static_cast<gl::GLsizeiptr>(data.size() * sizeof(T)),
                                         data.data());
        }

        template <typename T = std::byte>
        T* map(gl::GLenum access)
        {
                return static_cast<T*>(gl::glMapNamedBuffer(bufferID, access));
        }

        template <typename T = std::byte>
        T* mapRange(gl::GLsizeiptr offset, gl::GLsizeiptr length, gl::BufferAccessMask access)
        {
                if (offset + length > allocatedSizeBytes) {}
                return static_cast<T*>(gl::glMapNamedBufferRange(bufferID, offset, length, access));
        }

        void unmap()
        {
                gl::glUnmapNamedBuffer(bufferID);
        }
};

class GLImmutableBuffer : public GLBuffer
{
      public:
        GLImmutableBuffer() = delete;

        template <typename T>
        GLImmutableBuffer(const GLContext&, std::span<const T> data, gl::BufferStorageMask accessFlags)
        {
                static_assert(std::is_trivially_copyable_v<T>, "Buffer data must be copyable!");

                allocatedSizeBytes = data.size() * sizeof(T);

                gl::glCreateBuffers(1, &bufferID);
                gl::glNamedBufferStorage(bufferID, static_cast<gl::GLsizeiptr>(allocatedSizeBytes), data.data(),
                                         accessFlags);

                spdlog::trace("Created mutable buffer: {}, with {} bytes", bufferID, allocatedSizeBytes);
        }

        GLImmutableBuffer(const GLContext&, size_t sizeBytes, gl::BufferStorageMask accessFlags)
        {
                allocatedSizeBytes = sizeBytes;

                gl::glCreateBuffers(1, &bufferID);
                gl::glNamedBufferStorage(bufferID, static_cast<gl::GLsizeiptr>(allocatedSizeBytes), nullptr,
                                         accessFlags);

                spdlog::trace("Created mutable buffer: {}, with {} bytes", bufferID, allocatedSizeBytes);
        }

        ~GLImmutableBuffer()
        {
                gl::glDeleteBuffers(1, &bufferID);
                spdlog::trace("Destroyed Buffer: {}", bufferID);
        }

        // Uncopyable & unmovable,
        // I'm still torn on this but I HATE
        // the idea of a random uninitialized buffer
        // just existing, it kills me. So no move/copy
        GLImmutableBuffer(const GLImmutableBuffer&) = delete;
        GLImmutableBuffer& operator=(const GLImmutableBuffer&) = delete;
        GLImmutableBuffer(GLImmutableBuffer&& other) = delete;
        GLImmutableBuffer& operator=(GLImmutableBuffer&&) = delete;

        virtual void bind(gl::GLenum target) override
        {
                glBindBuffer(target, bufferID);
        }

        virtual void bindBase(gl::GLenum target, int base) override
        {
                glBindBufferBase(target, base, bufferID);
        }

        template <typename T>
        void subData(gl::GLintptr offsetBytes, std::span<const T> data)
        {
                static_assert(std::is_trivially_copyable<T>::value, "Buffer data must be copyable!");

                const size_t dataSizeBytes = sizeof(T) * data.size();
                if (offsetBytes + dataSizeBytes > allocatedSizeBytes)
                {
                        throw std::runtime_error("Cannot sub data into an undersized buffer!");
                }

                gl::glNamedBufferSubData(bufferID, offsetBytes, static_cast<gl::GLsizeiptr>(data.size() * sizeof(T)),
                                         data.data());
        }

        template <typename T = std::byte>
        T* map(gl::GLenum access)
        {
                return static_cast<T*>(gl::glMapNamedBuffer(bufferID, access));
        }

        template <typename T = std::byte>
        T* mapRange(gl::GLsizeiptr offset, gl::GLsizeiptr length, gl::BufferAccessMask access)
        {
                if (offset + length > allocatedSizeBytes) {}
                return static_cast<T*>(gl::glMapNamedBufferRange(bufferID, offset, length, access));
        }

        void unmap()
        {
                gl::glUnmapNamedBuffer(bufferID);
        }
};

class GLVertexArray
{
      public:
        GLVertexArray() = delete;
        GLVertexArray(const GLContext&)
        {
                gl::glCreateVertexArrays(1, &vaoID);
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

        void bindVertexBuffer(gl::GLuint bindingIndex, const gl::GLuint& bufferID, gl::GLintptr offset,
                              gl::GLsizei stride)
        {
                gl::glVertexArrayVertexBuffer(vaoID, bindingIndex, bufferID, offset, stride);
        }

        void bindElementBuffer(const gl::GLuint& bufferID)
        {
                gl::glVertexArrayElementBuffer(vaoID, bufferID);
        }

        void defineAttribute(gl::GLuint attribIndex, gl::GLuint bindingIndex, gl::GLint size, gl::GLenum type,
                             gl::GLboolean normalized, gl::GLuint relativeOffset)
        {
                gl::glEnableVertexArrayAttrib(vaoID, attribIndex);
                gl::glVertexArrayAttribFormat(vaoID, attribIndex, size, type, normalized, relativeOffset);
                gl::glVertexArrayAttribBinding(vaoID, attribIndex, bindingIndex);
        }

      private:
        gl::GLuint vaoID;
};

class GLTexture
{
      public:
        GLTexture(const GLContext&);
        ~GLTexture();
};
