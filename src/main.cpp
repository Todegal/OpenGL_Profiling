#include <glbinding/gl/bitfield.h>

#include <glbinding/gl/functions.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/random.hpp>
#include <glm/gtx/string_cast.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#ifndef NDEBUG
#include "imgui_context.h"
#endif

#include "input_handler.h"
#include "opengl_context.h"
#include "orbit_camera.h"
#include "profiler.h"
#include "raw_data.h"
#include "simple_renderer.h"
#include "timer.h"
#include "window.h"

#include <numbers>

int main()
{
        Timer timer;

#ifndef NDEBUG
        spdlog::set_level(spdlog::level::trace);
#endif
        spdlog::set_pattern("[%n] [%^%l%$] %v"); // logger name, colored level, message
        spdlog::set_default_logger(spdlog::stdout_color_mt("graphics_engine"));

#ifndef NDEBUG
        static const uint32_t initRegionId = Profiler::RegisterRegion("initialization", __FILE__, __LINE__);
        Profiler::BeginRegion(initRegionId);
#endif
        try
        {
                GLFWContext glfwContext;

                WindowFlags flags;
                flags.startMaximized = true;
                flags.resizable = true;

                Window window(glfwContext, 1280, 720, "-- graphics_engine --", flags);

                GLContext glContext(window);

#ifndef NDEBUG
                EngineImGuiContext imguiContext(window, timer);
#endif

                InputHandler input(window);
                input.defineAction("orbit", {}, {GLFW_MOUSE_BUTTON_1});
                input.defineAction("zoom", {}, {GLFW_MOUSE_BUTTON_2});

                OrbitCamera orbitCamera(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1.0f, 0.001f);

                RawScene scene;
                scene.addFile("test_models/camera/Camera_01_4k.gltf");

                SimpleRenderer renderer(glContext, scene, orbitCamera, timer);

#ifndef NDEBUG
                Profiler::EndRegion();

                imguiContext.getProfiler().updateInitEvents(Profiler::GetCurrentFrameEvents());
#endif

                while (!window.shouldClose())
                {
                        Profiler::EndFrame();

                        PROFILE_SCOPE("frame");

                        timer.update();

#ifndef NDEBUG
                        if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse)
                        {
#endif
                                input.pollInputs();
#ifndef NDEBUG
                        }
#endif

                        if (input.getAction("orbit"))
                        {
                                const glm::vec2 mouseOffset = input.getMouseOffset();

                                orbitCamera.rotateAzimuth(mouseOffset.x * std::numbers::pi * 2.0f);
                                orbitCamera.rotatePolar(mouseOffset.y * std::numbers::pi * 2.0f);
                        }
                        else if (input.getAction("zoom"))
                        {
                                const glm::vec2 mouseOffset = input.getMouseOffset();

                                orbitCamera.zoom(mouseOffset.y * 10.0f);
                        }

                        glfwContext.pollEvents();

                        renderer.render();

#ifndef NDEBUG
                        imguiContext.draw();
#endif

                        window.swapBuffers();
                }
        }
        catch (const std::exception& e)
        {
                spdlog::critical(e.what());
                return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}
