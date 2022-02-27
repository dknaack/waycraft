#ifndef CAMERA_H
#define CAMERA_H

#include "types.h"

struct camera {
    mat4 view, projection;
    vec3 position;
    vec3 right, front, up;
    f32 speed;
    f32 yaw, pitch;
    f32 fov;
};

void camera_init(struct camera *c, vec3 pos, f32 speed, f32 fov);
void camera_resize(struct camera *c, f32 width, f32 height);
void camera_rotate(struct camera *c, f32 dx, f32 dy);

#endif /* CAMERA_H */ 
