#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct GLFWwindow;

class InputHandler
{
public:
	InputHandler(GLFWwindow* window);

	void pollInputs();

private:
	void pollAxes();
	void pollActions();
	void pollToggles();

private:
	GLFWwindow* windowPtr;

	struct ActionMapping
	{
		std::vector<int> keys;
		std::vector<int> mouseButtons;

		bool state = false;
	};

	struct AxisMapping
	{
		std::vector<int> keys;
		std::string axis;

		float scale = 1.0f;
	};

	struct ToggleMapping
	{
		std::vector<int> keys;
		std::vector<int> mouseButtons;

		bool previous = false; // 1 Frame buffer

		bool state = false;
	};

	std::vector<AxisMapping> axesMappings;
	std::unordered_map<std::string, float> axesValues;

	std::unordered_map<std::string, ActionMapping> actions;

	std::unordered_map<std::string, ToggleMapping> toggles;

public:
	void defineAxis(const std::string& name, const std::vector<int>& keys = { }, float scale = 1.0f);
	float getAxis(const std::string& name);

	void defineAction(const std::string& name, const std::vector<int>& keys = { }, const std::vector<int>& mouseButtons = { });
	bool getAction(const std::string& name);

	void defineToggle(const std::string& name, const std::vector<int>& keys = { }, const std::vector<int>& mouseButtons = { });
	bool getToggle(const std::string& name);
};