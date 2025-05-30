#include "characterController.h"

#include <GLFW/glfw3.h>

#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/constants.hpp>

#include <spdlog/spdlog.h>

CharacterController::CharacterController(InputHandler& inputHandler, glm::vec3 startPos)
	: input(inputHandler), position(startPos), characterModel(std::make_shared<RenderableModel>(RawModel("../Models/Dummy/Dummy.glb", "mixamorig:Hips").extract(), false)),
	state(IDLE), animControl(characterModel)
{
	animControl.addBlend("move_right", "walk", "strafe_right");
	animControl.addBlend("move_left", "walk", "strafe_left");

	animControl.addBlend("sprint_right", "run", "strafe_run_right");
	animControl.addBlend("sprint_left", "run", "strafe_run_left");

	animControl.getAnimation("hop").loop = false;
	animControl.getAnimation("turn_left").loop = false;

	animControl.selectAnimation("idle", 0.0f);

	stateFunctions.resize(NUM_STATES);
	stateFunctions[IDLE] = ([&](float dt) { return idle(dt); });
	stateFunctions[WALK] = ([&](float dt) { return walk(dt); });
	stateFunctions[SPRINT] = ([&](float dt) { return sprint(dt); });
	stateFunctions[TURN] = ([&](float dt) { return turn(dt); });
	stateFunctions[HOP] = ([&](float dt) { return hop(dt); });

	input.defineAxis("forward", { GLFW_KEY_W }, { }, 1.0f);
	input.defineAxis("forward", { GLFW_KEY_S }, { GLFW_GAMEPAD_AXIS_LEFT_Y },  -1.0f);

	input.defineAxis("right", { GLFW_KEY_D }, { GLFW_GAMEPAD_AXIS_LEFT_X }, 1.0f);
	input.defineAxis("right", { GLFW_KEY_A }, { }, - 1.0f);

	input.defineAxis("lookPolar", { }, { GLFW_GAMEPAD_AXIS_RIGHT_Y });
	input.defineAxis("lookAzimuth", { }, { GLFW_GAMEPAD_AXIS_RIGHT_X }, -1.0f);

	input.defineAction("sprint", { GLFW_KEY_LEFT_SHIFT }, { }, { GLFW_GAMEPAD_BUTTON_LEFT_BUMPER });
	input.defineAction("jump", { GLFW_KEY_SPACE }, { }, { GLFW_GAMEPAD_BUTTON_A });

	input.defineAction("orbit", { }, { GLFW_MOUSE_BUTTON_LEFT });

	viewAzimuth = glm::radians(0.0f);
	viewPolar = glm::radians(40.0f);
	radius = 2.0f;
	height = 1.8f;

	viewCamera.center = position + glm::vec3(0.0f, height, 0.0f);

	const float sineAzimuth = glm::sin(viewAzimuth);
	const float cosineAzimuth = glm::cos(viewAzimuth);
	const float sinePolar = glm::sin(viewPolar);
	const float cosinePolar = glm::cos(viewPolar);

	const glm::vec3 eye = {
		viewCamera.center.x + (radius * cosinePolar * sineAzimuth),
		viewCamera.center.y + (radius * sinePolar),
		viewCamera.center.z + (radius * cosinePolar * cosineAzimuth)
	};

	viewCamera.eye = eye;

	characterFacing = 0.0f;

	forwardVector = glm::vec3(
		glm::sin(characterFacing),
		0.0f,
		glm::cos(characterFacing)
	);

	rightVector = -glm::cross(forwardVector, glm::vec3(0.0f, 1.0f, 0.0f));
	
	// calculate radial vector
	glm::vec3 xzEye = { position.x + (radius * cosineAzimuth), 0.0f, position.z + (radius * sineAzimuth) };
	radialVector = glm::normalize(glm::cross(glm::normalize(position - xzEye), glm::vec3(0.0f, 1.0f, 0.0f)));
}

