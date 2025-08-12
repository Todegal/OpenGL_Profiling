#include "orbit_camera.h"
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

#include "imgui_context.h"
#include "input_handler.h"
#include "opengl_context.h"
#include "profiler.h"
#include "raw_data.h"
#include "simple_renderer.h"
#include "timer.h"
#include "window.h"

int main()
{
        Timer timer;

#ifndef NDEBUG
        spdlog::set_level(spdlog::level::trace);
#endif
        spdlog::set_pattern("[%n] [%^%l%$] %v"); // logger name, colored level, message
        spdlog::set_default_logger(spdlog::stdout_color_mt("graphics_engine"));

        static const uint32_t initRegionId = Profiler::RegisterRegion("initialization", __FILE__, __LINE__);
        Profiler::BeginRegion(initRegionId);

        GLFWContext glfwContext;

        WindowFlags flags;
        flags.startMaximized = true;
        flags.resizable = true;

        Window window(glfwContext, 800, 600, "-- graphics_engine --", flags);

        GLContext glContext(window);

        EngineImGuiContext imguiContext(window, timer);

        InputHandler input(window);
        input.defineAction("orbit", {}, {GLFW_MOUSE_BUTTON_1});
        input.defineAction("zoom", {}, {GLFW_MOUSE_BUTTON_2});

        OrbitCamera orbitCamera(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1.0f, 0.001f);

        RawScene scene;
        scene.addFile("../opengl/models/FlightHelmet/glTF/FlightHelmet.gltf");

        SimpleRenderer renderer(glContext, scene, orbitCamera, timer);

        Profiler::EndRegion();

        imguiContext.getProfiler().updateInitEvents(Profiler::GetCurrentFrameEvents());

        while (!window.shouldClose())
        {
                Profiler::EndFrame();

                PROFILE_SCOPE("frame");

                timer.update();

                if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse) input.pollInputs();

                if (input.getAction("orbit"))
                {
                        const glm::vec2 mouseOffset = input.getMouseOffset();

                        orbitCamera.rotateAzimuth(mouseOffset.x * M_PI * 2.0f);
                        orbitCamera.rotatePolar(mouseOffset.y * M_PI * 2.0f);
                }
                else if (input.getAction("zoom"))
                {
                        const glm::vec2 mouseOffset = input.getMouseOffset();

                        orbitCamera.zoom(mouseOffset.y * orbitCamera.getRadius() * 10.0f);
                }

                glfwContext.pollEvents();

                renderer.render();

                imguiContext.draw();

                window.swapBuffers();
        }

        return EXIT_SUCCESS;
}
