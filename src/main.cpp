#include "opengl_context.h"
#include <chrono>
#include <glbinding/gl/bitfield.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/random.hpp>
#include <glm/gtx/string_cast.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "input_handler.h"
#include "orbit_camera.h"
// #include "pbr_renderer.h"
#include "imgui_context.h"
#include "raw_data.h"
// #include "shader_program.h"
#include "profiler.h"
#include "timer.h"
#include "window.h"

#include <iostream>

void print_region_recursive(const std::shared_ptr<Profiler::TimedRegion> region, int depth = 0)
{
        for (int i = 0; i < depth; ++i)
        {
                std::cout << "\t";
        }
        std::cout << region->name << ": " << region->getSampleMilliseconds()
                  << "ms, total: " << region->getDurationMilliseconds() / 1000.0f << "s\n";

        for (const auto& child : region->children)
        {
                print_region_recursive(std::get<1>(child), depth + 1);
        }
}

void print_regions(const Profiler& profiler)
{
        const auto regions = profiler.getTopRegions();
        for (const auto& region : regions)
        {
                print_region_recursive(std::get<1>(region));
        }
}

int main()
{
        Timer timer;

#ifndef NDEBUG
        spdlog::set_level(spdlog::level::info);
#endif
        spdlog::set_pattern("[%n] [%^%l%$] %v"); // logger name, colored level, message
        spdlog::set_default_logger(spdlog::stdout_color_mt("graphics_engine"));

        GLFWContext glfwContext;

        WindowFlags flags;
        flags.startMaximized = true;
        flags.resizable = true;

        Window window(glfwContext, 800, 600, "-- graphics_engine --", flags);

        GLContext glContext(window);

        EngineImGuiContext imguiContext(window, timer);

        InputHandler input(window);

        while (!window.shouldClose())
        {
                PROFILE_SCOPE("frame");

                timer.update();

                glfwContext.pollEvents();

                gl::glClear(gl::ClearBufferMask::GL_COLOR_BUFFER_BIT);

                imguiContext.draw();

                if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse) input.pollInputs();

                window.swapBuffers();
        }

        print_regions(getProfiler());

        return EXIT_SUCCESS;
}
