#include "opengl_context.h"
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
#include "imgui_windows.h"
#include "raw_data.h"
// #include "shader_program.h"
#include "timer.h"
#include "window.h"

#include <algorithm>
#include <execution>
#include <format>
#include <memory>

int main()
{
        Timer t;
        t.start();

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

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // IF using Docking Branch

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(
            window.getWindowPtr().get(), true); // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
        ImGui_ImplOpenGL3_Init();

        imgui_data imguiData;

        InputHandler input(window);

        while (!window.shouldClose())
        {
                t.tick();

		glfwContext.pollEvents();

		gl::glClear(gl::ClearBufferMask::GL_COLOR_BUFFER_BIT);

                // Start the Dear ImGui frame
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();

                drawMenuBar(imguiData);
                if (imguiData.showMetrics) { metrics(t, imguiData); }

                if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse) input.pollInputs();


                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		window.swapBuffers();
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        return EXIT_SUCCESS;
}
