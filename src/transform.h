#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

// my ambitions for this transform system:
// -- complete type safety, transforming between spaces can ONLY be done by transforms which change the type of the
// vector
// -- yeah that's basically it...

// pitch (x-axis): looking up/down
//   positive = looking down
//   negative = looking up
// glm::vec3(30, 0, 0) // looking down 30°

// yaw (y-axis): turning left/right
//   positive = turning left (counter-clockwise from above)
//   negative = turning right
// glm::vec3(0, 90, 0) // facing left/west

// roll (z-axis): tilting/banking
//   positive = rolling counter-clockwise
//   negative = rolling clockwise
// glm::vec3(0, 0, 45) // banking/tilting 45°

// common directions (assuming y-up, z-forward, x-right):
// glm::vec3(0, 0, 0)   // facing forward (north)
// glm::vec3(0, 90, 0)  // facing left (west)
// glm::vec3(0, 180, 0) // facing backward (south)
// glm::vec3(0, -90, 0) // facing right (east)

// Space tags :| oh god i'm playing with fire
struct LocalSpace
{
};

struct NodeSpace
{
};

struct ModelSpace
{
};

struct WorldSpace
{
};

struct ViewSpace
{
};

struct ClipSpace
{
};

template <typename Space>
class Point3;

template <typename Space>
class Vec3;

template <typename Space>
class Normal3;

template <typename Space>
class Point3
{
      private:
        glm::vec3 v{};

      public:
        Point3() = default;

        Point3(const glm::vec3& vec) : v(vec)
        {
        }

        Point3(float x, float y, float z) : v(x, y, z)
        {
        }

        const glm::vec3& getv() const
        {
                return v;
        }

        float distance(const Point3<Space>& other) const
        {
                return glm::distance(v, other.getv());
        }

        Vec3<Space> operator-(const Point3<Space>& other) const
        {
                return Vec3<Space>(v - other.getv());
        }
};

template <typename Space>
class Vec3
{
      private:
        glm::vec3 v{};

      public:
        Vec3() = default;

        Vec3(const glm::vec3& vec) : v(vec)
        {
        }

        Vec3(float x, float y, float z) : v(x, y, z)
        {
        }

        const glm::vec3& getv() const
        {
                return v;
        }

        Vec3<Space> operator+(const Vec3<Space>& other) const
        {
                return Vec3<Space>(v + other.getv());
        }

        Vec3<Space> operator-(const Vec3<Space>& other) const
        {
                return Vec3<Space>(v - other.getv());
        }

        Vec3<Space> operator*(float s) const
        {
                return Vec3<Space>(v * s);
        }

        Vec3<Space> operator/(float s) const
        {
                return Vec3<Space>(v / s);
        }

        float length() const
        {
                return glm::length(v);
        }

        float lengthSquared() const
        {
                return glm::dot(v, v);
        }

        Vec3<Space> normalized() const
        {
                return Vec3<Space>(glm::normalize(v));
        }

        float dot(const Vec3<Space>& other) const
        {
                return glm::dot(v, other.v);
        }

        Vec3<Space> cross(const Vec3<Space>& other) const
        {
                return Vec3<Space>(glm::cross(v, other.getv()));
        }
};

template <typename Space>
Point3<Space> operator+(const Point3<Space>& p, const Vec3<Space>& v)
{
        return Point3<Space>(p.getv() + v.getv());
}

template <typename Space>
Point3<Space> operator+(const Vec3<Space>& v, const Point3<Space>& p)
{
        return Point3<Space>(p.getv() + v.getv());
}

// point minus vector is a point
template <typename Space>
Point3<Space> operator-(const Point3<Space>& p, const Vec3<Space>& v)
{
        return Point3<Space>(p.getv() - v.getv());
}

// commutative multiplacation with scalars
template <typename Space>
Vec3<Space> operator*(float s, const Vec3<Space>& v)
{
        return v * s;
}

template <typename Space>
class Normal3
{
      private:
        glm::vec3 v{};

      public:
        Normal3() : v(0.0f, 1.0f, 0.0f)
        {
        }

        Normal3(float x, float y, float z) : v(x, y, z)
        {
        }

        Normal3(const glm::vec3& vec) : v(glm::normalize(vec))
        {
        }

        const glm::vec3& getv() const
        {
                return v;
        }
};

template <typename From, typename To>
class Transform
{
      public:
        Transform() = delete;

