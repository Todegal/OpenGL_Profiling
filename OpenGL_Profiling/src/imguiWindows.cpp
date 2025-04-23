#include "imguiWindows.h"

#include <format>

#include "pbrRenderer.h"
#include "timer.h"


void metrics(Timer& timer, imgui_data& data)
{
	int windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

	if (!ImGui::Begin("Frame Metrics", &data.showMetrics, windowFlags))
	{
		ImGui::End();
		return;
	}

	static float frametime = 0.0f;
	timer.fixedTimeStep<std::ratio<1, 1>>([&]() { frametime = timer.getDeltaTime<Timer::f_mlliseconds>().count(); });

	ImGui::Text("%.3f ms/frame | %.2f fps", frametime, 1.0f / (frametime / 1000.0f));

	ImGui::End();
}

void drawMenuBar(imgui_data& data)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Info"))
        {
            ImGui::MenuItem("Render Flags", "", &data.showRenderDialog);
			ImGui::MenuItem("Frame Metrics", "", &data.showMetrics);
			ImGui::MenuItem("Character Info", "", &data.showCharacterInfo);
			ImGui::Checkbox("Unlock Camera", &data.orbitCameraEnabled);

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}