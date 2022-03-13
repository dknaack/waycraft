#include <math.h>

#include "types.h"
#include "memory.h"
#include "world.h"
#include "gl.h"

enum block_type {
    BLOCK_AIR,
    BLOCK_STONE,
    BLOCK_DIRT,
    BLOCK_GRASS,
    BLOCK_GRASS_TOP,
    BLOCK_PLANKS,
    BLOCK_OAK_LOG,
    BLOCK_OAK_LEAVES,
    BLOCK_SAND,
    BLOCK_WATER,
};

static inline f32
normalize_height(f32 value)
{
    f32 x = value + 0.8;

    if (x < 1.0) {
        x *= MIN(MAX(x, 0.6), 1.0);
    }

    return x;
}

static void
chunk_init(struct chunk *chunk, u8 *blocks, i32 cx, i32 cy, i32 cz)
{
    f32 noise_size = 0.1;

    chunk->blocks = blocks;
    for (i32 z = 0; z < CHUNK_SIZE; z++) {
        for (i32 x = 0; x < CHUNK_SIZE; x++) {
            f32 nx = noise_size * (cx + x + 0.5);
            f32 nz = noise_size * (cz + z + 0.5);
            i32 height = normalize_height(noise_layered_2d(nx, nz)) * CHUNK_SIZE - cy;

            i32 ymax = MAX(0, MIN(CHUNK_SIZE, height));
            for (i32 y = 0; y < ymax; y++) {
                u32 i = (z * CHUNK_SIZE + y) * CHUNK_SIZE + x;
                if (y + 1 == height && cy + y > 0) {
                    blocks[i] = BLOCK_GRASS;
                } else if (y + 2 >= height && (cy + ymax <= 2 && cy + ymax >= -2)) {
                    blocks[i] = BLOCK_SAND;
                } else if (y + 2 >= height) {
                    blocks[i] = BLOCK_DIRT;
                } else {
                    blocks[i] = BLOCK_STONE;
                }
            }

            if (cy < 0) {
                for (i32 y = ymax; y < CHUNK_SIZE; y++) {
                    u32 i = (z * CHUNK_SIZE + y) * CHUNK_SIZE + x;
                    blocks[i] = BLOCK_WATER;
                }
            }
        }
    }
}

static u32
chunk_at(const struct chunk *chunk, i32 x, i32 y, i32 z)
{
    u32 is_inside_chunk = ((0 <= x && x < CHUNK_SIZE) && 
                           (0 <= y && y < CHUNK_SIZE) && 
                           (0 <= z && z < CHUNK_SIZE));
    if (is_inside_chunk) {
        u32 index = (z * CHUNK_SIZE + y) * CHUNK_SIZE + x;

        return chunk->blocks[index];
    } else {
        return 0;
    }
}

u32
world_at(const struct world *world, f32 x, f32 y, f32 z)
{
    x -= world->position.x;
    y -= world->position.y;
    z -= world->position.z;

    u32 cx = x / CHUNK_SIZE;
    u32 cy = y / CHUNK_SIZE;
    u32 cz = z / CHUNK_SIZE;

    x = fmodf(x, CHUNK_SIZE);
    y = fmodf(y, CHUNK_SIZE);
    z = fmodf(z, CHUNK_SIZE);

    i32 chunk_is_inside_world = ((0 <= cx && cx < world->width) && 
                                 (0 <= cy && cy < world->height) && 
                                 (0 <= cz && cz < world->depth));
    if (chunk_is_inside_world) {
        u32 width  = world->width;
        u32 height = world->height;
        u32 depth  = world->depth;

        u32 chunk_index = (cz * height + cy) * width + cx;
        u32 chunk_count = width * height * depth;
        assert(chunk_index < chunk_count);

        struct chunk *chunk = &world->chunks[chunk_index];
        if (!chunk->blocks) {
            vec3 world_position = world->position;

            f32 chunk_x = world_position.x + cx * CHUNK_SIZE;
            f32 chunk_y = world_position.y + cy * CHUNK_SIZE;
            f32 chunk_z = world_position.z + cz * CHUNK_SIZE;

            u64 offset = chunk_index * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
            u8 *blocks = world->blocks + offset;
            chunk_init(chunk, blocks, chunk_x, chunk_y, chunk_z);
        }

        return chunk_at(chunk, x, y, z);
    } else {
        return 0;
    }
}

static void
world_position_of_chunk(const struct world *world, const struct chunk *chunk,
                        f32 *x, f32 *y, f32 *z)

