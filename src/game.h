#ifndef GAME_H
#define GAME_H

#include "types.h"
#include "camera.h"

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
    } controller;

    f32 dt;
    u32 width;
    u32 height;
};

struct vertex {
    vec3 position;
    vec2 texcoord;
};

struct world {
    u8 *blocks;
    u32 width;
    u32 height;
    u32 depth;
    u32 texture;
    u32 vao, vbo, ebo;
    u32 vertex_count;
    u32 index_count;
};

struct game_state {
    struct world world;
    struct camera camera;
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
