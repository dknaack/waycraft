#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uintptr_t usize, usz;
typedef uint64_t  u64;
typedef uint32_t  u32;
typedef uint16_t  u16;
typedef uint8_t   u8;

typedef intptr_t isize, isz;
typedef int64_t  i64;
typedef int32_t  i32;
typedef int16_t  i16;
typedef int8_t   i8;

typedef double f64;
typedef float  f32;

typedef union {
	struct { f32 x, y; };
	struct { f32 width, height; };
	f32 e[2];
} v2;

typedef union {
	struct { f32 x, y, z; };
	struct { f32 r, g, b; };
	f32 e[3];
} v3;

typedef union {
	struct { f32 x, y, z, w; };
	struct { f32 r, g, b, a; };
	struct { v3 xyz; f32 _w; };
	f32 e[4];
} v4;

// NOTE: matrices are column major
typedef struct {
	f32 _e[16];
	f32 e[4][4];
	v4 c[4];
} m4x4;

typedef struct {
	f32 e[9];
	f32 v[3][3];
} m3x3;

typedef union {
	struct { i32 x, y, z; };
	i32 e[3];
} v3i;

typedef struct box2 {
	v2 min;
	v2 max;
} box2;

typedef struct box3 {
	v3 min;
	v3 max;
} box3;

#define assert(expr) ((expr) ? 0 : (*(volatile int *)0 = 0))
#define static_assert(expr, msg) _Static_assert(expr, msg)

#define KB(x)       ((x) * 1024ll)
#define MB(x)     (KB(x) * 1024ll)
#define GB(x)     (MB(x) * 1024ll)
#define TB(x)     (GB(x) * 1024ll)
#define MOD(a, n) (((a) % (n) + (n)) % (n))
#define MIN(a, b) ((a) < (b)? (a) : (b))
#define MAX(a, b) ((a) > (b)? (a) : (b))
#define SIGN(x)   ((x) < 0? -1 : 1)
#define LENGTH(x) (sizeof(x)/sizeof((x)[0]))
#define CLAMP(x, min, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))
#define CLAMP01(x) CLAMP(x, 0, 1)

#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define F32_PI 3.14159265358979323846f
#define F32_TAU (2.0f * F32_PI)
#define F32_INF INFINITY
#define F32_EPSILON 1e-5f
