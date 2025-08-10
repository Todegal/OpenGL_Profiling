#pragma once

#include <functional>

#include "timer.h"
#include "camera.h"
#include "inputHandler.h"
#include "model.h"
#include "imguiWindows.h"
#include "animationController.h"


class CharacterController
{
private:
	InputHandler& input;
	const std::shared_ptr<RenderableModel> characterModel;

	AnimationController animControl;

private:
	class CharacterCamera : public Camera
	{
	public:
		// Inherited via Camera
		const glm::mat4 getViewMatrix() const override;
		const glm::vec3 getEye() const override;

		glm::vec3 eye;
		glm::vec3 center;
	};

	CharacterCamera viewCamera;

	float viewAzimuth;
	float viewPolar;
	float radius;
	float height;

	float characterFacing;
	
	glm::vec3 forwardVector;
	glm::vec3 rightVector;
	glm::vec3 radialVector;
	glm::vec3 position;

public:
	CharacterController(InputHandler& input, glm::vec3 startPos = { 0.0f, 0.0f, 0.0f });
	~CharacterController();

	void update(Timer::f_seconds dT);
	void showInfo(imgui_data& data);

public:
	const std::shared_ptr<RenderableModel> getModel() { return characterModel; }
	std::shared_ptr<Camera> getCameraPtr() { return std::make_shared<CharacterCamera>(std::move(viewCamera)); }

	enum CHARACTER_STATE
	{
		IDLE,
		WALK,
		SPRINT,
		WALK_BACK,
		HOP,
		TURN,
		NUM_STATES
	} state;

	using state_f = std::function<CHARACTER_STATE(float)>;
	std::vector<state_f> stateFunctions;

private:
	CHARACTER_STATE idle(float dT);
	CHARACTER_STATE walk(float dT);
	CHARACTER_STATE sprint(float dt);
	CHARACTER_STATE turn(float dt);
	CHARACTER_STATE hop(float dt);
};