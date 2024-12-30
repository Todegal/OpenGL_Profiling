#include "characterController.h"

#include <GLFW/glfw3.h>

#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/constants.hpp>

#include <spdlog/spdlog.h>

CharacterController::CharacterController(InputHandler& inputHandler, glm::vec3 startPos)
	: input(inputHandler), position(startPos), characterModel(std::make_shared<Model>("../Models/Dummy/Dummy.glb", "mixamorig:Hips")), state(IDLE)
{
	stateFunctions.push_back([&](float dt) { return idle(dt); });
	stateFunctions.push_back([&](float dt) { return walk(dt); });
	stateFunctions.push_back([&](float dt) { return sprint(dt); });
	stateFunctions.push_back([&](float dt) { return strafe(dt); });
	stateFunctions.push_back([&](float dt) { return strafeRun(dt); });
	stateFunctions.push_back([&](float dt) { return turn(dt); });

	characterModel->selectAnimation("idle");

	input.defineAxis("forward", { GLFW_KEY_W }, 1.0f);
	input.defineAxis("forward", { GLFW_KEY_S }, -1.0f);

	input.defineAxis("right", { GLFW_KEY_D }, 1.0f);
	input.defineAxis("right", { GLFW_KEY_A }, -1.0f);

	input.defineAction("sprint", { GLFW_KEY_LEFT_SHIFT });
	input.defineAction("jump", { GLFW_KEY_SPACE });

	input.defineAction("orbit", { }, { GLFW_MOUSE_BUTTON_LEFT });

	viewAzimuth = glm::radians(-90.0f);
	viewPolar = glm::radians(40.0f);
	radius = 2.0f;
	height = 1.8f;

	camera.center = position + glm::vec3(0.0f, height, 0.0f);

	const float sineAzimuth = glm::sin(viewAzimuth);
	const float cosineAzimuth = glm::cos(viewAzimuth);
	const float sinePolar = glm::sin(viewPolar);
	const float cosinePolar = glm::cos(viewPolar);

	const glm::vec3 eye = {
		position.x + (radius * cosinePolar * cosineAzimuth),
		position.y + (radius * sinePolar),
		position.z + (radius * cosinePolar * sineAzimuth)
	};

	camera.eye = eye;

	characterFacing = viewAzimuth + glm::pi<float>();

	forwardVector = glm::vec3(
		glm::cos(characterFacing),
		0.0f,
		glm::sin(characterFacing)
	);

	rightVector = -glm::cross(forwardVector, glm::vec3(0.0f, 1.0f, 0.0f));
	
	// calculate radial vector
	glm::vec3 xzEye = { position.x + (radius * cosineAzimuth), 0.0f, position.z + (radius * sineAzimuth) };
	radialVector = glm::normalize(glm::cross(glm::normalize(position - xzEye), glm::vec3(0.0f, 1.0f, 0.0f)));
}

void CharacterController::update(float dT)
{
	if (input.getAction("orbit"))
	{
		viewAzimuth += static_cast<float>(input.getMouseOffset().x) * 10.0f;
		viewPolar += static_cast<float>(input.getMouseOffset().y) * 10.0f;
	}

	viewAzimuth = std::fmod(viewAzimuth, glm::two_pi<float>());
	viewPolar = std::fmod(viewPolar, glm::two_pi<float>());
	characterFacing = std::fmod(characterFacing, glm::two_pi<float>());
	
	if (viewAzimuth < 0.0f) viewAzimuth += glm::two_pi<float>();
	if (characterFacing < 0.0f) characterFacing += glm::two_pi<float>();

	
	viewPolar = std::clamp(viewPolar, -glm::quarter_pi<float>(), glm::half_pi<float>() - 0.5f);
	

	const float sineAzimuth = glm::sin(viewAzimuth);
	const float cosineAzimuth = glm::cos(viewAzimuth);
	const float sinePolar = glm::sin(viewPolar);
	const float cosinePolar = glm::cos(viewPolar);

	const glm::vec3 eye = {
		camera.center.x + (radius * cosinePolar * cosineAzimuth),
		camera.center.y + (radius * sinePolar),
		camera.center.z + (radius * cosinePolar * sineAzimuth)
	};

	camera.eye = eye;

	camera.center = position + glm::vec3(0.0f, height, 0.0f);

	forwardVector = glm::normalize(glm::vec3(
		glm::cos(characterFacing), 
		0.0f, 
		glm::sin(characterFacing)
	));

	rightVector = -glm::normalize(glm::cross(forwardVector, glm::vec3(0.0f, 1.0f, 0.0f)));

	// calculate radial vector
	glm::vec3 xzEye = { camera.eye.x, 0.0f, camera.eye.z };
	radialVector = -glm::normalize(glm::cross(glm::normalize(position - xzEye), glm::vec3(0.0f, 1.0f, 0.0f)));

	state = stateFunctions[state](dT);

	characterModel->advanceAnimation(dT);

	characterModel->getTransform()->translation = position;
	characterModel->getTransform()->rotation = glm::quat(glm::vec3(0.0f, (-characterFacing / 2.0f) + glm::quarter_pi<float>(), 0.0f));
}

