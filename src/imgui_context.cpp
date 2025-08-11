#include "imgui_context.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "profiler.h"
#include "timer.h"

EngineImGuiContext::EngineImGuiContext(const Window& window, Timer<>& timer)
{
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // IF using Docking Branch

        timer.addFixedIntervalFunction<std::chrono::seconds>([&]() { frametime = timer.getDeltaTimeMilliseconds(); });

        ImGui_ImplGlfw_InitForOpenGL(
            window.getWindowPtr().get(),
            true); // Second param install_callback=true will install GLFW callbacks and chain to existing ones.
        ImGui_ImplOpenGL3_Init();
}

EngineImGuiContext::~EngineImGuiContext()
{
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
}

void EngineImGuiContext::draw()
{
        PROFILE_FUNCTION();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        drawMenuBar();
        if (showMetrics) { drawMetrics(); }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void EngineImGuiContext::drawMetrics()
{
        PROFILE_FUNCTION();

        int windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

        if (!ImGui::Begin("Frame Metrics", &showMetrics, windowFlags))
        {
                ImGui::End();
                return;
        }

        ImGui::Text("%.3f ms/frame | %.2f fps", frametime, 1.0f / (frametime / 1000.0f));

        ImGui::End();
}

void EngineImGuiContext::drawMenuBar()
{
        PROFILE_FUNCTION();

        if (ImGui::BeginMainMenuBar())
        {
                if (ImGui::BeginMenu("Info"))
                {
                        ImGui::MenuItem("Frame Metrics", "", &showMetrics);

                        ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
        }
}
