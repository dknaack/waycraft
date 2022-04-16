static f32 v2_len(v2 a);
static f32 v2_dot(v2 a, v2 b);
static v2 v2_add(v2 a, v2 b);
static v2 v2_sub(v2 a, v2 b);
static v2 v2_mul(v2 a, v2 b);
static v2 v2_div(v2 a, v2 b);
static v2 v2_mulf(v2 a, f32 f);
static v2 v2_norm(v2 a);

static f32 v3_len(v3 a);
static f32 v3_dot(v3 a, v3 b);
static v3 v3_add(v3 a, v3 b);
static v3 v3_sub(v3 a, v3 b);
static v3 v3_mul(v3 a, v3 b);
static v3 v3_div(v3 a, v3 b);
static v3 v3_cross(v3 a, v3 b);
static v3 v3_mulf(v3 a, f32 f);
static v3 v3_norm(v3 a);
static v3 v3_floor(v3 a);
static v3 v3_round(v3 a);
static v3 v3_neg(v3 a);
static v3 v3_lerp(v3 a, v3 b, f32 t);
static v3 v3_abs(v3 v);

static f32 v4_len(v4 v);
static f32 v4_dot(v4 a, v4 b);
static v4 v4_add(v4 a, v4 b);
static v4 v4_sub(v4 a, v4 b);
static v4 v4_mul(v4 a, v4 b);
static v4 v4_div(v4 a, v4 b);
static v4 v4_mulf(v4 a, f32 f);
static v4 v4_norm(v4 a);

static m4x4 m4x4_id(f32 f);
static m4x4 m4x4_scale(f32 x, f32 y, f32 z);
static m4x4 m4x4_translate(f32 x, f32 y, f32 z);
static m4x4 m4x4_rotate(v3 axis, f32 angle);
static m4x4 m4x4_look_at(v3 eye, v3 target, v3 up);
static m4x4 m4x4_add(m4x4 a, m4x4 b);
static m4x4 m4x4_sub(m4x4 a, m4x4 b);
static m4x4 m4x4_mulf(m4x4 a, f32 f);
static m4x4 m4x4_mul(m4x4 a, m4x4 b);
static m4x4 m4x4_perspective(f32 fov, f32 aspect, f32 near, f32 far);
static m4x4 m4x4_to_coords(v3 pos, v3 right, v3 up, v3 forward);
static v4 m4x4_mulv(m4x4 m, v4 v);
static v4 m4x4_to_qt(m4x4 m);

static v4 m3x3_to_qt(m3x3 m);
static v3 m3x3_mulv(m3x3 m, v3 v);

static v3i v3i_vec3(v3 a);

static v3 qt_mul_v3(v4 q, v3 v);
static v4 qt_mul(v4 a, v4 b);
static v4 qt_conj(v4 q);
static v4 qt_inv(v4 q);
static v4 qt_norm(v4 q);
static v4 qt_rotate(v3 from, v3 to);
