#define add(a, b) (_Generic((a), v2: v2_add, v3: v3_add, v4: v4_add, m4x4: m4x4_add)(a, b))
#define sub(a, b) (_Generic((a), v2: v2_sub, v3: v3_sub, v4: v4_sub, m4x4: m4x4_sub)(a, b))
#define div(a, b) (_Generic((a), v2: v2_div, v3: v3_div, v4: v4_div)(a, b))
#define dot(a, b) (_Generic((a), v2: v2_dot, v3: v3_dot, v4: v4_dot)(a, b))
#define mulf(a, f) (_Generic((a), v2: v2_mulf, v3: v3_mulf, v4: v4_mulf, m4x4: m4x4_mulf)(a, f))
#define divf(a, f) (_Generic((a), v2: v2_divf, v3: v3_divf, v4: v4_divf, m4x4: m4x4_divf)(a, f))
#define mul(a, b) (_Generic((a), v2: v2_mul, v3: v3_mul, v4: v4_mul, m4x4: \
		_Generic((b), m4x4: m4x4_mul, v4: m4x4_mulv))(a, b))

#define neg(x) (_Generic((x), v2: v2_neg, v3: v3_neg, v4: v4_neg)(x))
#define length(x) (_Generic((x), v2: v2_len, v3: v3_len, v4: v4_len)(x))
#define length_sq(x) (_Generic((x), v2: v2_len_sq, v3: v3_len_sq, v4: v4_len_sq)(x))
#define normalize(x) (_Generic((x), v2: v2_norm, v3: v3_norm, v4: v4_norm)(x))
