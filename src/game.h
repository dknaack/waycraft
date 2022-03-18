#ifndef GAME_H
#define GAME_H

#include "types.h"
#include "camera.h"
#include "world.h"
#include "memory.h"

enum game_mode {
    GAME_MODE_GAME,
    GAME_MODE_WINDOW,
};

struct window {
    m4x4 transform;
    u32 width, height;
    u32 texture;
};

struct game_input {
    struct {
        f32 dx, dy;
        f32 x, y;
        u8 buttons[8];
    } mouse;

    struct {
        u8 move_up;
        u8 move_down;
        u8 move_left;
        u8 move_right;
        u8 jump;
        u8 modifiers;
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
        i32 model;
        i32 view;
        i32 projection;
    } shader;
    u32 program;

    struct player {
        v3 position;
        v3 velocity;
        u8 is_jumping;
        u8 frames_since_jump;

        u8 hotbar[9];
        u8 hotbar_selection;
    } player;
    
    struct window *windows;
    u32 window_count;
    u32 window_vertex_array;
    u32 window_vertex_buffer;
    u32 window_index_buffer;
    u32 active_window;

    u32 cursor;
};

static i32 game_init(struct game_state *game);
static i32 game_update(struct game_state *game, struct game_input *input);
static void game_finish(struct game_state *game);

#endif /* GAME_H */ 