{
    vec3 world_position = world->position;
    i32 index  = chunk - world->chunks;
    i32 width  = world->width;
    i32 height = world->height;
    i32 depth  = world->depth;

    *x = world_position.x + CHUNK_SIZE * (f32)(index % width);
    *y = world_position.y + CHUNK_SIZE * (f32)(index / width % height);
    *z = world_position.z + CHUNK_SIZE * (f32)(index / width / height % depth);
}

static void
block_texcoords(enum block_type block, vec2 *uv)
{
    uv[0] = vec2_mulf(vec2_add(VEC2(block, 0), VEC2(0, 0)), 1 / 16.f);
    uv[1] = vec2_mulf(vec2_add(VEC2(block, 0), VEC2(1, 0)), 1 / 16.f);
    uv[2] = vec2_mulf(vec2_add(VEC2(block, 0), VEC2(0, 1)), 1 / 16.f);
    uv[3] = vec2_mulf(vec2_add(VEC2(block, 0), VEC2(1, 1)), 1 / 16.f);
}

static void
chunk_generate_mesh(struct chunk *chunk, const struct world *world, struct mesh *mesh)
{
    f32 xmin, ymin, zmin;
    world_position_of_chunk(world, chunk, &xmin, &ymin, &zmin);
    for (i32 z = zmin; z < zmin + CHUNK_SIZE; z++) {
        for (i32 y = ymin; y < ymin + CHUNK_SIZE; y++) {
            for (i32 x = xmin; x < xmin + CHUNK_SIZE; x++) {
                vec2 uv[4];

                vec3 pos0 = VEC3(x + 0.5, y + 0.5, z + 0.5);
                vec3 pos1 = VEC3(x - 0.5, y + 0.5, z + 0.5);
                vec3 pos2 = VEC3(x + 0.5, y - 0.5, z + 0.5);
                vec3 pos3 = VEC3(x - 0.5, y - 0.5, z + 0.5);

                vec3 pos4 = VEC3(x + 0.5, y + 0.5, z - 0.5);
                vec3 pos5 = VEC3(x - 0.5, y + 0.5, z - 0.5);
                vec3 pos6 = VEC3(x + 0.5, y - 0.5, z - 0.5);
                vec3 pos7 = VEC3(x - 0.5, y - 0.5, z - 0.5);

                u32 block_type = world_at(world, x, y, z);
                if (block_type == BLOCK_AIR) {
                    continue;
                } else if (block_type == BLOCK_GRASS) {
                    block_texcoords(BLOCK_DIRT, uv);
                    if (!world_at(world, x, y - 1, z)) {
                        mesh_push_quad(mesh, pos7, pos6, pos3, pos2,
                                       uv[0], uv[1], uv[2], uv[3]);
                    }

                    block_texcoords(BLOCK_GRASS_TOP, uv);
                    if (!world_at(world, x, y + 1, z)) {
                        mesh_push_quad(mesh, pos4, pos5, pos0, pos1,
                                       uv[0], uv[1], uv[2], uv[3]);
                    }

                    block_texcoords(BLOCK_GRASS, uv);
                    if (!world_at(world, x + 1, y, z)) {
                        mesh_push_quad(mesh, pos4, pos0, pos6, pos2,
                                       uv[0], uv[1], uv[2], uv[3]);
                    }
                } else {
                    block_texcoords(block_type, uv);
                    if (!world_at(world, x, y - 1, z)) {
                        mesh_push_quad(mesh, pos7, pos6, pos3, pos2,
                                       uv[0], uv[1], uv[2], uv[3]);
                    }

                    if (!world_at(world, x, y + 1, z)) {
                        mesh_push_quad(mesh, pos4, pos5, pos0, pos1,
                                       uv[0], uv[1], uv[2], uv[3]);
                    }

                    if (!world_at(world, x + 1, y, z)) {
                        mesh_push_quad(mesh, pos4, pos0, pos6, pos2,
                                       uv[0], uv[1], uv[2], uv[3]);
                    }
                }

                if (!world_at(world, x - 1, y, z)) {
                    mesh_push_quad(mesh, pos1, pos5, pos3, pos7,
                                   uv[0], uv[1], uv[2], uv[3]);
                }

                if (!world_at(world, x, y, z + 1)) {
                    mesh_push_quad(mesh, pos0, pos1, pos2, pos3,
                                   uv[0], uv[1], uv[2], uv[3]);
                }

                if (!world_at(world, x, y, z - 1)) {
                    mesh_push_quad(mesh, pos5, pos4, pos7, pos6,
                                   uv[0], uv[1], uv[2], uv[3]);
                }
            }
        }
    }
}

