#pragma once

#include <glm/glm.hpp>

class Camera
{
public:
    virtual const glm::mat4 getViewMatrix() const = 0;
    virtual const glm::vec3 getEye() const = 0;

public:
    const float getFov() const { return fov; }

private:
    float fov = 90.0f;
};