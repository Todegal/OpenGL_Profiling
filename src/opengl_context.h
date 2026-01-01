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
                if (offset + length > allocatedSizeBytes)
                {
                        throw std::runtime_error("Cannot map beyond the buffer bounds!");
                }

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

class GLTexture
{
      public:
        GLTexture(const GLContext&, gl::GLenum target) : textureID(0)
        {
                gl::glCreateTextures(target, 1, &textureID);
        }

        GLTexture(const GLTexture&) = delete;
        GLTexture& operator=(const GLTexture&) = delete;

        GLTexture(GLTexture&& other) = delete;
        GLTexture& operator=(const GLTexture&&) = delete;

        ~GLTexture()
        {
                gl::glDeleteTextures(1, &textureID);
        }

        void setParameter(gl::GLenum parameter, gl::GLenum value)
        {
                gl::glTextureParameteri(textureID, parameter, value);
        }

        void setParameter(gl::GLenum parameter, gl::GLfloat value)
        {
                gl::glTextureParameterf(textureID, parameter, value);
        }

        void setParameter(gl::GLenum parameter, const gl::GLfloat* values)
        {
                gl::glTextureParameterfv(textureID, parameter, values);
        }

        void setParameter(gl::GLenum parameter, const gl::GLint* values)
        {
                gl::glTextureParameteriv(textureID, parameter, values);
        }

      protected:
        gl::GLuint textureID;

      private:
        friend class GLFramebuffer;
};

class GLTexture2D : public GLTexture
{
      public:
        GLTexture2D(const GLContext& c, std::size_t width, std::size_t height, gl::GLenum internalFormat,
                    std::size_t suggestedLevels = 0)
            : GLTexture(c, gl::GLenum::GL_TEXTURE_2D), size(width, height)
        {
                const int maxLevels = static_cast<int>(std::log2(std::min(size.x, size.y))) - 1;
                const int levels = std::max(maxLevels, 1);

                gl::glTextureStorage2D(
                    textureID, suggestedLevels > 0 ? std::min(static_cast<int>(suggestedLevels), maxLevels) : levels,
                    internalFormat, static_cast<gl::GLsizei>(size.x), static_cast<gl::GLsizei>(size.y));
        }

        GLTexture2D(const GLContext& c, const RawTexture& texture)
            : GLTexture(c, gl::GLenum::GL_TEXTURE_2D), size(texture.getDimensions())
        {
                gl::GLenum format;
                gl::GLenum internalFormat;
                switch (texture.getChannels())
                {
                case 1:
                        format = gl::GLenum::GL_R;
                        internalFormat = gl::GLenum::GL_R8;
                        break;
                case 2:
                        format = gl::GLenum::GL_RG;
                        internalFormat = gl::GLenum::GL_RG8;
                        break;
                case 3:
                        format = gl::GLenum::GL_RGB;
                        internalFormat = gl::GLenum::GL_RGB8;
                        break;
                default:
                        format = gl::GLenum::GL_RGBA;
                        internalFormat = gl::GLenum::GL_RGBA8;
                        break;
                }

                const int maxLevels = static_cast<int>(std::log2(std::min(size.x, size.y))) - 1;
                const int levels = std::max(maxLevels, 1);

                gl::glTextureStorage2D(textureID, levels, internalFormat, static_cast<gl::GLsizei>(size.x),
                                       static_cast<gl::GLsizei>(size.y));

                subImage(0, 0, 0, size.x, size.y, format, texture.getData());
        }

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

        const glm::ivec2 getDimensions() const
        {
                return size;
        }

      private:
        const glm::ivec2 size;
};

class GLRenderbuffer
{
      public:
        GLRenderbuffer(const GLContext&, std::size_t width, std::size_t height, gl::GLenum internalFormat)
        {

                gl::glCreateRenderbuffers(1, &renderbufferID);
                gl::glNamedRenderbufferStorage(renderbufferID, internalFormat, static_cast<gl::GLsizei>(width),
                                               static_cast<gl::GLsizei>(height));
        }

        ~GLRenderbuffer()
        {
                gl::glDeleteRenderbuffers(1, &renderbufferID);
        }

        GLRenderbuffer(const GLRenderbuffer&) = delete;
        GLRenderbuffer& operator=(const GLRenderbuffer&) = delete;

        GLRenderbuffer(GLRenderbuffer&& other) = delete;
        GLRenderbuffer& operator=(const GLRenderbuffer&&) = delete;

      private:
        gl::GLuint renderbufferID;

        friend class GLFramebuffer;
};

class GLFramebuffer
{
      public:
        GLFramebuffer(const GLContext&)
        {
                gl::glCreateFramebuffers(1, &framebufferID);
        }

        ~GLFramebuffer()
        {
                gl::glDeleteFramebuffers(1, &framebufferID);
        }

        GLFramebuffer(const GLFramebuffer&) = delete;
        GLFramebuffer& operator=(const GLFramebuffer&) = delete;

        GLFramebuffer(GLFramebuffer&& other) = delete;
        GLFramebuffer& operator=(const GLFramebuffer&&) = delete;

        void bindRead() const
        {
                gl::glBindFramebuffer(gl::GLenum::GL_READ_FRAMEBUFFER, framebufferID);
        }

        void bindDraw() const
        {
                gl::glBindFramebuffer(gl::GLenum::GL_DRAW_FRAMEBUFFER, framebufferID);
        };

        void bindAttachment(gl::GLenum attachment, const GLTexture& texture, gl::GLint level)
        {
                gl::glNamedFramebufferTexture(framebufferID, attachment, texture.textureID, level);
        }

        void bindAttachment(gl::GLenum attachment, const GLRenderbuffer& renderbuffer)
        {
                gl::glNamedFramebufferRenderbuffer(framebufferID, attachment, gl::GLenum::GL_RENDERBUFFER,
                                                   renderbuffer.renderbufferID);
        }

        bool isComplete()
        {
                return gl::glCheckNamedFramebufferStatus(framebufferID, gl::GLenum::GL_FRAMEBUFFER) ==
                       gl::GLenum::GL_FRAMEBUFFER_COMPLETE;
        }

        void clearBuffer(gl::GLenum buffer, gl::GLint drawBuffer, const gl::GLfloat* value)
        {
                gl::glClearNamedFramebufferfv(framebufferID, buffer, drawBuffer, value);
        }

        // stupid function TODO: rewrite
        void blitToScreen(std::size_t screenWidth, std::size_t screenHeight)
        {
                gl::glBlitNamedFramebuffer(framebufferID, 0, 0, 0, static_cast<gl::GLint>(screenWidth),
                                           static_cast<gl::GLint>(screenHeight), 0, 0,
                                           static_cast<gl::GLint>(screenWidth), static_cast<gl::GLint>(screenHeight),
                                           gl::ClearBufferMask::GL_COLOR_BUFFER_BIT, gl::GLenum::GL_LINEAR);
        }

      private:
        gl::GLuint framebufferID;
};
