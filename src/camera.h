#pragma once

#include <glm/glm.hpp>

#include "transform.h"

class Camera
{
      public:
        virtual const WorldToView getViewTransform() const = 0;
        virtual const Point3<WorldSpace> getEye() const = 0;

      public:
        float getFov() const
        {
                return fov;
        }

      private:
        float fov = 90.0f;
};