void CharacterController::update(Timer::f_seconds dT)
{
	const float deltaTime = dT.count() / 1.0f;

	if (input.getAction("orbit"))
	{
		viewAzimuth -= static_cast<float>(input.getMouseOffset().x) * 10.0f;
		viewPolar += static_cast<float>(input.getMouseOffset().y) * 10.0f;
	}
	else
	{
		viewAzimuth += input.getAxis("lookAzimuth") * 10.0f * deltaTime;
		viewPolar += input.getAxis("lookPolar") * 10.0f * deltaTime;
	}

	viewAzimuth = std::fmod(viewAzimuth, glm::two_pi<float>());
	viewPolar = std::fmod(viewPolar, glm::two_pi<float>());
	
	if (viewAzimuth < 0.0f) viewAzimuth += glm::two_pi<float>();
	if (characterFacing < 0.0f) characterFacing += glm::two_pi<float>();

	viewPolar = std::clamp(viewPolar, -glm::quarter_pi<float>(), glm::half_pi<float>() - 0.5f);

	const float sineAzimuth = glm::sin(viewAzimuth);
	const float cosineAzimuth = glm::cos(viewAzimuth);
	const float sinePolar = glm::sin(viewPolar);
	const float cosinePolar = glm::cos(viewPolar);

	viewCamera.center = position + glm::vec3(0.0f, height, 0.0f);

	const glm::vec3 eye = {
		viewCamera.center.x + (radius * cosinePolar * sineAzimuth),
		viewCamera.center.y + (radius * sinePolar),
		viewCamera.center.z + (radius * cosinePolar * cosineAzimuth)
	};

	viewCamera.eye = eye;

	forwardVector = glm::vec3(
		glm::sin(characterFacing),
		0.0f,
		glm::cos(characterFacing)
	);

	rightVector = -glm::normalize(glm::cross(forwardVector, glm::vec3(0.0f, 1.0f, 0.0f)));

	// calculate radial vector
	glm::vec3 xzEye = { viewCamera.eye.x, 0.0f, viewCamera.eye.z };
	radialVector = -glm::normalize(glm::cross(glm::normalize(position - xzEye), glm::vec3(0.0f, 1.0f, 0.0f)));

	state = stateFunctions[state](deltaTime);

	characterFacing = std::fmod(characterFacing, glm::two_pi<float>());

	animControl.advance(deltaTime);

	glm::vec3 displacement = (forwardVector * animControl.getVelocity().z) +
		(rightVector * animControl.getVelocity().x);

	position += displacement * deltaTime;

	characterModel->getTransform()->setTranslation(position);
	characterModel->getTransform()->setRotation(glm::quat(glm::vec3(0.0f, characterFacing / 2.0f, 0.0f)));
}

void CharacterController::showInfo(imgui_data& data)
{
	int windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

	if (!ImGui::Begin("Character Debug", &data.showCharacterInfo, windowFlags))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Position: %.2f, %.2f, %.2f", position.x, position.y, position.z);
	ImGui::Text("Character Facing: %.2f", glm::degrees(characterFacing));
	ImGui::Separator();
	ImGui::Text("Forward: %s", glm::to_string(forwardVector).c_str());
	ImGui::Separator();
	ImGui::Text("Camera Angles: %.2f, %.2f", glm::degrees(viewAzimuth), glm::degrees(viewPolar));
	ImGui::Text("Camera Position: %s", glm::to_string(viewCamera.eye).c_str());
	ImGui::Separator();
	ImGui::Text("Forward, Right: %.2f, %.2f", input.getAxis("forward"), input.getAxis("right"));
	ImGui::Separator();
	ImGui::Text("CurrentAnimation: %s; NextAnimation: %s", animControl.getCurrentAnimation().c_str(),
		animControl.getNextAnimation().c_str());
	ImGui::Text("Transition Elapsed: %.2f; Transition Duration: %.2f", animControl.getTrasitionElapsed(), animControl.getTrasitionDuration());
	ImGui::Text("Animation Velocity: %s", glm::to_string(animControl.getVelocity()).c_str());
	ImGui::Text("Animation Speed: %.2f", glm::length(animControl.getVelocity()));

	ImGui::End();
}

CharacterController::CHARACTER_STATE CharacterController::idle(float dT)
{
	//if (input.getAction("jump"))
	//{
	//	return TURN;
	//}

	if (input.getAxis("forward") != 0.0f || input.getAxis("right") != 0.0f)
	{
		return WALK;
	}

	animControl.selectAnimation("idle");

	return IDLE;
}

