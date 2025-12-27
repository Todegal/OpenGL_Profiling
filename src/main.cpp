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

#include <argparse/argparse.hpp>

#ifndef NDEBUG
#include "imgui_context.h"
#endif

#include "input_handler.h"
#include "opengl_context.h"
#include "orbit_camera.h"
#include "pbr_renderer.h"
#include "profiler.h"
#include "raw_data.h"
#include "simple_renderer.h"
#include "timer.h"
#include "window.h"
#include "scene_graph.h"

#include <numbers>

int main(int argc, char** argv)
{
        argparse::ArgumentParser program("graphics_engine", "0.0.1");
        program.add_description("Graphics engine designed by Niall Townley.");

        std::string filepath;
        program.add_argument("filepath")
            .help("Path of the scene to load (*.gltf, *.gltf, *.obj, *.fbx, etc.)")
            .store_into(filepath);

        bool fullscreen;
        program.add_argument("--fullscreen")
            .help("If present the engine will launch in fullscreen mode")
            .flag()
            .store_into(fullscreen);

        std::vector<int> dimensions;
        program.add_argument("-d", "--dimensions")
            .help("Set the screen dimensions")
            .nargs(2)
            .default_value(std::vector<int>{0, 0})
            .scan<'i', int>()
            .store_into(dimensions);

        try
        {
                program.parse_args(argc, argv);
        }
        catch (const std::exception& err)
        {
                std::cerr << err.what() << "\n";
                std::cerr << program;

                return EXIT_FAILURE;
        }

        Timer timer;

#ifndef NDEBUG
        spdlog::set_level(spdlog::level::debug);
#endif
        spdlog::set_pattern("[%H:%M:%S][%n] [%^%l%$] %v"); // logger name, coloured level, message
        spdlog::set_default_logger(spdlog::stdout_color_mt("graphics_engine"));

#ifndef NDEBUG
        static const std::uint32_t initRegionId = Profiler::RegisterRegion("initialization", __FILE__, __LINE__);
        Profiler::BeginRegion(initRegionId);
#endif
        try
        {
                GLFWContext glfwContext;

                WindowCreationFlags flags;
                flags.fullscreen = fullscreen;
                flags.width = dimensions.at(0);
                flags.height = dimensions.at(1);
                flags.title = std::format("-- graphics engine (built: {}@{}) --", __DATE__, __TIME__);

                Window window(glfwContext, flags);

                GLContext glContext(window);

#ifndef NDEBUG
                EngineImGuiContext imguiContext(window, timer);
#endif

                InputHandler input(window);
                input.defineAction("orbit", {}, {GLFW_MOUSE_BUTTON_1});
                input.defineAction("zoom", {}, {GLFW_MOUSE_BUTTON_2});
                input.defineAction("pan", {GLFW_KEY_LEFT_SHIFT});

                input.defineToggle("toggle_hdr", {GLFW_KEY_H}, {}, true);
                input.defineToggle("toggle_forward_pass", {GLFW_KEY_F}, {}, true);
                input.defineToggle("toggle_deferred_pass", {GLFW_KEY_D}, {}, false);
                input.defineToggle("toggle_normals", {GLFW_KEY_N}, {}, true);

                const Point3<WorldSpace> viewCenter(glm::vec3(0.0f));
                const Vec3<WorldSpace> upVector(0.0f, 1.0f, 0.0f);

                std::shared_ptr<OrbitCamera> orbitCamera =
                    std::make_shared<OrbitCamera>(viewCenter, upVector, 1.0f, 0.01f);

                std::unique_ptr<PBRRenderer> renderer;

                SceneGraph sceneGraph;

                {
                        RawScene scene(sceneGraph);
                        scene.addFile(filepath);
                        scene.addFile("test_models/tv/Television_01_4k.gltf");

                        renderer = std::make_unique<PBRRenderer>(glContext, scene, sceneGraph, orbitCamera);
                }

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

                                if (input.getAction("pan"))
                                {
                                        orbitCamera->moveHorizontal(-mouseOffset.x * 10.0f);
                                        orbitCamera->moveVertical(mouseOffset.y * 10.0f);
                                }
                                else
                                {
                                        orbitCamera->rotateAzimuth(mouseOffset.x *
                                                                   static_cast<float>(std::numbers::pi) * 2.0f);
                                        orbitCamera->rotatePolar(mouseOffset.y * static_cast<float>(std::numbers::pi) *
                                                                 2.0f);
                                }
                        }
                        else if (input.getAction("zoom"))
                        {
                                const glm::vec2 mouseOffset = input.getMouseOffset();

                                orbitCamera->zoom(mouseOffset.y * 10.0f);
                        }

                        // todo: remove this and sort out some ui
                        RenderFlags newFlags;
                        newFlags.set<RenderFlags::HDR_PASS_ENABLED>(input.getToggle("toggle_hdr"));
                        newFlags.set<RenderFlags::FORWARD_PASS_ENABLED>(input.getToggle("toggle_forward_pass"));
                        newFlags.set<RenderFlags::DEFERRED_PASS_ENABLED>(input.getToggle("toggle_deferred_pass"));
                        newFlags.set<RenderFlags::NORMALS_ENABLED>(input.getToggle("toggle_normals"));
                        renderer->setRenderFlags(newFlags);

                        glfwContext.pollEvents();

                        renderer->frame();

#ifndef NDEBUG
                        imguiContext.draw();
#endif

                        window.swapBuffers();
                }
        }
        catch (const std::runtime_error& e)
        {
                spdlog::critical(e.what());
                return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}
