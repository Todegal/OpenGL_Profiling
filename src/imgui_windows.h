#pragma once

#include <imgui.h>

class PBRRenderer_old;
class Timer;

struct imgui_data
{
	bool showMetrics;
	bool showCharacterInfo;
	bool orbitCameraEnabled;

	bool showRenderDialog;
};

void metrics(Timer& timer, imgui_data& data);
void drawMenuBar(imgui_data& data);