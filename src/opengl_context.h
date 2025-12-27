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
#include "raw_data.h"
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
        GLBuffer() = delete;

        GLBuffer(const GLContext&, size_t sizeBytes, gl::BufferStorageMask storageMask)
        {
                allocatedSizeBytes = sizeBytes;

                gl::glCreateBuffers(1, &bufferID);
                gl::glNamedBufferStorage(bufferID, static_cast<gl::GLsizeiptr>(allocatedSizeBytes), nullptr,
                                         storageMask);

                spdlog::trace("Created buffer: {}, with {} bytes", bufferID, allocatedSizeBytes);
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
        GLBuffer& operator=(GLBuffer&&) = delete;

        virtual void bind(gl::GLenum target)
        {
                glBindBuffer(target, bufferID);
        }

        virtual void bindBase(gl::GLenum target, gl::GLuint base)
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

      private:
        std::size_t allocatedSizeBytes;
        gl::GLuint bufferID;

        friend class GLVertexArray;
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

        void bindVertexBuffer(gl::GLuint bindingIndex, const GLBuffer& buffer, gl::GLintptr offset, gl::GLsizei stride)
        {
                gl::glVertexArrayVertexBuffer(vaoID, bindingIndex, buffer.bufferID, offset, stride);
        }

        void bindElementBuffer(const GLBuffer& buffer)
        {
                gl::glVertexArrayElementBuffer(vaoID, buffer.bufferID);
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

class GLTexture2D
{
      public:
        GLTexture2D(const GLContext&, std::size_t width, std::size_t height, gl::GLenum internalFormat,
                    std::size_t suggestedLevels = 0)
            : w(width), h(height)
        {
                const int maxLevels = static_cast<int>(std::log2(std::min(w, h))) - 1;
                const int levels = std::max(maxLevels, 1);

                gl::glCreateTextures(gl::GLenum::GL_TEXTURE_2D, 1, &textureID);
                gl::glTextureStorage2D(
                    textureID, suggestedLevels > 0 ? std::min(static_cast<int>(suggestedLevels), maxLevels) : levels,
                    internalFormat, static_cast<gl::GLsizei>(w), static_cast<gl::GLsizei>(h));

                gl::glTextureParameteri(textureID, gl::GLenum::GL_TEXTURE_MIN_FILTER,
                                        gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);
        }

        GLTexture2D(const GLContext&, const RawTexture& texture) : w(texture.getWidth()), h(texture.getHeight())
        {
                gl::GLenum format;
                switch (texture.getChannels())
                {
                case 1:
                        format = gl::GLenum::GL_R;
                        break;
                case 2:
                        format = gl::GLenum::GL_RG;
                        break;
                case 3:
                        format = gl::GLenum::GL_RGB;
                        break;
                default:
                        format = gl::GLenum::GL_RGBA;
                        break;
                }

                const int maxLevels = static_cast<int>(std::log2(std::min(w, h))) - 1;
                const int levels = std::max(maxLevels, 1);

                gl::glCreateTextures(gl::GLenum::GL_TEXTURE_2D, 1, &textureID);
                gl::glTextureStorage2D(textureID, levels, gl::GLenum::GL_RGBA8, static_cast<gl::GLsizei>(w),
                                       static_cast<gl::GLsizei>(h));

                gl::glTextureParameteri(textureID, gl::GLenum::GL_TEXTURE_MIN_FILTER,
                                        gl::GLenum::GL_LINEAR_MIPMAP_LINEAR);

                subImage(0, 0, 0, w, h, format, texture.getData());
        }

        ~GLTexture2D()
        {
                gl::glDeleteTextures(1, &textureID);
        }

        GLTexture2D(const GLTexture2D&) = delete;
        GLTexture2D& operator=(const GLTexture2D&) = delete;

        GLTexture2D(GLTexture2D&& other) = delete;
        GLTexture2D& operator=(const GLTexture2D&&) = delete;

        void subImage(std::size_t level, std::size_t offsetX, std::size_t offsetY, std::size_t width,
                      std::size_t height, gl::GLenum format, std::span<const std::uint8_t> data)
        {
                gl::glTextureSubImage2D(textureID, static_cast<gl::GLint>(level), static_cast<gl::GLint>(offsetX),
                                        static_cast<gl::GLint>(offsetY), static_cast<gl::GLsizei>(width),
                                        static_cast<gl::GLsizei>(height), format, gl::GLenum::GL_UNSIGNED_BYTE,
                                        data.data());
        }

        void fillMipmaps()
        {
                gl::glGenerateTextureMipmap(textureID);
        }

        void bindUnit(int unit)
        {
                gl::glBindTextureUnit(unit, textureID);
        }

        std::size_t getWidth() const
        {
                return w;
        }

        std::size_t getHeight() const
        {
                return h;
        }

        gl::GLuint getID()
        {
                return textureID;
        }

      private:
        const std::size_t w, h;
        gl::GLuint textureID;
};
