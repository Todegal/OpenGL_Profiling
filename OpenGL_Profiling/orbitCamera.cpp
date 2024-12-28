#include "orbitCamera.h"

OrbitCamera::OrbitCamera(const glm::vec3& center, const glm::vec3& upVector, float radius, float minRadius, float azimuthAngle, float polarAngle)
	: center(center), upVector(upVector), radius(radius), minRadius(minRadius), azimuthAngle(azimuthAngle), polarAngle(polarAngle)
{
}

void OrbitCamera::rotateAzimuth(const float radians)
{
	azimuthAngle += radians;

	// Keep azimuth angle within range <0..2PI) - it's not necessary, just to have it nicely output
	constexpr auto fullCircle = 2.0f * glm::pi<float>();
	azimuthAngle = fmodf(azimuthAngle, fullCircle);
	if (azimuthAngle < 0.0f) {
		azimuthAngle = fullCircle + azimuthAngle;
	}
}

void OrbitCamera::rotatePolar(const float radians)
{
	polarAngle += radians;

	// Check if the angle hasn't exceeded quarter of a circle to prevent flip, add a bit of epsilon like 0.001 radians
	constexpr auto polarCap = glm::pi<float>() / 2.0f - 0.001f;
	if (polarAngle > polarCap) {
		polarAngle = polarCap;
	}

	if (polarAngle < -polarCap) {
		polarAngle = -polarCap;
	}
}

void OrbitCamera::zoom(const float delta)
{
	radius -= delta * (radius * 0.1f);
	if (radius < minRadius) {
		radius = minRadius;
	}
}

void OrbitCamera::moveHorizontal(const float distance)
{
	const auto position = getEye();
	const glm::vec3 viewVector = getNormalizedViewVector();
	const glm::vec3 strafeVector = glm::normalize(glm::cross(viewVector, upVector));
	center += strafeVector * distance * radius;
}

void OrbitCamera::moveVertical(const float distance)
{
	const auto position = getEye();
	const glm::vec3 viewVector = getNormalizedViewVector();
	const glm::vec3 strafeVector = glm::normalize(glm::cross(viewVector, upVector));
	const glm::vec3 relativeUp = glm::normalize(glm::cross(strafeVector, viewVector));
	center += relativeUp * distance * radius;
}

const glm::mat4 OrbitCamera::getViewMatrix() const
{
	return glm::lookAt(getEye(), center, upVector);
}

const glm::vec3 OrbitCamera::getEye() const
{
	// Calculate sines / cosines of angles
	const auto sineAzimuth = sin(azimuthAngle);
	const auto cosineAzimuth = cos(azimuthAngle);
	const auto sinePolar = sin(polarAngle);
	const auto cosinePolar = cos(polarAngle);

	// Calculate eye position out of them
	const auto x = center.x + radius * cosinePolar * cosineAzimuth;
	const auto y = center.y + radius * sinePolar;
	const auto z = center.z + radius * cosinePolar * sineAzimuth;

	return glm::vec3(x, y, z);
}
