#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"

class OrbitCamera : public Camera
{
public:
    OrbitCamera(
        const glm::vec3& center, 
        const glm::vec3& upVector, 
        float radius, float minRadius, 
        float azimuthAngle = 0.0f, float polarAngle = 0.0f
    );

    void rotateAzimuth(const float radians);
    void rotatePolar(const float radians);
    void zoom(const float by);

    void moveHorizontal(const float distance);
    void moveVertical(const float distance);

    virtual const glm::mat4 getViewMatrix() const;
    virtual const glm::vec3 getEye() const;

    const glm::vec3 getViewPoint() const { return center; }
    const glm::vec3 getUpVector() const { return upVector; }
    const glm::vec3 getNormalizedViewVector() const { return glm::normalize(center - getEye()); }
    float getAzimuthAngle() const { return azimuthAngle; }
    float getPolarAngle() const { return polarAngle; }
    float getRadius() const { return radius; }

    void setViewPoint(glm::vec3 view) { center = view; }

private:
    glm::vec3 center; // Center of the orbit camera sphere (the point upon which the camera looks)
    glm::vec3 upVector; // Up vector of the camera
    float radius; // Radius of the orbit camera sphere
    float minRadius; // Minimal radius of the orbit camera sphere (cannot fall below this value)
    float azimuthAngle; // Azimuth angle on the orbit camera sphere
    float polarAngle; // Polar angle on the orbit camera sphere
};