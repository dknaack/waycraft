#ifndef MATH_H
#define MATH_H

#include "types.h"

f32  v2_len(v2 a);
f32  v2_dot(v2 a, v2 b);
v2 v2_add(v2 a, v2 b);
v2 v2_sub(v2 a, v2 b);
v2 v2_mul(v2 a, v2 b);
v2 v2_div(v2 a, v2 b);
v2 v2_mulf(v2 a, float f);
v2 v2_norm(v2 a);

f32  v3_len(v3 a);
f32  v3_dot(v3 a, v3 b);
v3 v3_add(v3 a, v3 b);
v3 v3_sub(v3 a, v3 b);
v3 v3_mul(v3 a, v3 b);
v3 v3_div(v3 a, v3 b);
v3 v3_cross(v3 a, v3 b);
v3 v3_mulf(v3 a, float f);
v3 v3_norm(v3 a);
v3 v3_floor(v3 a);
v3 v3_round(v3 a);
v3 v3_neg(v3 a);
v3 v3_lerp(v3 a, v3 b, float t);

f32  v4_len(v4 v);
f32  v4_dot(v4 a, v4 b);
v4 v4_add(v4 a, v4 b);
v4 v4_sub(v4 a, v4 b);
v4 v4_mul(v4 a, v4 b);
v4 v4_div(v4 a, v4 b);
v4 v4_mulf(v4 a, float f);
v4 v4_norm(v4 a);

mat4 mat4_id(float f);
mat4 mat4_scale(float x, float y, float z);
mat4 mat4_translate(float x, float y, float z);
mat4 mat4_rotate(v3 axis, float angle);
mat4 mat4_look_at(v3 eye, v3 target, v3 up);
mat4 mat4_add(mat4 a, mat4 b);
mat4 mat4_sub(mat4 a, mat4 b);
mat4 mat4_mulf(mat4 a, float f);
mat4 mat4_mul(mat4 a, mat4 b);
mat4 mat4_perspective(float fov, float aspect, float near, float far);

v3i v3i_vec3(v3 a);

#endif /* MATH_H */ 