CharacterController::CHARACTER_STATE CharacterController::idle(float dT)
{
	characterModel->selectAnimation("idle", 0.4f);

	float newDirection = std::fmod(viewAzimuth + glm::pi<float>(), glm::two_pi<float>());

	if (input.getAxis("forward") != 0.0f)
	{
		if (std::abs(characterFacing - newDirection) > 0.1f)
			return TURN;

		return WALK;
	}

	if (input.getAxis("right") != 0.0f)
	{
		if (std::abs(characterFacing - newDirection) > 0.1f)
			return TURN;

		return state = STRAFE;
	}

	return IDLE;
}

CharacterController::CHARACTER_STATE CharacterController::turn(float dt)
{
	float newDirection = std::fmod(viewAzimuth + glm::pi<float>(), glm::two_pi<float>());

	if (std::abs(characterFacing - newDirection) < 0.1f)
		return IDLE;

	float diff = newDirection - characterFacing;
	bool isturningRight = (diff < glm::pi<float>() && diff >= 0.0f);

	characterModel->selectAnimation(isturningRight ? "turn_right" : "turn_left");

	characterFacing += (isturningRight ? 1.0f : -1.0f) * turnSpeed * dt;

	return TURN;
}

CharacterController::CHARACTER_STATE CharacterController::walk(float dt)
{
	if (input.getAxis("forward") == 0.0f)
	{
		if (input.getAxis("right") != 0.0f)
		{
			return STRAFE;
		}

		return IDLE;
	}

	if (input.getAction("sprint"))
	{
		return SPRINT;
	}

	characterModel->selectAnimation("walk", 0.2f, true);
	
	glm::vec2 axes = { input.getAxis("forward"), input.getAxis("right") };

	position += forwardVector * dt * characterModel->getVelocity().z * axes.x;

	characterFacing = viewAzimuth + glm::pi<float>() + (glm::quarter_pi<float>() * axes.y);

	return WALK;
}

CharacterController::CHARACTER_STATE CharacterController::sprint(float dt)
{
	if (input.getAxis("forward") == 0.0f)
	{
		if (input.getAxis("right") != 0.0f)
		{
			return STRAFE_RUN;
		}

		return IDLE;
	}

	if (!input.getAction("sprint"))
	{
		return WALK;
	}

	characterModel->selectAnimation("run", 0.2f, true);


	glm::vec2 axes = { input.getAxis("forward"), input.getAxis("right") };

	position += forwardVector * dt * characterModel->getVelocity().z * axes.x;

	characterFacing = viewAzimuth + glm::pi<float>() + (glm::quarter_pi<float>() * axes.y);
	
	return SPRINT;
}

CharacterController::CHARACTER_STATE CharacterController::strafe(float dt)
{
	if (input.getAxis("forward") != 0.0f)
	{
		return WALK;
	}

	if (input.getAxis("right") == 0.0f)
	{
		return IDLE;
	}

	if (input.getAction("sprint"))
	{
		return STRAFE_RUN;
	}

	float axis = input.getAxis("right");

	characterModel->selectAnimation(axis > 0.0f ? "strafe_right" : "strafe_left", 0.2f, true);

	glm::vec3 displacement = radialVector * dt * characterModel->getVelocity().x;
	viewAzimuth += (glm::length(displacement) / radius) * input.getAxis("right");

	spdlog::debug("eye : {}", glm::to_string(camera.getEye()));

	position += displacement;
	characterFacing = viewAzimuth + glm::pi<float>();

	return STRAFE;
}

CharacterController::CHARACTER_STATE CharacterController::strafeRun(float dt)
{
	if (input.getAxis("right") == 0.0f)
	{
		return IDLE;
	}

	if (!input.getAction("sprint"))
	{
		return STRAFE;
	}

	float axis = input.getAxis("right");

	characterModel->selectAnimation(axis > 0.0f ? "strafe_run_right" : "strafe_run_left", 0.2f, true);

	glm::vec3 displacement = radialVector * dt * characterModel->getVelocity().x;
	viewAzimuth += (glm::length(displacement) / radius) * input.getAxis("right");

	spdlog::debug("eye : {}", glm::to_string(camera.getEye()));

	position += displacement;
	characterFacing = viewAzimuth + glm::pi<float>();

	return STRAFE_RUN;
}

CharacterController::~CharacterController()
{
}

const glm::mat4 CharacterController::CharacterCamera::getViewMatrix() const
{
	return glm::lookAt(
		eye,
		center,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
}

const glm::vec3 CharacterController::CharacterCamera::getEye() const
{
	return eye;
}
