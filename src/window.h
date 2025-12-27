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

struct WindowCreationFlags
{
        std::size_t width = 1920;
        std::size_t height = 1080;
        std::string title = "";
        bool maximized = false;
        bool resizable = false;
        bool fullscreen = false;
};

class Window
{
      public:
        Window(const GLFWContext&, const WindowCreationFlags& flags);
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
