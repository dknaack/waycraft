#include <math.h>

#include "types.h"
#include "camera.h"

void
camera_init(struct camera *c, v3 position, f32 speed, f32 fov)
{
    c->view = c->projection = m4x4_id(1.f);
    c->position = position;
    c->front = V3(0, 0, 1);
    c->up = V3(0, 1, 0);
    c->right = V3(1, 0, 0);
    c->yaw = c->pitch = 0.f;
    c->speed = speed;
    c->fov = fov;
}

void
camera_resize(struct camera *c, f32 width, f32 height)
{
    c->projection = m4x4_perspective(c->fov, width / height, 0.1f, 1000.f);
}

void
camera_rotate(struct camera *c, f32 dx, f32 dy)
{
    const f32 sensitivity = 0.1f;

    c->yaw   += dx * sensitivity;
    c->pitch -= dy * sensitivity;

    c->pitch = CLAMP(c->pitch, -89.f, 89.f);

	c->front.x = cosf(DEG2RAD(c->yaw)) * cosf(DEG2RAD(c->pitch));
	c->front.y = sinf(DEG2RAD(c->pitch));
	c->front.z = sinf(DEG2RAD(c->yaw)) * cosf(DEG2RAD(c->pitch));

	c->front = v3_norm(c->front);
    c->right = v3_norm(v3_cross(c->front, V3(0, 1, 0)));
    c->up    = v3_norm(v3_cross(c->right, c->front));
	c->view  = m4x4_look_at(c->position, v3_add(c->position, c->front), c->up);
}
