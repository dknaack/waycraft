#include <math.h>

#include "types.h"
#include "math.h"

static float
lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

/*
 * v2 function definitions
 */

v2
v2_add(v2 a, v2 b)
{
    return V2(a.x + b.x, a.y + b.y);
}

v2
v2_sub(v2 a, v2 b)
{
    return V2(a.x - b.x, a.y - b.y);
}

v2
v2_mul(v2 a, v2 b)
{
    return V2(a.x * b.x, a.y * b.y);
}

v2
v2_div(v2 a, v2 b)
{
    return V2(a.x / b.x, a.y / b.y);
}

v2
v2_mulf(v2 a, float f)
{
    return V2(a.x * f, a.y * f);
}

v2
v2_divf(v2 a, float f)
{
    return V2(a.x / f, a.y / f);
}

v2
v2_norm(v2 a)
{
    return v2_divf(a, v2_len(a));
}

float
v2_len(v2 a)
{
    return sqrtf(a.x * a.x + a.y * a.y);
}

float
v2_dot(v2 a, v2 b)
{
    return a.x * b.x + a.y * b.y;
}

/*
 * v3 function definitions
 */

v3
v3_add(v3 a, v3 b)
{
    return V3(a.x + b.x,
                a.y + b.y,
                a.z + b.z);
}

v3
v3_sub(v3 a, v3 b)
{
    return V3(a.x - b.x,
                a.y - b.y,
                a.z - b.z);
}

v3
v3_mul(v3 a, v3 b)
{
    return V3(a.x * b.x,
                a.y * b.y,
                a.z * b.z);
}

v3
v3_div(v3 a, v3 b)
{
    return V3(a.x / b.x, a.y / b.y, a.z / b.z);
}

v3
v3_modf(v3 a, float f)
{
    return V3(fmodf(a.x, f), fmodf(a.y, f), fmodf(a.z, f));
}

v3
v3_mulf(v3 a, float f)
{
    return V3(a.x * f,
                a.y * f,
                a.z * f);
}

v3
v3_divf(v3 a, float f)
{
    return V3(a.x / f, a.y / f, a.z / f);
}

