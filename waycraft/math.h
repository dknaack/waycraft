#define generic_vector_func(name, x) _Generic((x), \
	v2: v2_##name, v3: v3_##name, v4: v4_##name)
#define generic_vectori_func(name, x) _Generic((x), \
	v2: v2_##name, v3: v3_##name, v4: v4_##name, v3i: v3i_##name)

#define add(a, b) (generic_vectori_func(add, a)(a, b))
#define sub(a, b) (generic_vectori_func(sub, a)(a, b))
#define div(a, b) (generic_vectori_func(div, a)(a, b))
#define dot(a, b) (generic_vectori_func(dot, a)(a, b))
#define mulf(a, f) (_Generic((a), v2: v2_mulf, v3: v3_mulf, v4: v4_mulf, m4x4: m4x4_mulf)(a, f))
#define divf(a, f) (_Generic((a), v2: v2_divf, v3: v3_divf, v4: v4_divf, m4x4: m4x4_divf)(a, f))
#define mul(a, b) (_Generic((a), v2: v2_mul, v3: v3_mul, v4: v4_mul, m4x4: \
		_Generic((b), m4x4: m4x4_mul, v4: m4x4_mulv))(a, b))

#define neg(x)       (generic_vector_func(neg, x)(x))
#define length(x)    (generic_vector_func(len, x)(x))
#define length_sq(x) (generic_vector_func(len_sq, x)(x))
#define normalize(x) (generic_vector_func(norm, x)(x))

#define cross(a, b) v3_cross(a, b)
