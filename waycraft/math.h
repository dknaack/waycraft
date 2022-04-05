#ifndef MATH_H
#define MATH_H

#include <waycraft/types.h>

f32 v2_len(v2 a);
f32 v2_dot(v2 a, v2 b);
v2 v2_add(v2 a, v2 b);
v2 v2_sub(v2 a, v2 b);
v2 v2_mul(v2 a, v2 b);
v2 v2_div(v2 a, v2 b);
v2 v2_mulf(v2 a, f32 f);
v2 v2_norm(v2 a);

f32 v3_len(v3 a);
f32 v3_dot(v3 a, v3 b);
v3 v3_add(v3 a, v3 b);
v3 v3_sub(v3 a, v3 b);
v3 v3_mul(v3 a, v3 b);
v3 v3_div(v3 a, v3 b);
v3 v3_cross(v3 a, v3 b);
v3 v3_mulf(v3 a, f32 f);
v3 v3_norm(v3 a);
v3 v3_floor(v3 a);
v3 v3_round(v3 a);
v3 v3_neg(v3 a);
v3 v3_lerp(v3 a, v3 b, f32 t);
v3 v3_abs(v3 v);

f32 v4_len(v4 v);
f32 v4_dot(v4 a, v4 b);
v4 v4_add(v4 a, v4 b);
v4 v4_sub(v4 a, v4 b);
v4 v4_mul(v4 a, v4 b);
v4 v4_div(v4 a, v4 b);
v4 v4_mulf(v4 a, f32 f);
v4 v4_norm(v4 a);

m4x4 m4x4_id(f32 f);
m4x4 m4x4_scale(f32 x, f32 y, f32 z);
m4x4 m4x4_translate(f32 x, f32 y, f32 z);
m4x4 m4x4_rotate(v3 axis, f32 angle);
m4x4 m4x4_look_at(v3 eye, v3 target, v3 up);
m4x4 m4x4_add(m4x4 a, m4x4 b);
m4x4 m4x4_sub(m4x4 a, m4x4 b);
m4x4 m4x4_mulf(m4x4 a, f32 f);
m4x4 m4x4_mul(m4x4 a, m4x4 b);
m4x4 m4x4_perspective(f32 fov, f32 aspect, f32 near, f32 far);
m4x4 m4x4_to_coords(v3 pos, v3 right, v3 up, v3 forward);
v4 m4x4_mulv(m4x4 m, v4 v);

v3i v3i_vec3(v3 a);

#endif /* MATH_H */
