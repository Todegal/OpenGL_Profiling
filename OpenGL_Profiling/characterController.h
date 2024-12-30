#pragma once

#include <functional>

#include "camera.h"
#include "inputHandler.h"
#include "model.h"

class CharacterController
{
private:
	InputHandler& input;
	const std::shared_ptr<Model> characterModel;

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

	CharacterCamera camera;

	float viewAzimuth;
	float viewPolar;
	float radius;
	float height;

	float characterFacing;
	
	glm::vec3 forwardVector;
	glm::vec3 rightVector;
	glm::vec3 radialVector;
	glm::vec3 position;

	float turnSpeed = 1.0f / 0.1f; // speed to turn whole circle

public:
	CharacterController(InputHandler& input, glm::vec3 startPos = { 0.0f, 0.0f, 0.0f });
	~CharacterController();

	void update(float dT);

public:
	const std::shared_ptr<Model> getModel() { return characterModel; }
	const Camera& getCamera() { return camera; }

	enum CHARACTER_STATE
	{
		IDLE,
		WALK,
		SPRINT,
		STRAFE,
		STRAFE_RUN,
		TURN,
		NUM_STATES
	} state;

	using state_f = std::function<CHARACTER_STATE(float)>;
	std::vector<state_f> stateFunctions;

private:
	CHARACTER_STATE idle(float dT);
	CHARACTER_STATE walk(float dT);
	CHARACTER_STATE sprint(float dt);
	CHARACTER_STATE strafe(float dT);
	CHARACTER_STATE strafeRun(float dt);
	CHARACTER_STATE turn(float dt);
};