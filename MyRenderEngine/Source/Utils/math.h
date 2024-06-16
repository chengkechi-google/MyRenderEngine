#pragma once
#include "assert.h"
#include "linalg/linalg.h"

using namespace linalg;
using namespace linalg::aliases;

using uint = uint32_t;
using quaternion = float4;

static const float M_PI = 3.14159265f;

template<class T>
inline T degree_to_radian(T degree)
{
    return degree * M_PI / 180.0f;
}

template<class T>
inline T radian_to_degree(T radian)
{
    return radian * 180.0f / M_PI;
}

inline quaternion rotation_quat(const float3& eulerAngles) // Pitch-yaw-row order, in degrees
{
    float3 radians = degree_to_radian(eulerAngles);

    float3 c = cos(radians * 0.5f);
    float3 s = sin(radians * 0.5f);

    quaternion q;
    q.w = c.x * c.y * c.z + s.x * s.y * s.z;
    q.x = s.x * c.y * c.z - c.x * s.y * s.z;
    q.y = c.x * s.y * c.z + s.x * c.y * s.z;
    q.z = c.x * c.y * s.z - s.x * s.y * c.z;
    
    return q;
}

inline float rotation_pitch(const quaternion& q)
{
    const float y = 2.0f * (q.y * q.z + q.w * q.x);
    const float x = q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z;
    if (y == 0.0f && x == 0.0f) //avoid atan2(0,0) - handle singularity
    {
        return radian_to_degree(2.0f * atan2(q.x, q.w));
    }

    return radian_to_degree(atan2(y, x));
}

inline float rotation_yaw(const quaternion& q)
{
    return radian_to_degree(asin(clamp(-2.0f * (q.x * q.z - q.w * q.y), -1.0f, 1.0f)));
}

inline float rotation_roll(const quaternion& q)
{
    return radian_to_degree(atan2(2.0f * (q.x * q.y + q.w * q.z), q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z));
}

inline float3 roatation_angles(const quaternion& q)
{
    return float3(rotation_pitch(q), rotation_yaw(q), rotation_roll(q));
}



inline float sign(float x)
{
    return float((x > 0) - (x < 0));
}

inline bool nearly_equal(float a, float b)
{
    return std::abs(a - b) < FLT_EPSILON; 
}

inline float4 normalize_plane(const float4& plane)
{
    float length = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
    return plane * (1.0f / length);
}

template<class T>
inline bool nearly_equal(const T& a, const T& b)
{
    return nearly_equal(a.x, b.x) && nearly_equal(a.y, b.y) && nearly_equal(a.z, b.z) && nearly_equal(a.w, b.w);
}

inline uint32_t DivideRoundingUp(uint32_t a, uint32_t b)
{
    return (a + b - 1) / b;
}

inline bool IsPow2(uint32_t x)
{
    return (x & (x - 1)) == 0;
}

// It is round up to b, and b is power of 2
inline uint32_t RoundUpPow2(uint32_t a, uint32_t b)
{
    MY_ASSERT(IsPow2(b));
    return (a + b - 1) & ~(b - 1);
}