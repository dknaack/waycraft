#ifndef BACKEND_H
#define BACKEND_H

struct backend_memory {
    void *data;
    u64 size;

    u32 is_initialized;
};

struct game_input;
void game_update(struct backend_memory *memory, struct game_input *input);

#endif /* BACKEND_H */ 
