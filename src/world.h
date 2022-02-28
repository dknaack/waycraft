#ifndef WORLD_H
#define WORLD_H

#define CHUNK_SIZE 16

struct mesh {
    struct vertex *vertices;
    u32 *indices;

    u32 vertex_count;
    u32 index_count;
};

struct world {
    struct chunk *chunks;
    u32 width;
    u32 height;
    u32 depth;
    u32 texture;
};

struct chunk {
    u8 *blocks;
    i32 x;
    i32 y;
    i32 z;
    u32 vao, vbo, ebo;
    u32 index_count;
};

i32 world_init(struct world *world);
void world_generate_mesh(struct world *world, struct mesh *mesh);
void world_render(const struct world *world);
void world_finish(struct world *world);

#endif /* WORLD_H */ 
