#pragma once

#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include <memory>

class GLFWContext
{
      public:
        GLFWContext();
        ~GLFWContext();

        GLFWContext(GLFWContext&) = delete;
        GLFWContext& operator=(GLFWContext&) = delete;

        GLFWContext(GLFWContext&&) = delete;
        GLFWContext& operator=(GLFWContext&&) = delete;

        void pollEvents();
};

struct WindowFlags
{
        bool startMaximized = false;
        bool resizable = false;

        std::int8_t samples{};
};

class Window
{
      public:
        Window(const GLFWContext&, int startWidth, int startHeight, std::string title, const WindowFlags& flags);
        ~Window();

        Window(Window&) = delete;
        Window& operator=(Window&) = delete;

        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

      private:
        struct GLFWwindow_Deleter // Functor to destroy GLFWwindow*
        {
                void operator()(GLFWwindow* w) const
                {
                        spdlog::trace("Destroying window!");
                        glfwDestroyWindow(w);
                }
        };
        using GLFWUniqueWindowPtr = std::unique_ptr<GLFWwindow, GLFWwindow_Deleter>;

        GLFWUniqueWindowPtr windowPtr;

      public:
        const GLFWUniqueWindowPtr& getWindowPtr() const
        {
                return windowPtr;
        }

        bool shouldClose() const
        {
                return glfwWindowShouldClose(windowPtr.get());
        }

        void swapBuffers();

        glm::ivec2 getFramebufferSize() const;
};
