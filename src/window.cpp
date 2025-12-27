#include "window.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include "profiler.h"

GLFWContext::GLFWContext()
{
        PROFILE_FUNCTION();

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

Window::Window(const GLFWContext&, const WindowCreationFlags& flags)
{
        PROFILE_FUNCTION();

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

        if (!flags.resizable) { glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); }
        if (flags.samples) { glfwWindowHint(GLFW_SAMPLES, flags.samples); }

        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

        const auto& monitor = glfwGetPrimaryMonitor();
        const auto& vidMode = glfwGetVideoMode(monitor);

        glfwWindowHint(GLFW_BLUE_BITS, vidMode->blueBits);
        glfwWindowHint(GLFW_RED_BITS, vidMode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, vidMode->greenBits);
        glfwWindowHint(GLFW_REFRESH_RATE, vidMode->refreshRate);

        std::size_t width = flags.width;
        std::size_t height = flags.height;

        if (flags.fullscreen)
        {
                if (width == 0 || height == 0)
                {
                        width = vidMode->width;
                        height = vidMode->height;
                }
        }
        else if (width == 0 || height == 0) { throw std::runtime_error("Width and Height must be positive integers"); }

        GLFWwindow_Deleter windowDeleter;
        windowPtr = GLFWUniqueWindowPtr(glfwCreateWindow(static_cast<int>(width), static_cast<int>(height),
                                                         flags.title.c_str(), flags.fullscreen ? monitor : nullptr, nullptr),
                                        windowDeleter);
        if (windowPtr == nullptr)
        {
                glfwTerminate();
                throw std::runtime_error("Failed to create window!");
        }

        if (flags.maximized) { glfwMaximizeWindow(windowPtr.get()); }
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
        PROFILE_FUNCTION();

        int x, y;
        glfwGetFramebufferSize(windowPtr.get(), &x, &y);

        return glm::ivec2(x, y);
}
