#ifndef MATH_H
#define MATH_H

#include "types.h"

f32  vec2_len(vec2 a);
f32  vec2_dot(vec2 a, vec2 b);
vec2 vec2_add(vec2 a, vec2 b);
vec2 vec2_sub(vec2 a, vec2 b);
vec2 vec2_mul(vec2 a, vec2 b);
vec2 vec2_div(vec2 a, vec2 b);
vec2 vec2_mulf(vec2 a, float f);
vec2 vec2_norm(vec2 a);

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

f32  vec4_len(vec4 v);
f32  vec4_dot(vec4 a, vec4 b);
vec4 vec4_add(vec4 a, vec4 b);
vec4 vec4_sub(vec4 a, vec4 b);
vec4 vec4_mul(vec4 a, vec4 b);
vec4 vec4_div(vec4 a, vec4 b);
vec4 vec4_mulf(vec4 a, float f);
vec4 vec4_norm(vec4 a);

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
