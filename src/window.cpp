#include "window.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include "profiler.h"

GLFWContext::GLFWContext()
{
        glfwSetErrorCallback(
            [](int error, const char* description) { spdlog::error("GLFW Error {}: {}", error, description); });

        if (!glfwInit())
        {
                glfwTerminate();
                throw std::runtime_error("Failed to initialize GLFW!");
        }
}

GLFWContext::~GLFWContext()
{
        // this should destroy the window as well... bit suspect but okay
        glfwTerminate();
}

void GLFWContext::pollEvents()
{
        PROFILE_FUNCTION();
        glfwPollEvents();
}

Window::Window(const GLFWContext&, int startWidth, int startHeight, std::string title, const WindowFlags& flags)
{
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

        if (!flags.resizable) { glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); }

        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

        GLFWwindow_Deleter windowDeleter;
        windowPtr = GLFWUniqueWindowPtr(glfwCreateWindow(startWidth, startHeight, title.c_str(), nullptr, nullptr),
                                        windowDeleter);

        if (flags.startMaximized) { glfwMaximizeWindow(windowPtr.get()); }

        if (windowPtr == nullptr)
        {
                glfwTerminate();
                throw std::runtime_error("Failed to create window!");
        }
}

Window::~Window()
{
}

void Window::swapBuffers()
{
        PROFILE_FUNCTION();

        glfwSwapBuffers(windowPtr.get());
}

glm::ivec2 Window::getFramebufferSize() const
{
        int x, y;
        glfwGetFramebufferSize(windowPtr.get(), &x, &y);

        return glm::ivec2(x, y);
}
