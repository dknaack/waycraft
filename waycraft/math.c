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
static m4x4
m4x4_add(m4x4 a, m4x4 b)
{
	m4x4 result = {0};

	result.c[0] = v4_add(a.c[0], b.c[0]);
	result.c[1] = v4_add(a.c[1], b.c[1]);
	result.c[2] = v4_add(a.c[2], b.c[2]);
	result.c[3] = v4_add(a.c[3], b.c[3]);

	return result;
}

static m4x4
m4x4_sub(m4x4 a, m4x4 b)
{
	m4x4 result = {0};

	result.c[0] = v4_sub(a.c[0], b.c[0]);
	result.c[1] = v4_sub(a.c[1], b.c[1]);
	result.c[2] = v4_sub(a.c[2], b.c[2]);
	result.c[3] = v4_sub(a.c[3], b.c[3]);

	return result;
}

static m4x4
m4x4_mul(m4x4 a, m4x4 b)
{
	m4x4 result = {0};

	for (u32 i = 0; i < 4; i++) {
		for (u32 j = 0; j < 4; j++) {
			for (u32 k = 0; k < 4; k++) {
				result.e[i][j] += a.e[k][j] * b.e[i][k];
			}
		}
	}

	return result;
}

static m4x4
m4x4_mulf(m4x4 a, f32 f)
{
	m4x4 result = {0};

	result.c[0] = v4_mulf(a.c[0], f);
	result.c[1] = v4_mulf(a.c[1], f);
	result.c[2] = v4_mulf(a.c[2], f);
	result.c[3] = v4_mulf(a.c[3], f);

	return result;
}

static m4x4
m4x4_transpose(m4x4 a)
{
	m4x4 result = {0};

	for (u32 i = 0; i < 4; i++) {
		for (u32 j = 0; j < 4; j++) {
			result.e[i][j] = a.e[j][i];
		}
	}

	return result;
}

static m4x4
m4x4_id(f32 f)
{
	m4x4 result = {0};

	result.e[0][0] = 1;
	result.e[1][1] = 1;
	result.e[2][2] = 1;
	result.e[3][3] = 1;

	return result;
}

static m4x4
m4x4_translate(f32 x, f32 y, f32 z)
{
	m4x4 result = m4x4_id(1);

	result.c[3].x = x;
	result.c[3].y = y;
	result.c[3].z = z;

	return result;
}

static m4x4
m4x4_scale(f32 x, f32 y, f32 z)
{
	m4x4 result = {0};

	result.e[0][0] = x;
	result.e[1][1] = y;
	result.e[2][2] = z;
	result.e[3][3] = 1;

	return result;
}

static m4x4
m4x4_perspective(f32 fov, f32 aspect, f32 near, f32 far)
{
	m4x4 result = {0};
	f32 tan_fov = tan(radians(fov) / 2);

	result.e[0][0] = 1 / (aspect * tan_fov);
	result.e[1][1] = 1 / (tan_fov);
	result.e[2][2] = -(far + near) / (far - near);
	result.e[2][3] = -1;
	result.e[3][2] = -(2 * far * near) / (far - near);

	return result;
}

static m4x4
m4x4_ortho(f32 bottom, f32 top, f32 left, f32 right, f32 near, f32 far)
{
	m4x4 result = m4x4_id(1);

    result.e[0][0] = 2 / (right - left);
    result.e[1][1] = 2 / (top - bottom);
    result.e[2][2] = -2 / (far - near);

    result.e[3][0] = -(right + left) / (right - left);
    result.e[3][1] = -(top + bottom) / (top - bottom);
    result.e[3][2] = -(far + near) / (far - near);

	return result;
}

static m4x4
m4x4_look_at(v3 eye, v3 center, v3 up)
{
	m4x4 result = m4x4_id(1);

	v3 z = v3_norm(v3_sub(center, eye));
	v3 x = v3_norm(v3_cross(z, up));
	v3 y = v3_cross(x, z);

	result.e[0][0] = x.x;
	result.e[1][0] = x.y;
	result.e[2][0] = x.z;
	result.e[3][0] = -v3_dot(x, eye);

	result.e[0][1] = y.x;
	result.e[1][1] = y.y;
	result.e[2][1] = y.z;
	result.e[3][1] = -v3_dot(y, eye);

	result.e[0][2] = -z.x;
	result.e[1][2] = -z.y;
	result.e[2][2] = -z.z;
	result.e[3][2] = v3_dot(z, eye);

	return result;
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
	v4 result = {0};

	result = v4_add(result, v4_mul(m.c[0], V4(v.x, v.x, v.x, v.x)));
	result = v4_add(result, v4_mul(m.c[1], V4(v.y, v.y, v.y, v.y)));
	result = v4_add(result, v4_mul(m.c[2], V4(v.z, v.z, v.z, v.z)));
	result = v4_add(result, v4_mul(m.c[3], V4(v.w, v.w, v.w, v.w)));

	return result;
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