float
v3_len(v3 a)
{
    return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

float
v3_dot(v3 a, v3 b)
{
    return (a.x * b.x +
            a.y * b.y +
            a.z * b.z);
}

v3
v3_cross(v3 a, v3 b)
{
    return V3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

v3
v3_norm(v3 a)
{
    return v3_divf(a, v3_len(a));
}

v3
v3_floor(v3 a)
{
    return V3(floorf(a.x), floorf(a.y), floorf(a.z));
}

v3
v3_round(v3 a)
{
    return V3(roundf(a.x), roundf(a.y), roundf(a.z));
}

v3
v3_neg(v3 a)
{
    return V3(-a.x, -a.y, -a.z);
}

v3
v3_lerp(v3 a, v3 b, float t)
{
    return V3(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t));
}

/*
 * v4 function definitions
 */

v4
v4_add(v4 a, v4 b)
{
    return V4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

v4
v4_sub(v4 a, v4 b)
{
    return V4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

v4
v4_mul(v4 a, v4 b)
{
    return V4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

v4
v4_div(v4 a, v4 b)
{
    return V4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}

v4
v4_mulf(v4 a, float f)
{
    return V4(a.x * f, a.y * f, a.z * f, a.w * f);
}

v4
v4_divf(v4 a, float f)
{
    return V4(a.x / f, a.y / f, a.z / f, a.w / f);
}

v4
v4_norm(v4 a)
{
    return v4_divf(a, v4_len(a));
}

float
v4_len(v4 a)
{
    return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w);
}

float
v4_dot(v4 a, v4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * a.w;
}

/*
 * m4x4 function definitions
 */

m4x4
m4x4_add(m4x4 a, m4x4 b)
{
    unsigned int i;

    for (i = 0; i < 16; i++)
        a.e[i] += b.e[i];
    return a;
}

m4x4
m4x4_sub(m4x4 a, m4x4 b)
{
    unsigned int i;

    for (i = 0; i < 16; i++)
        a.e[i] -= b.e[i];

    return a;
}

m4x4
m4x4_mul(m4x4 a, m4x4 b)
{
    unsigned int i, j, k;
    m4x4 result = {0};

    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            for (k = 0; k < 4; k++)
                result.e[i + 4 * j] += a.e[i + 4 * k] * b.e[k + 4 * j];
    return result;
}

m4x4
m4x4_mulf(m4x4 a, float f)
{
    unsigned int i;

    for (i = 0; i < 16; i++)
        a.e[i] *= f;
    return a;
}

m4x4
m4x4_transpose(m4x4 a)
{
    return M4X4(
        a.e[0], a.e[4], a.e[ 8], a.e[12],
        a.e[1], a.e[5], a.e[ 9], a.e[13],
        a.e[2], a.e[6], a.e[10], a.e[14],
        a.e[3], a.e[7], a.e[11], a.e[15]);
}

m4x4
m4x4_id(float f)
{
    return M4X4(
        f, 0, 0, 0,
        0, f, 0, 0,
        0, 0, f, 0,
        0, 0, 0, 1);
}

m4x4
m4x4_translate(float x, float y, float z)
{
    return M4X4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        x, y, z, 1);
}

m4x4
m4x4_scale(float x, float y, float z)
{
    return M4X4(
        x, 0, 0, 0,
        0, y, 0, 0,
        0, 0, z, 0,
        0, 0, 0, 1);
}

m4x4
m4x4_perspective(float fov, float aspect, float near, float far)
{
    m4x4 result = {0};

    float top = near * tanf(fov * PI / 180.f * 0.5f);
    float right = top * aspect;
    float fn = (far - near);

    result.e[0]  = near / right;
    result.e[5]  = near / top;
    result.e[10] = -(far + near) / fn;
    result.e[11] = -1.f;
    result.e[14] = -(2.f * far * near) / fn;

#if MATH_FLIP_Y
    result.e[5] *= -1.f;
#endif

    return result;
}

m4x4
m4x4_look_at(v3 eye, v3 target, v3 up)
{
    m4x4 result = {0};

    v3 z = v3_norm(v3_sub(eye, target));
    v3 x = v3_norm(v3_cross(up, z));
    v3 y = v3_cross(z, x);

    result.e[0]  = x.x;
    result.e[1]  = y.x;
    result.e[2]  = z.x;

    result.e[4]  = x.y;
    result.e[5]  = y.y;
    result.e[6]  = z.y;

    result.e[8]  = x.z;
    result.e[9]  = y.z;
    result.e[10] = z.z;

    result.e[15] = 1.f;

    return m4x4_mul(result, m4x4_translate(-eye.x, -eye.y, -eye.z));
}

m4x4
m4x4_rotate(v3 axis, float angle)
{
    v3 a = v3_norm(axis);
    float cos = cosf(angle);
    float sin = sinf(angle);

    m4x4 m1 = m4x4_scale(cos, cos, cos);

    m4x4 m2 = M4X4(
            a.x * a.x, a.x * a.y, a.x * a.z, 0,
            a.y * a.x, a.y * a.y, a.y * a.z, 0,
            a.z * a.x, a.z * a.y, a.z * a.z, 0,
            0,         0,         0,         0);

    m4x4 m3 = M4X4(
               0, -a.z,  a.y, 0,
             a.z,    0, -a.x, 0,
            -a.y,  a.x,    0, 0,
               0,    0,    0, 0);

    return m4x4_add(m1, m4x4_add(m4x4_mulf(m2, 1.f - cos), m4x4_mulf(m3, sin)));
}

v3i
v3i_vec3(v3 a)
{
    v3i result;

    result.x = a.x;
    result.y = a.y;
    result.z = a.z;

    return result;
}
