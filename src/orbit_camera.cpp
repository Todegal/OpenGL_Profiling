#include "orbit_camera.h"

OrbitCamera::OrbitCamera(const Point3<WorldSpace>& center, const Vec3<WorldSpace>& upVector, float radius, float minRadius,
                         float azimuthAngle, float polarAngle)
    : center(center), upVector(upVector), radius(radius), minRadius(minRadius), azimuthAngle(azimuthAngle),
      polarAngle(polarAngle)
{
}

void OrbitCamera::rotateAzimuth(const float radians)
{
        azimuthAngle += radians;

        // Keep azimuth angle within range <0..2PI) - it's not necessary, just to have it nicely output
        constexpr auto fullCircle = 2.0f * glm::pi<float>();
        azimuthAngle = fmodf(azimuthAngle, fullCircle);
        if (azimuthAngle < 0.0f) { azimuthAngle = fullCircle + azimuthAngle; }
}

void OrbitCamera::rotatePolar(const float radians)
{
        polarAngle += radians;

        // Check if the angle hasn't exceeded quarter of a circle to prevent flip, add a bit of epsilon like 0.001
        // radians
        constexpr auto polarCap = glm::pi<float>() / 2.0f - 0.001f;
        if (polarAngle > polarCap) { polarAngle = polarCap; }

        if (polarAngle < -polarCap) { polarAngle = -polarCap; }
}

void OrbitCamera::zoom(const float delta)
{
        radius -= delta * (radius * 0.1f);
        if (radius < minRadius) { radius = minRadius; }
}

void OrbitCamera::moveHorizontal(const float distance)
{
        const auto viewVector = getNormalizedViewVector();
        const auto strafeVector = viewVector.cross(upVector).normalized();
        center = center + (strafeVector * distance * radius);
}

void OrbitCamera::moveVertical(const float distance)
{
        const auto viewVector = getNormalizedViewVector();
        const auto strafeVector = viewVector.cross(upVector).normalized();
        const auto relativeUp = strafeVector.cross(viewVector).normalized();
        center = center + (relativeUp * distance * radius);
}

const WorldToView OrbitCamera::getViewTransform() const
{
        return WorldToView(glm::lookAt(getEye().getv(), center.getv(), upVector.getv()));
}

const Point3<WorldSpace> OrbitCamera::getEye() const
{
        // Calculate sines / cosines of angles
        const auto sineAzimuth = sin(azimuthAngle);
        const auto cosineAzimuth = cos(azimuthAngle);
        const auto sinePolar = sin(polarAngle);
        const auto cosinePolar = cos(polarAngle);

        const auto c = center.getv();

        // Calculate eye position out of them
        const auto x = c.x + (radius * cosinePolar * cosineAzimuth);
        const auto y = c.y + (radius * sinePolar);
        const auto z = c.z + (radius * cosinePolar * sineAzimuth);

        return Point3<WorldSpace>(x, y, z);
}