i32
world_init(struct world *world, struct memory_arena *arena)
{
    struct mesh *mesh = &world->mesh;
    u32 size = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    mesh->vertices = arena_alloc(arena, size * 64, struct vertex);
    mesh->indices = arena_alloc(arena, size * 36, u32);
    if (!mesh->vertices || !mesh->indices) {
        return -1;
    }

    i32 width, height, comp;
    u8 *data;

    if (!(data = stbi_load("res/textures.png", &width, &height, &comp, 3))) {
        fprintf(stderr, "Failed to load texture atlas\n");
        return -1;
    }

    u32 texture;
    gl.GenTextures(1, &texture);
    gl.BindTexture(GL_TEXTURE_2D, texture);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.GenerateMipmap(GL_TEXTURE_2D);
    world->texture = texture;

    u64 chunk_count = world->width * world->height * world->depth;
    u64 chunk_size = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    u64 block_size = chunk_count * chunk_size;
    world->blocks = arena_alloc(arena, block_size, u8);

    return 0;
}

static struct chunk *
world_next_chunk_to_load(const struct world *world, u32 i)
{
    /* TODO: load from the middle outwards */
    i32 width  = world->width;
    i32 height = world->height;
    i32 depth  = world->depth;
    i32 total  = width * height * depth;

    if (i & 1) {
        return &world->chunks[total / 2 - (i + 1) / 2];
    } else {
        return &world->chunks[total / 2 + i / 2];
    }
}

void
world_update(struct world *world, vec3 player_position)
{
    struct mesh *mesh = &world->mesh;

    u32 max_load = 8;
    u32 total_chunk_count = world->width * world->height * world->depth;
    u32 first_chunk = world->loaded_chunk_count;
    u32 last_chunk = first_chunk + max_load;
    if (last_chunk > total_chunk_count) {
        last_chunk = total_chunk_count;
    }

    for (u32 i = first_chunk; i < last_chunk; i++) {
        mesh->index_count = 0;
        mesh->vertex_count = 0;

        struct chunk *chunk = world_next_chunk_to_load(world, i);
        chunk_generate_mesh(chunk, world, mesh);

        gl.GenVertexArrays(1, &chunk->vao);
        gl.GenBuffers(1, &chunk->ebo);
        gl.GenBuffers(1, &chunk->vbo);

        u32 vao = chunk->vao;
        gl.BindVertexArray(vao);

        u32 vbo = chunk->vbo;
        gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
        gl.BufferData(GL_ARRAY_BUFFER, mesh->vertex_count * 
                      sizeof(*mesh->vertices), mesh->vertices, GL_STATIC_DRAW);

        u32 ebo = chunk->ebo;
        gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->index_count * 
                      sizeof(*mesh->indices), mesh->indices, GL_STATIC_DRAW);

        gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex), 
                               (const void *)offsetof(struct vertex, position));
        gl.EnableVertexAttribArray(0);
        gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), 
                               (const void *)offsetof(struct vertex, texcoord));
        gl.EnableVertexAttribArray(1);
        chunk->index_count = mesh->index_count;
    }

    world->loaded_chunk_count = last_chunk;
}

void
world_render(const struct world *world)
{
    gl.BindTexture(GL_TEXTURE_2D, world->texture);

    u32 chunk_count = world->loaded_chunk_count;
    for (u32 i = 0; i < chunk_count; i++) {
        const struct chunk *chunk = world_next_chunk_to_load(world, i);
        gl.BindVertexArray(chunk->vao);
        gl.DrawElements(GL_TRIANGLES, chunk->index_count, GL_UNSIGNED_INT, 0);
    }
}

void
world_finish(struct world *world)
{
    u32 chunk_count = world->loaded_chunk_count;
    struct chunk *chunk = world->chunks;
    while (chunk_count-- > 0) {
        gl.DeleteVertexArrays(1, &chunk->vao);
        gl.DeleteBuffers(1, &chunk->vbo);
        gl.DeleteBuffers(1, &chunk->ebo);
        chunk++;
    }

    gl.DeleteTextures(1, &world->texture);
}

void
world_destroy_block(struct world *world, i32 x, i32 y, i32 z)
{
}
