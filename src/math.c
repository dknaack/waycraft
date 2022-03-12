#include <math.h>

#include "types.h"
#include "math.h"

static float
lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

/*
 * vec2 function definitions
 */

vec2
vec2_add(vec2 a, vec2 b)
{
    return VEC2(a.x + b.x, a.y + b.y);
}

vec2
vec2_sub(vec2 a, vec2 b)
{
    return VEC2(a.x - b.x, a.y - b.y);
}

vec2
vec2_mul(vec2 a, vec2 b)
{
    return VEC2(a.x * b.x, a.y * b.y);
}

vec2
vec2_div(vec2 a, vec2 b)
{
    return VEC2(a.x / b.x, a.y / b.y);
}

vec2
vec2_mulf(vec2 a, float f)
{
    return VEC2(a.x * f, a.y * f);
}

vec2
vec2_divf(vec2 a, float f)
{
    return VEC2(a.x / f, a.y / f);
}

vec2
vec2_norm(vec2 a)
{
    return vec2_divf(a, vec2_len(a));
}

float
vec2_len(vec2 a)
{
    return sqrtf(a.x * a.x + a.y * a.y);
}

float
vec2_dot(vec2 a, vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

/*
 * vec3 function definitions
 */

vec3
vec3_add(vec3 a, vec3 b)
{
    return VEC3(a.x + b.x,
                a.y + b.y,
                a.z + b.z);
}

vec3
vec3_sub(vec3 a, vec3 b)
{
    return VEC3(a.x - b.x,
                a.y - b.y,
                a.z - b.z);
}

vec3
vec3_mul(vec3 a, vec3 b)
{
    return VEC3(a.x * b.x,
                a.y * b.y,
                a.z * b.z);
}

vec3
vec3_div(vec3 a, vec3 b)
{
    return VEC3(a.x / b.x, a.y / b.y, a.z / b.z);
}

vec3
vec3_modf(vec3 a, float f)
{
    return VEC3(fmodf(a.x, f), fmodf(a.y, f), fmodf(a.z, f));
}

vec3
vec3_mulf(vec3 a, float f)
{
    return VEC3(a.x * f,
                a.y * f,
                a.z * f);
}

vec3
vec3_divf(vec3 a, float f)
{
    return VEC3(a.x / f, a.y / f, a.z / f);
}

float
vec3_len(vec3 a)
{
    return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

float
vec3_dot(vec3 a, vec3 b)
{
    return (a.x * b.x +
            a.y * b.y +
            a.z * b.z);
}

vec3
vec3_cross(vec3 a, vec3 b)
{
    return VEC3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

vec3
vec3_norm(vec3 a)
{
    return vec3_divf(a, vec3_len(a));
}

vec3
vec3_floor(vec3 a)
{
    return VEC3(floorf(a.x), floorf(a.y), floorf(a.z));
}

vec3
vec3_lerp(vec3 a, vec3 b, float t)
{
    return VEC3(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t));
}

/*
 * vec4 function definitions
 */

vec4
vec4_add(vec4 a, vec4 b)
{
    return VEC4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

vec4
vec4_sub(vec4 a, vec4 b)
{
    return VEC4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

vec4
vec4_mul(vec4 a, vec4 b)
{
    return VEC4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

vec4
vec4_div(vec4 a, vec4 b)
{
    return VEC4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}

vec4
vec4_mulf(vec4 a, float f)
{
    return VEC4(a.x * f, a.y * f, a.z * f, a.w * f);
}

vec4
vec4_divf(vec4 a, float f)
{
    return VEC4(a.x / f, a.y / f, a.z / f, a.w / f);
}

vec4
vec4_norm(vec4 a)
{
    return vec4_divf(a, vec4_len(a));
}

float
vec4_len(vec4 a)
{
    return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w);
}

float
vec4_dot(vec4 a, vec4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * a.w;
}

/*
 * mat4 function definitions
 */

mat4
mat4_add(mat4 a, mat4 b)
{
    unsigned int i;

    for (i = 0; i < 16; i++)
        a.e[i] += b.e[i];
    return a;
}

mat4
mat4_sub(mat4 a, mat4 b)
{
    unsigned int i;

    for (i = 0; i < 16; i++)
        a.e[i] -= b.e[i];

    return a;
}

mat4
mat4_mul(mat4 a, mat4 b)
{
    unsigned int i, j, k;
    mat4 result = {0};

    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            for (k = 0; k < 4; k++)
                result.e[i + 4 * j] += a.e[i + 4 * k] * b.e[k + 4 * j];
    return result;
}

mat4
mat4_mulf(mat4 a, float f)
{
    unsigned int i;

    for (i = 0; i < 16; i++)
        a.e[i] *= f;
    return a;
}

mat4
mat4_transpose(mat4 a)
{
    return MAT4(
        a.e[0], a.e[4], a.e[ 8], a.e[12],
        a.e[1], a.e[5], a.e[ 9], a.e[13],
        a.e[2], a.e[6], a.e[10], a.e[14],
        a.e[3], a.e[7], a.e[11], a.e[15]);
}

mat4
mat4_id(float f)
{
    return MAT4(
        f, 0, 0, 0,
        0, f, 0, 0,
        0, 0, f, 0,
        0, 0, 0, 1);
}

mat4
mat4_translate(float x, float y, float z)
{
    return MAT4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        x, y, z, 1);
}

mat4
mat4_scale(float x, float y, float z)
{
    return MAT4(
        x, 0, 0, 0,
        0, y, 0, 0,
        0, 0, z, 0,
        0, 0, 0, 1);
}

mat4
mat4_perspective(float fov, float aspect, float near, float far)
{
    mat4 result = {0};

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

mat4
mat4_look_at(vec3 eye, vec3 target, vec3 up)
{
    mat4 result = {0};

    vec3 z = vec3_norm(vec3_sub(eye, target));
    vec3 x = vec3_norm(vec3_cross(up, z));
    vec3 y = vec3_cross(z, x);

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

    return mat4_mul(result, mat4_translate(-eye.x, -eye.y, -eye.z));
}

mat4
mat4_rotate(vec3 axis, float angle)
{
    vec3 a = vec3_norm(axis);
    float cos = cosf(angle);
    float sin = sinf(angle);

    mat4 m1 = mat4_scale(cos, cos, cos);

    mat4 m2 = MAT4(
            a.x * a.x, a.x * a.y, a.x * a.z, 0,
            a.y * a.x, a.y * a.y, a.y * a.z, 0,
            a.z * a.x, a.z * a.y, a.z * a.z, 0,
            0,         0,         0,         0);

    mat4 m3 = MAT4(
               0, -a.z,  a.y, 0,
             a.z,    0, -a.x, 0,
            -a.y,  a.x,    0, 0,
               0,    0,    0, 0);

    return mat4_add(m1, mat4_add(mat4_mulf(m2, 1.f - cos), mat4_mulf(m3, sin)));
}

ivec3
ivec3_vec3(vec3 a)
{
    ivec3 result;

    result.x = a.x;
    result.y = a.y;
    result.z = a.z;

    return result;
}
