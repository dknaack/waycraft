#ifndef TYPES_H
#define TYPES_H

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

typedef uintptr_t usize;
typedef uint64_t  u64;
typedef uint32_t  u32;
typedef uint16_t  u16;
typedef uint8_t   u8;

typedef intptr_t isize;
typedef int64_t  i64;
typedef int32_t  i32;
typedef int16_t  i16;
typedef int8_t   i8;

typedef double f64;
typedef float  f32;

typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;

typedef union {
    struct { f32 x, y; };
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

typedef struct {
	f32 e[16];
    f32 v[4][4];
} m4x4;

typedef struct {
	f32 e[9];
    f32 v[3][3];
} m3x3;

typedef union {
    struct { i32 x, y, z; };
    f32 e[3];
} v3i;

#define KB(x)       ((x) * 1024ll)
#define MB(x)     (KB(x) * 1024ll)
#define GB(x)     (MB(x) * 1024ll)
#define TB(x)     (GB(x) * 1024ll)
#define MOD(a, n) (((a) % (n) + (n)) % (n))
#define MIN(a, b) ((a) < (b)? (a) : (b))
#define MAX(a, b) ((a) > (b)? (a) : (b))
#define SIGN(x)   ((x) < 0? -1 : 1)
#define LENGTH(x) (sizeof(x)/sizeof((x)[0]))
#define DEG2RAD(deg) ((deg) / 180.f * PI)
#define CLAMP(x, min, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))

#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define M4X4(...)      (m4x4){{ __VA_ARGS__ }}
#define M3X3(...)      (m3x3){{ __VA_ARGS__ }}
#define V4(x, y, z, w) (v4){{ (x), (y), (z), (w) }}
#define V3(x, y, z)    (v3){{ (x), (y), (z) }}
#define V2(x, y)       (v2){{ (x), (y) }}

#define PI 3.14159265359f

#define U64_MAX 0xffffffffffffffffl
#define U32_MAX 0xffffffff
#define U16_MAX 0xffff
#define U8_MAX  0xff

#define I64_MAX 0x7fffffffffffffffl
#define I32_MAX 0x7fffffff
#define I16_MAX 0x7fff
#define I8_MAX  0x7f

#define I64_MIN (i64)0x8000000000000000l
#define I32_MIN (i32)0x80000000l
#define I16_MIN (i16)0x8000
#define I8_MIN  (i8)0x80

#define F32_EPSILON 1e-5f

#endif /* TYPES_H */
