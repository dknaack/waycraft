#define generic_float_vector_func(name, x) _Generic((x), \
v2: v2_##name, v3: v3_##name, v4: v4_##name)
#define generic_vector_func(name, x) _Generic((x), \
    v2: v2_##name, v3: v3_##name, v4: v4_##name, v3i: v3i_##name)
#define generic_matrix_vector_func(name, x) _Generic((x), m4x4: m4x4_##name, \
    v2: v2_##name, v3: v3_##name, v4: v4_##name, v3i: v3i_##name)
#define generic_float_matrix_vector_func(name, x) _Generic((x), \
    v2: v2_##name, v3: v3_##name, v4: v4_##name, m4x4: m4x4_##name)

#define add(a, b)  (generic_matrix_vector_func(add, (a))(a, b))
#define sub(a, b)  (generic_matrix_vector_func(sub, (a))(a, b))
#define div(a, b)  (generic_vector_func(div, (a))(a, b))
#define dot(a, b)  (generic_float_vector_func(dot, (a))(a, b))
#define mulf(a, b) (generic_float_matrix_vector_func(mulf, (a))(a, b))
#define divf(a, b) (generic_float_matrix_vector_func(divf, (a))(a, b))
#define mul(a, b)  (_Generic((a), v2: v2_mul, v3: v3_mul, v4: v4_mul, \
    m4x4: _Generic((b), m4x4: m4x4_mul, v4: m4x4_mulv, default: 0))(a, b))

#define neg(x)       (generic_float_vector_func(neg, (x))(x))
#define length(x)    (generic_float_vector_func(len, (x))(x))
#define length_sq(x) (generic_float_vector_func(len_sq, (x))(x))
#define normalize(x) (generic_float_vector_func(norm, (x))(x))

#define cross(a, b) v3_cross(a, b)

#define m4x4(...)      (m4x4){{ __VA_ARGS__ }}
#define m3x3(...)      (m3x3){{ __VA_ARGS__ }}
#define v2(x, y)       (v2){{ (x), (y) }}
#define v3(x, y, z)    (v3){{ (x), (y), (z) }}
#define v4(x, y, z, w) (v4){{ (x), (y), (z), (w) }}
#define v3i(x, y, z)   (v3i){{ (x), (y), (z) }}
