static f32
lerp(f32 a, f32 b, f32 t)
{
	return a + (b - a) * t;
}

static f32
degrees(f32 radians)
{
	return 180.0f * radians / PI;
}

static f32
radians(f32 degrees)
{
	return PI * degrees / 180.0f;
}

/* v2 functions */
static v2 v2_add(v2 a, v2 b) { return V2(a.x + b.x, a.y + b.y); }
static v2 v2_sub(v2 a, v2 b) { return V2(a.x - b.x, a.y - b.y); }
static v2 v2_mul(v2 a, v2 b) { return V2(a.x * b.x, a.y * b.y); }
static v2 v2_div(v2 a, v2 b) { return V2(a.x / b.x, a.y / b.y); }
static v2 v2_mulf(v2 a, f32 f) { return V2(a.x * f, a.y * f); }
static v2 v2_divf(v2 a, f32 f) { return V2(a.x / f, a.y / f); }
static f32 v2_len(v2 a) { return sqrtf(a.x * a.x + a.y * a.y); }
static v2 v2_norm(v2 a) { return v2_divf(a, v2_len(a)); }
static f32 v2_dot(v2 a, v2 b) { return a.x * b.x + a.y * b.y; }

/* v3 functions */
static v3 v3_add(v3 a, v3 b) { return V3(a.x + b.x, a.y + b.y, a.z + b.z); }
static v3 v3_sub(v3 a, v3 b) { return V3(a.x - b.x, a.y - b.y, a.z - b.z); }
static v3 v3_mul(v3 a, v3 b) { return V3(a.x * b.x, a.y * b.y, a.z * b.z); }
static v3 v3_div(v3 a, v3 b) { return V3(a.x / b.x, a.y / b.y, a.z / b.z); }
static v3 v3_mulf(v3 a, f32 f) { return V3(a.x * f, a.y * f, a.z * f); }
static v3 v3_divf(v3 a, f32 f) { return V3(a.x / f, a.y / f, a.z / f); }
static f32 v3_dot(v3 a, v3 b) { return (a.x * b.x + a.y * b.y + a.z * b.z); }
static f32 v3_len_sq(v3 a) { return v3_dot(a, a); }
static f32 v3_len(v3 a) { return sqrtf(v3_len_sq(a)); }
static v3 v3_norm(v3 a) { return v3_divf(a, v3_len(a)); }

static v3 v3_floor(v3 a) { return V3(floorf(a.x), floorf(a.y), floorf(a.z)); }
static v3 v3_round(v3 a) { return V3(roundf(a.x), roundf(a.y), roundf(a.z)); }
static v3 v3_neg(v3 a) { return V3(-a.x, -a.y, -a.z); }
static v3 v3_lerp(v3 a, v3 b, f32 t) { return v3_add(a, v3_mulf(v3_sub(b, a), t)); }
static v3 v3_abs(v3 a) { return V3(fabsf(a.x), fabsf(a.y), fabsf(a.z)); }
static v3 v3_modf(v3 a, f32 f) { return V3(fmodf(a.x, f), fmodf(a.y, f), fmodf(a.z, f)); }

