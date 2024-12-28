#include "inputHandler.h"

#include <GLFW/glfw3.h>

#include <algorithm>

InputHandler::InputHandler(GLFWwindow* window)
	: windowPtr(window)
{
}

void InputHandler::pollInputs()
{
	pollAxes();
	pollActions();
	pollToggles();
}

void InputHandler::pollAxes()
{
	for (auto& [_, value] : axesValues)
	{
		value = 0.0f;
	}

	for (const auto& map : axesMappings)
	{
		float mod = 0.0f;
		for (const auto k : map.keys)
		{
			if (glfwGetKey(windowPtr, k) != GLFW_RELEASE)
			{
				mod += map.scale;
			}
		}

		axesValues[map.axis] += std::clamp(mod, -1.0f, 1.0f);
	}
}

void InputHandler::pollActions()
{
	for (auto& [name, map] : actions)
	{
		bool value = false;
		for (const auto k : map.keys)
		{
			if (glfwGetKey(windowPtr, k) != GLFW_RELEASE)
			{
				value = true;
			}
		}

		for (const auto m : map.mouseButtons)
		{
			if (glfwGetMouseButton(windowPtr, m) != GLFW_RELEASE)
			{
				value = true;
			}
		}

		map.state = value;
	}
}

void InputHandler::pollToggles()
{
	for (auto& [name, map] : toggles)
	{
		bool mod = false;
		for (const auto k : map.keys)
		{
			if (glfwGetKey(windowPtr, k) == GLFW_PRESS && !map.previous)
			{
				mod = true;
				map.previous = true; 
			}
			else if (glfwGetKey(windowPtr, k) == GLFW_RELEASE)
			{
				map.previous = false; 
			}
		}

		for (const auto m : map.mouseButtons)
		{
			if (glfwGetMouseButton(windowPtr, m) == GLFW_PRESS && !map.previous)
			{
				mod = true;
				map.previous = true; 
			}
			else if (glfwGetMouseButton(windowPtr, m) == GLFW_RELEASE)
			{
				map.previous = false; 
			}
		}

		if (mod)
			map.state = !map.state;
	}
}

void InputHandler::defineAxis(const std::string& name, const std::vector<int>& keys, float scale)
{
	AxisMapping map;
	map.axis = name;
	map.keys = keys;
	map.scale = scale;

	axesMappings.push_back(map);

	if (axesValues.find(name) == axesValues.end())
	{
		axesValues[name] = 0.0f;
	}
}

float InputHandler::getAxis(const std::string& name)
{
	if (axesValues.find(name) != axesValues.end())
		return axesValues[name];

	return 0.0f;
}

void InputHandler::defineAction(const std::string& name, const std::vector<int>& keys, const std::vector<int>& mouseButtons)
{
	ActionMapping map;
	map.keys = keys;
	map.mouseButtons = mouseButtons;
	map.state = false;

	actions[name] = map;
}

bool InputHandler::getAction(const std::string& name)
{
	if (actions.find(name) != actions.end())
		return actions[name].state;

	return false;
}

void InputHandler::defineToggle(const std::string& name, const std::vector<int>& keys, const std::vector<int>& mouseButtons)
{
	ToggleMapping map;
	map.keys = keys;
	map.mouseButtons = mouseButtons;
	map.state = false;

	toggles[name] = map;
}

bool InputHandler::getToggle(const std::string& name)
{
	if (toggles.find(name) != toggles.end())
		return toggles[name].state;

	return false;
}
