#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"

class OrbitCamera : public Camera
{
      public:
        OrbitCamera(const Point3<WorldSpace>& center, const Vec3<WorldSpace>& upVector, float radius, float minRadius,
                    float azimuthAngle = 0.0f, float polarAngle = 0.0f);

        void rotateAzimuth(const float radians);
        void rotatePolar(const float radians);
        void zoom(const float by);

        void moveHorizontal(const float distance);
        void moveVertical(const float distance);

        virtual const WorldToView getViewTransform() const override;
        virtual const Point3<WorldSpace> getEye() const override;

        const Point3<WorldSpace> getViewPoint() const
        {
                return center;
        }

        const Vec3<WorldSpace> getUpVector() const
        {
                return upVector;
        }

        const Vec3<WorldSpace> getNormalizedViewVector() const
        {
                //return glm::normalize(center - getEye());
                return (center - getEye()).normalized();
        }

        float getAzimuthAngle() const
        {
                return azimuthAngle;
        }

        float getPolarAngle() const
        {
                return polarAngle;
        }

        float getRadius() const
        {
                return radius;
        }

        void setViewPoint(Point3<WorldSpace> view)
        {
                center = view;
        }

      private:
        Point3<WorldSpace> center;   // Center of the orbit camera sphere (the point upon which the camera looks)
        Vec3<WorldSpace> upVector; // Up vector of the camera

        float radius;       // Radius of the orbit camera sphere
        float minRadius;    // Minimal radius of the orbit camera sphere (cannot fall below this value)
        float azimuthAngle; // Azimuth angle on the orbit camera sphere
        float polarAngle;   // Polar angle on the orbit camera sphere
};