/* v4 function definitions */
static v4 v4_add(v4 a, v4 b) { return V4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
static v4 v4_sub(v4 a, v4 b) { return V4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
static v4 v4_mul(v4 a, v4 b) { return V4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }
static v4 v4_div(v4 a, v4 b) { return V4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }
static v4 v4_mulf(v4 a, f32 f) { return V4(a.x * f, a.y * f, a.z * f, a.w * f); }
static v4 v4_divf(v4 a, f32 f) { return V4(a.x / f, a.y / f, a.z / f, a.w / f); }
static f32 v4_dot(v4 a, v4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * a.w; }
static f32 v4_len_sq(v4 a) { return v4_dot(a, a); }
static f32 v4_len(v4 a) { return sqrtf(v4_len_sq(a)); }
static v4 v4_norm(v4 a) { return v4_divf(a, v4_len(a)); }

static v3
v3_cross(v3 a, v3 b)
{
	return V3(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x);
}


/* m4x4 function definitions */
m4x4
m4x4_add(m4x4 a, m4x4 b)
{
	u32 i;

	for (i = 0; i < 16; i++)
		a.e[i] += b.e[i];
	return a;
}

m4x4
m4x4_sub(m4x4 a, m4x4 b)
{
	u32 i;

	for (i = 0; i < 16; i++)
		a.e[i] -= b.e[i];

	return a;
}

m4x4
m4x4_mul(m4x4 a, m4x4 b)
{
	m4x4 result = {0};

	for (u32 i = 0; i < 4; i++) {
		for (u32 j = 0; j < 4; j++) {
			for (u32 k = 0; k < 4; k++) {
				result.e[i + 4 * j] += a.e[i + 4 * k] * b.e[k + 4 * j];
			}
		}
	}

	return result;
}

m4x4
m4x4_mulf(m4x4 a, f32 f)
{
	for (u32 i = 0; i < 16; i++) {
		a.e[i] *= f;
	}

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
m4x4_id(f32 f)
{
	return M4X4(
		f, 0, 0, 0,
		0, f, 0, 0,
		0, 0, f, 0,
		0, 0, 0, 1);
}

m4x4
m4x4_translate(f32 x, f32 y, f32 z)
{
	return M4X4(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		x, y, z, 1);
}

m4x4
m4x4_scale(f32 x, f32 y, f32 z)
{
	return M4X4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, z, 0,
		0, 0, 0, 1);
}

m4x4
m4x4_perspective(f32 fov, f32 aspect, f32 near, f32 far)
{
	m4x4 result = {0};

	f32 top = near * tanf(fov * PI / 180.f * 0.5f);
	f32 right = top * aspect;
	f32 fn = (far - near);

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

static m4x4
m4x4_rotate(v3 axis, f32 angle)
{
	v3 a = v3_norm(axis);
	f32 cos = cosf(angle);
	f32 sin = sinf(angle);

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

static m4x4
m4x4_to_coords(v3 pos, v3 right, v3 up, v3 forward)
{
	return m4x4_mul(
		m4x4_translate(pos.x, pos.y, pos.z),
		M4X4(
			right.x,   right.y,   right.z,   0,
			up.x,      up.y,      up.z,      0,
			forward.x, forward.y, forward.z, 0,
			0,         0,         0,         1));
}

static v4
m4x4_mulv(m4x4 m, v4 v)
{
	return V4(
		m.e[0] * v.x + m.e[4] * v.y + m.e[ 8] * v.z + m.e[12] * v.w,
		m.e[1] * v.x + m.e[5] * v.y + m.e[ 9] * v.z + m.e[13] * v.w,
		m.e[2] * v.x + m.e[6] * v.y + m.e[10] * v.z + m.e[14] * v.w,
		m.e[3] * v.x + m.e[7] * v.y + m.e[11] * v.z + m.e[15] * v.w);
}

static inline v4
m4x4_to_qt(m4x4 m)
{
	v4 q;

	q.w = sqrt(1 + m.e[0] + m.e[5] + m.e[10]) / 2;
	q.x = (m.e[9] - m.e[6]) / (4 * q.w);
	q.y = (m.e[2] - m.e[8]) / (4 * q.w);
	q.z = (m.e[4] - m.e[1]) / (4 * q.w);

	return q;
}

static v4
m3x3_to_qt(m3x3 m)
{
	f32 t = m.v[0][0] + m.v[1][1] + m.v[2][2];
	v4 q;

	if (t > 0) {
		f32 s = sqrt(t + 1) * 2;
		q.x = (m.v[2][1] - m.v[1][2]) / s;
		q.y = (m.v[0][2] - m.v[2][0]) / s;
		q.z = (m.v[1][0] - m.v[0][1]) / s;
		q.w = 0.25 * s;
	} else if (m.v[0][0] > m.v[1][1] && m.v[0][0] > m.v[2][2]) {
		f32 s = sqrt(1.0 + m.v[0][0] - m.v[1][1] - m.v[2][2]) * 2;
		q.x = 0.25 * s;
		q.y = (m.v[0][1] + m.v[1][0]) / s;
		q.z = (m.v[0][2] + m.v[2][0]) / s;
		q.w = (m.v[2][1] - m.v[1][2]) / s;
	} else if (m.v[1][1] > m.v[2][2]) {
		f32 s = sqrt(1.0 + m.v[1][1] - m.v[0][0] - m.v[2][2]) * 2;
		q.x = (m.v[0][1] + m.v[1][0]) / s;
		q.y = 0.25 * s;
		q.z = (m.v[1][2] + m.v[2][1]) / s;
		q.w = (m.v[0][2] - m.v[2][0]) / s;
	} else {
		f32 s = sqrt(1.0 + m.v[2][2] - m.v[0][0] - m.v[1][1]) * 2;
		q.x = (m.v[0][2] + m.v[2][0]) / s;
		q.y = (m.v[1][2] + m.v[2][1]) / s;
		q.z = 0.25 * s;
		q.w = (m.v[1][0] - m.v[0][1]) / s;
	}

	return q;
}

static v3
m3x3_mulv(m3x3 m, v3 v)
{
	return V3(
		m.e[0] * v.x + m.e[3] * v.y + m.e[6] * v.z,
		m.e[1] * v.x + m.e[4] * v.y + m.e[7] * v.z,
		m.e[2] * v.x + m.e[5] * v.y + m.e[8] * v.z);
}

static v3i
v3i_vec3(v3 a)
{
	v3i result;

	result.x = a.x;
	result.y = a.y;
	result.z = a.z;

	return result;
}

/*
 * quaternion functions
 */

static v4
qt_add(v4 a, v4 b)
{
	return v4_add(a, b);
}

static v4
qt_mul(v4 a, v4 b)
{
	v4 result;

	result.xyz = v3_add(v3_cross(a.xyz, b.xyz),
		v3_add(v3_mulf(b.xyz, a.w), v3_mulf(a.xyz, b.w)));
	result.w = a.w * b.w - v3_dot(a.xyz, b.xyz);

	return result;
}

static v3
qt_mul_v3(v4 q, v3 v)
{
	v3 t = v3_mulf(v3_cross(q.xyz, v), 2);
	return v3_add(v, v3_add(v3_mulf(t, q.w), v3_cross(q.xyz, t)));
}

static v4
qt_conj(v4 a)
{
	v4 result;

	result.xyz = v3_neg(a.xyz);
	result.w = a.w;

	return result;
}

static f32
qt_len(v4 a)
{
	return v4_dot(a, a);
}

static v4
qt_inv(v4 q)
{
	return v4_divf(qt_conj(q), qt_len(q));
}

static v4
qt_rotate(v3 from, v3 to)
{
	v4 q = V4(0, 0, 1, 0);
	f32 w = v3_dot(from, to);

	if (w != -1) {
		q.xyz = v3_cross(from, to);
		q.w = w;
		q.w += v4_len(q);
		q = v4_norm(q);
	}

	return q;
}
