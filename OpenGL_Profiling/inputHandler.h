#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>

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

	glm::vec2 mousePos;
	glm::vec2 oldMousePos;

	float scroll;
	void setScrollOffset(float s) { scroll = s; }

public:
	void defineAxis(const std::string& name, const std::vector<int>& keys = { }, float scale = 1.0f);
	float getAxis(const std::string& name);

	void defineAction(const std::string& name, const std::vector<int>& keys = { }, const std::vector<int>& mouseButtons = { });
	bool getAction(const std::string& name);

	void defineToggle(const std::string& name, const std::vector<int>& keys = { }, const std::vector<int>& mouseButtons = { }, bool initialValue = false);
	bool getToggle(const std::string& name);

	const glm::vec2 getMousePos() const { return mousePos;	};
	const glm::vec2 getMouseOffset() const { return mousePos - oldMousePos; }

	const float getScrollOffset() const { return scroll; }
};