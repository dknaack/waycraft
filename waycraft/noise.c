#include <math.h>

#include "types.h"

/* source: https://en.wikipedia.org/wiki/Perlin_noise#Implementation */

f32
smoothstep(f32 a, f32 b, f32 t)
{
    return (b - a) * ((t * (t * 6 - 15) + 10) * t * t * t) + a;
}

v2
noise_random_gradient(i32 ix, i32 iy)
{
    u32 w = 8 * sizeof(u32);
    u32 s = w / 2;

    u32 a = ix, b = iy;
    a *= 3284157443;
    b ^= (a << s | (a >> w)) - s;
    b *= 1911520717;
    a ^= (b << s | (b >> w)) - s;
    a *= 2048419325;

    f32 random = a * (3.14159265 / ~(~0u >> 1));
    return V2(cosf(random), sinf(random));
}

f32
noise_gradient(i32 ix, i32 iy, f32 x, f32 y)
{
    v2 gradient = noise_random_gradient(ix, iy);

    f32 dx = x - (f32)ix;
    f32 dy = y - (f32)iy;

    return dx * gradient.x + dy * gradient.y;
}

f32
noise_2d(f32 x, f32 y)
{
    i32 x0 = floorf(x);
    i32 x1 = x0 + 1;
    i32 y0 = floorf(y);
    i32 y1 = y0 + 1;

    f32 sx = x - (f32)x0;
    f32 sy = y - (f32)y0;

    f32 n00 = noise_gradient(x0, y0, x, y);
    f32 n01 = noise_gradient(x1, y0, x, y);
    f32 ix0 = smoothstep(n00, n01, sx);

    f32 n10 = noise_gradient(x0, y1, x, y);
    f32 n11 = noise_gradient(x1, y1, x, y);
    f32 ix1 = smoothstep(n10, n11, sx);

    f32 result = smoothstep(ix0, ix1, sy);
    return result;
}

f32
noise_layered_2d(f32 x, f32 y)
{
    u32 layer_count = 8;
    f32 noise_value = 0;
    f32 noise_size = 0.125;
    f32 amplitude_multiplier = 2.0;
    for (u32 i = 0; i < layer_count; i++) {
        f32 nx = noise_size * x;
        f32 ny = noise_size * y;

        noise_value += amplitude_multiplier * noise_2d(nx, ny);
        amplitude_multiplier /= 1.375;
        noise_size *= 1.5;
    }

    return noise_value;
}
