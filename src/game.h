#ifndef GAME_H
#define GAME_H

#include "types.h"
#include "camera.h"
#include "world.h"
#include "memory.h"

struct window {
    vec4 rotation;
    vec3 scale;
    vec3 position;
    u32 width, height;
    u32 texture;
};

struct game_input {
    struct {
        f32 dx, dy;
        f32 x, y;
    } mouse;

    struct {
        u8 move_up;
        u8 move_down;
        u8 move_left;
        u8 move_right;
        u8 jump;
    } controller;

    f32 dt;
    u32 width;
    u32 height;
};

struct game_state {
    struct world world;
    struct camera camera;
    struct memory_arena arena;
    struct {
        u32 program;
        i32 camera_position;
        i32 view;
        i32 projection;
    } shader;
    u32 program;
};

i32 game_init(struct game_state *game);
i32 game_update(struct game_state *game, struct game_input *input);
void game_finish(struct game_state *game);

#endif /* GAME_H */ 
