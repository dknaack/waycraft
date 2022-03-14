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

f32  vec3_len(vec3 a);
f32  vec3_dot(vec3 a, vec3 b);
vec3 vec3_add(vec3 a, vec3 b);
vec3 vec3_sub(vec3 a, vec3 b);
vec3 vec3_mul(vec3 a, vec3 b);
vec3 vec3_div(vec3 a, vec3 b);
vec3 vec3_cross(vec3 a, vec3 b);
vec3 vec3_mulf(vec3 a, float f);
vec3 vec3_norm(vec3 a);
vec3 vec3_floor(vec3 a);
vec3 vec3_round(vec3 a);
vec3 vec3_neg(vec3 a);
vec3 vec3_lerp(vec3 a, vec3 b, float t);

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
mat4 mat4_rotate(vec3 axis, float angle);
mat4 mat4_look_at(vec3 eye, vec3 target, vec3 up);
mat4 mat4_add(mat4 a, mat4 b);
mat4 mat4_sub(mat4 a, mat4 b);
mat4 mat4_mulf(mat4 a, float f);
mat4 mat4_mul(mat4 a, mat4 b);
mat4 mat4_perspective(float fov, float aspect, float near, float far);

ivec3 ivec3_vec3(vec3 a);

#endif /* MATH_H */ 
