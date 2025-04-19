#pragma once

#include <imgui.h>

class PBRRenderer;
class Timer;

struct imgui_data
{
	bool showRenderFlags;
	bool showMetrics;
	bool showCharacterInfo;
	bool orbitCameraEnabled;
};

void metrics(Timer& timer, imgui_data& data);
void drawMenuBar(imgui_data& data);