CharacterController::CHARACTER_STATE CharacterController::turn(float dt)
{
	auto& turnLeft = animControl.getAnimation("turn_left");

	if (turnLeft.elapsed >= turnLeft.duration && animControl.getCurrentAnimation() == "turn_left")
	{
		return IDLE;
	}

	float targetFacing = viewAzimuth + glm::pi<float>();
	float difference = (targetFacing - characterFacing);
	float speed = (targetFacing - characterFacing) / (turnLeft.duration - turnLeft.elapsed);

	animControl.selectAnimation("turn_left", 0.2f, false);

	if (std::abs(difference) > glm::quarter_pi<float>())
		characterFacing += speed * dt;

	return TURN;
}

CharacterController::CHARACTER_STATE CharacterController::hop(float dt)
{
	auto& hopAnim = animControl.getAnimation("hop");

	if (hopAnim.elapsed >= hopAnim.duration)
	{
		return IDLE;
	}
	
	animControl.selectAnimation("hop", 0.2f, false);

	return HOP;
}

CharacterController::CHARACTER_STATE CharacterController::walk(float dt)
{
	if (input.getAction("jump"))
	{
		return HOP;
	}

	if (input.getAxis("forward") == 0.0f && input.getAxis("right") == 0.0f)
	{
		return IDLE;
	}

	//if (input.getAction("sprint"))
	//{
	//	return SPRINT;
	//}

	glm::vec2 axes = { input.getAxis("forward"), input.getAxis("right") };
	glm::vec2 normalizedAxes = glm::normalize(axes);

	glm::vec2 iAxes = { axes.x >= 0 ? glm::ceil(axes.x) : glm::floor(axes.x),
						axes.y >= 0 ? glm::ceil(axes.y) : glm::floor(axes.y) };

	/*std::string anim = (axes.y >= 0.0f) ? "move_right" : "move_left";

	if (std::abs(axes.y) == 1.0f && axes.x == 0.0f) anim = axes.y > 0 ? "strafe_right" : "strafe_left";
	if (std::abs(axes.x) == 1.0f && axes.y == 0.0f) anim = "walk";

	animControl.selectAnimation(anim);
	if (anim == "move_right" || anim == "move_left") animControl.getBlend(anim).blendFactor = std::abs(normalizedAxes.y);*/

	animControl.selectAnimation("walk");

	/*theta = atan2(
		right_axis * sin(view_azimuth) + forward_axis * cos(view_azimuth),
		right_axis * cos(view_azimuth) - forward_axis * sin(view_azimuth)
	)*/

	characterFacing = glm::atan(
		axes.y * glm::sin(viewAzimuth) + axes.x * glm::cos(viewAzimuth),
		axes.y * glm::cos(viewAzimuth) - axes.x * glm::sin(viewAzimuth)
	) + glm::half_pi<float>();

	return WALK;
}

CharacterController::CHARACTER_STATE CharacterController::sprint(float dt)
{
	if (input.getAction("jump"))
	{
		return HOP;
	}

	if (input.getAxis("forward") == 0.0f && input.getAxis("right") == 0.0f)
	{
		return IDLE;
	}

	if (!input.getAction("sprint"))
	{
		return WALK;
	}

	glm::vec2 axes = { input.getAxis("forward"), input.getAxis("right") };
	glm::vec2 normalizedAxes = glm::normalize(axes);

	std::string anim = (axes.y >= 0.0f) ? "sprint_right" : "sprint_left";

	if (std::abs(axes.y) == 1.0f && axes.x == 0.0f) anim = axes.y > 0 ? "strafe_run_right" : "strafe_run_left";
	if (std::abs(axes.x) == 1.0f && axes.y == 0.0f) anim = "run";

	animControl.selectAnimation(anim);
	if (anim == "sprint_right" || anim == "sprint_left") animControl.getBlend(anim).blendFactor = std::abs(normalizedAxes.y);

	characterFacing = viewAzimuth + glm::pi<float>();
	//(glm::quarter_pi<float>() * axes.y * (axes.x > 0 ? -1 : 1));

	return WALK;
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