        Transform(const glm::mat4& m) : matrix(m)
        {
                normalMatrix = glm::inverse(glm::transpose(glm::mat3(m)));
        }

        static Transform<From, To> identity()
        {
                return Transform<From, To>(glm::mat4(1.0f));
        }

        static Transform<From, To> fromTRS(const glm::vec3& translation, const glm::quat& rotation,
                                           const glm::vec3& scale)
        {
                glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
                glm::mat4 R = glm::toMat4(rotation);
                glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
                return Transform<From, To>(T * R * S);
        }

        static Transform<From, To> fromTRSEuler(const glm::vec3& translation, const glm::vec3& eulerAngles,
                                                const glm::vec3& scale)
        {
                glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
                glm::mat4 R = glm::yawPitchRoll(eulerAngles.y, eulerAngles.x, eulerAngles.z);
                glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
                return Transform<From, To>(T * R * S);
        }

        static Transform<From, To> fromTRSEulerDegrees(const glm::vec3& translation, const glm::vec3& eulerAngles,
                                                       const glm::vec3& scale)
        {
                const auto radians = glm::radians(eulerAngles);

                glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
                glm::mat4 R = glm::yawPitchRoll(radians.y, radians.x, radians.z);
                glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
                return Transform<From, To>(T * R * S);
        }

        static Transform fromEuler(const glm::vec3& eulerAngles)
        {
                return fromTRSEuler(glm::vec3(0.0f), eulerAngles, glm::vec3(1.0f));
        }

        static Transform fromEulerDegrees(const glm::vec3& eulerDegrees)
        {
                return fromTRSEulerDegrees(glm::vec3(0.0f), eulerDegrees, glm::vec3(1.0f));
        }

        static Transform fromTranslation(const glm::vec3& translation)
        {
                return fromTRS(translation, glm::quat(1, 0, 0, 0), glm::vec3(1.0f));
        }

        static Transform fromScale(const glm::vec3& scale)
        {
                return fromTRS(glm::vec3(0.0f), glm::quat(1, 0, 0, 0), scale);
        }

        Point3<To> transformPoint(const Point3<From>& point) const
        {
                glm::vec4 p = matrix * glm::vec4(point.getv(), 1.0f);
                return Point3<To>(glm::vec3(p) / p.w);
        }

        Vec3<To> transformDirection(const Vec3<From>& dir) const
        {
                glm::vec4 d = matrix * glm::vec4(dir.getv(), 0.0f);
                return Vec3<To>(glm::vec3(d));
        }

        Normal3<To> transformNormal(const Normal3<From>& normal) const
        {
                glm::vec3 d = normalMatrix * normal.getv();
                return Normal3<To>(d);
        }

        template <typename Next>
        Transform<From, Next> then(const Transform<To, Next>& next) const
        {
                return Transform<From, Next>(next.getMatrix() * matrix);
        }

        Transform<To, From> inverse() const
        {
                return Transform<To, From>(glm::inverse(matrix));
        }

        const glm::mat4& getMatrix() const
        {
                return matrix;
        }

        const glm::mat3& getNormalMatrix() const
        {
                return normalMatrix;
        }

        glm::vec3 getTranslation() const
        {
                return glm::vec3(matrix[3]);
        }

        glm::quat getRotation() const
        {
                glm::mat3 rotMat(matrix);
                // Remove scale
                rotMat[0] = glm::normalize(rotMat[0]);
                rotMat[1] = glm::normalize(rotMat[1]);
                rotMat[2] = glm::normalize(rotMat[2]);
                return glm::quat_cast(rotMat);
        }

        glm::vec3 getScale() const
        {
                return glm::vec3(glm::length(glm::vec3(matrix[0])), glm::length(glm::vec3(matrix[1])),
                                 glm::length(glm::vec3(matrix[2])));
        }

      private:
        glm::mat4 matrix;
        glm::mat3 normalMatrix;
};

using LocalToModel = Transform<LocalSpace, ModelSpace>;
using ModelToWorld = Transform<ModelSpace, WorldSpace>;
using LocalToWorld = Transform<LocalSpace, WorldSpace>;
using WorldToView = Transform<WorldSpace, ViewSpace>;
using ViewToClip = Transform<ViewSpace, ClipSpace>;
using WorldToClip = Transform<WorldSpace, ClipSpace>;
