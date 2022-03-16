#include <math.h>

#include "types.h"
#include "memory.h"
#include "world.h"
#include "gl.h"
#include "debug.h"
#include "timer.h"

inline u32
block_is_empty(enum block_type block)
{
    return block == BLOCK_AIR || block == BLOCK_WATER;
}

static inline u32
block_is_not_water(enum block_type block)
{
    return block != BLOCK_WATER;
}

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
    f32 noise_size = 0.1f;

    chunk->flags = CHUNK_MODIFIED;
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
    u32 result = 0;
    u32 is_inside_chunk = ((0 <= x && x < CHUNK_SIZE) && 
                           (0 <= y && y < CHUNK_SIZE) && 
                           (0 <= z && z < CHUNK_SIZE));
    if (is_inside_chunk) {
        u32 index = (z * CHUNK_SIZE + y) * CHUNK_SIZE + x;

        result = chunk->blocks[index];
    }

    return result;
}

static vec3
world_get_chunk_position(const struct world *world, const struct chunk *chunk)
{
    vec3 world_position = world->position;
    i32 index  = chunk - world->chunks;
    i32 width  = world->width;
    i32 height = world->height;
    i32 depth  = world->depth;

    vec3 result;
    result.x = world_position.x + CHUNK_SIZE * (index % width);
    result.y = world_position.y + CHUNK_SIZE * (index / width % height);
    result.z = world_position.z + CHUNK_SIZE * (index / width / height % depth);

    return result;
}

static void
world_unload_chunk(struct world *world, struct chunk *chunk)
{
    if (chunk) {
        u32 is_already_modified = chunk->flags & CHUNK_MODIFIED;
        if (!is_already_modified) {
            chunk->flags |= CHUNK_MODIFIED;

            u32 chunk_index = chunk - world->chunks;
            u32 index = world->unloaded_chunk_count++;
            assert(index < world->chunk_count);

            world->unloaded_chunks[index] = chunk_index;
        }
    }
}

// NOTE: may generate chunk if it has not been initialized
static struct chunk *
world_get_chunk(struct world *world, f32 x, f32 y, f32 z)
{
    struct chunk *chunk = 0;

    x -= world->position.x;
    y -= world->position.y;
    z -= world->position.z;

    i32 chunk_x = x / CHUNK_SIZE;
    i32 chunk_y = y / CHUNK_SIZE;
    i32 chunk_z = z / CHUNK_SIZE;
    i32 chunk_is_inside_world = ((0 <= chunk_x && chunk_x < world->width) && 
                                 (0 <= chunk_y && chunk_y < world->height) && 
                                 (0 <= chunk_z && chunk_z < world->depth));
    if (chunk_is_inside_world) {
        u32 width  = world->width;
        u32 height = world->height;
        u32 depth  = world->depth;

        u32 chunk_index = (chunk_z * height + chunk_y) * width + chunk_x;
        u32 chunk_count = width * height * depth;
        assert(chunk_index < chunk_count);

        chunk = &world->chunks[chunk_index];
        if (!chunk->blocks) {
            vec3 chunk_pos = world_get_chunk_position(world, chunk);

            u64 offset = chunk_index * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
            u8 *blocks = world->blocks + offset;
            chunk_init(chunk, blocks, chunk_pos.x, chunk_pos.y, chunk_pos.z);
            world_unload_chunk(world, chunk);
        }
    }

    return chunk;
}

static vec3
world_get_block_position(const struct world *world, f32 x, f32 y, f32 z)
{
    vec3 relative_pos = vec3_sub(VEC3(x, y, z), world->position);
    vec3 block_pos = vec3_modf(relative_pos, CHUNK_SIZE);

    return block_pos;
}

u32
world_at(struct world *world, f32 x, f32 y, f32 z)
{
    u32 result = 0;

    struct chunk *chunk = world_get_chunk(world, x, y, z);

    if (chunk) {
        vec3 block_pos = world_get_block_position(world, x, y, z);
        result = chunk_at(chunk, block_pos.x, block_pos.y, block_pos.z);
    }

    return result;
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
block_texcoords_top(enum block_type block, vec2 *uv)
{
    if (block == BLOCK_GRASS) {
        block_texcoords(BLOCK_GRASS_TOP, uv);
    } else {
        block_texcoords(block, uv);
    }
}

static void
block_texcoords_side(enum block_type block, vec2 *uv)
{
    block_texcoords(block, uv);
}

static void
block_texcoords_bottom(enum block_type block, vec2 *uv)
{
    if (block == BLOCK_GRASS) {
        block_texcoords(BLOCK_DIRT, uv);
    } else {
        block_texcoords(block, uv);
    }
}

static void
chunk_generate_mesh(struct chunk *chunk, struct world *world, struct mesh *mesh)
{
    vec3 min = world_get_chunk_position(world, chunk);
    vec3 max = vec3_add(min, VEC3(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE));

    for (i32 z = min.z; z < max.z; z++) {
        for (i32 y = min.y; y < max.y; y++) {
            for (i32 x = min.x; x < max.x; x++) {
                vec2 uv[4];

                u32 block = world_at(world, x, y, z);
                if (block == BLOCK_AIR) {
                    continue;
                }

                f32 yoff = block == BLOCK_WATER && world_at(world, x, y + 1, z) == BLOCK_AIR ? -0.1 : 0;
                vec3 pos0 = VEC3(x + 0.5, y + yoff + 0.5, z + 0.5);
                vec3 pos1 = VEC3(x - 0.5, y + yoff + 0.5, z + 0.5);
                vec3 pos2 = VEC3(x + 0.5, y - 0.5, z + 0.5);
                vec3 pos3 = VEC3(x - 0.5, y - 0.5, z + 0.5);

                vec3 pos4 = VEC3(x + 0.5, y + yoff + 0.5, z - 0.5);
                vec3 pos5 = VEC3(x - 0.5, y + yoff + 0.5, z - 0.5);
                vec3 pos6 = VEC3(x + 0.5, y - 0.5, z - 0.5);
                vec3 pos7 = VEC3(x - 0.5, y - 0.5, z - 0.5);

                u32 (*is_empty)(enum block_type block) =
                    block == BLOCK_WATER ? block_is_not_water : block_is_empty;

                block_texcoords_bottom(block, uv);
                if (is_empty(world_at(world, x, y - 1, z))) {
                    mesh_push_quad(mesh, pos7, pos6, pos3, pos2,
                                   uv[0], uv[1], uv[2], uv[3]);
                }

                block_texcoords_top(block, uv);
                if (is_empty(world_at(world, x, y + 1, z))) {
                    mesh_push_quad(mesh, pos4, pos5, pos0, pos1,
                                   uv[0], uv[1], uv[2], uv[3]);
                }

                block_texcoords_side(block, uv);
                if (is_empty(world_at(world, x + 1, y, z))) {
                    mesh_push_quad(mesh, pos4, pos0, pos6, pos2,
                                   uv[0], uv[1], uv[2], uv[3]);
                }

                if (is_empty(world_at(world, x - 1, y, z))) {
                    mesh_push_quad(mesh, pos1, pos5, pos3, pos7,
                                   uv[0], uv[1], uv[2], uv[3]);
                }

                if (is_empty(world_at(world, x, y, z + 1))) {
                    mesh_push_quad(mesh, pos0, pos1, pos2, pos3,
                                   uv[0], uv[1], uv[2], uv[3]);
                }

                if (is_empty(world_at(world, x, y, z - 1))) {
                    mesh_push_quad(mesh, pos5, pos4, pos7, pos6,
                                   uv[0], uv[1], uv[2], uv[3]);
                }
            }
        }
    }

    gl.BindBuffer(GL_ARRAY_BUFFER, chunk->vbo);
    gl.BufferData(GL_ARRAY_BUFFER, mesh->vertex_count * 
                  sizeof(*mesh->vertices), mesh->vertices, GL_STATIC_DRAW);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk->ebo);
    gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->index_count *
                  sizeof(*mesh->indices), mesh->indices, GL_STATIC_DRAW);

    chunk->index_count = mesh->index_count;
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
    world->chunk_count = chunk_count;

    u64 chunk_size = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    u64 block_size = chunk_count * chunk_size;
    world->blocks = arena_alloc(arena, block_size, u8);

    world->unloaded_chunks = arena_alloc(arena, chunk_count, u32);
    world->unloaded_chunk_count = 0;

    while (chunk_count-- > 0) {
        world_unload_chunk(world, world->chunks + chunk_count);
    }

    return 0;
}

void
world_update(struct world *world, vec3 player_position)
{
    struct mesh *mesh = &world->mesh;
    struct chunk *chunks = world->chunks;

    u32 max_load = 4;
    u32 batch_count = MIN(world->unloaded_chunk_count, max_load);
    u32 *unloaded_chunks = world->unloaded_chunks + 
        world->unloaded_chunk_count;

    for (u32 i = 0; i < batch_count; i++) {
        mesh->index_count = 0;
        mesh->vertex_count = 0;

        unloaded_chunks--;
        u32 chunk_index = *unloaded_chunks;
        struct chunk *chunk = &chunks[chunk_index];

        u32 is_initialized = chunk->flags & CHUNK_INITIALIZED;
        if (!is_initialized) {
            chunk->flags |= CHUNK_INITIALIZED;

            gl.GenVertexArrays(1, &chunk->vao);
            gl.GenBuffers(1, &chunk->ebo);
            gl.GenBuffers(1, &chunk->vbo);
        }

        u32 vao = chunk->vao;
        gl.BindVertexArray(vao);

        chunk_generate_mesh(chunk, world, mesh);

        gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex), 
                               (const void *)offsetof(struct vertex, position));
        gl.EnableVertexAttribArray(0);
        gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), 
                               (const void *)offsetof(struct vertex, texcoord));
        gl.EnableVertexAttribArray(1);

        chunk->flags &= ~CHUNK_MODIFIED;
    }

    world->unloaded_chunk_count -= batch_count;
}

void
world_render(const struct world *world)
{
    gl.BindTexture(GL_TEXTURE_2D, world->texture);

    u32 chunk_count = world->chunk_count;
    struct chunk *chunk = world->chunks;
    while (chunk_count-- > 0) {
        u32 is_initialized = chunk->flags & CHUNK_INITIALIZED;
        if (is_initialized) {
            gl.BindVertexArray(chunk->vao);
            gl.DrawElements(GL_TRIANGLES, chunk->index_count, GL_UNSIGNED_INT, 0);
        }

        chunk++;
    }
}

void
world_finish(struct world *world)
{
    u32 chunk_count = world->chunk_count;
    struct chunk *chunk = world->chunks;
    while (chunk_count-- > 0) {
        u32 is_initialized = chunk->flags & CHUNK_INITIALIZED;
        if (is_initialized) {
            gl.DeleteVertexArrays(1, &chunk->vao);
            gl.DeleteBuffers(1, &chunk->vbo);
            gl.DeleteBuffers(1, &chunk->ebo);
            chunk++;
        }
    }

    gl.DeleteTextures(1, &world->texture);
}

void
world_destroy_block(struct world *world, f32 x, f32 y, f32 z)
{
    world_place_block(world, x, y, z, BLOCK_AIR);
}

void
world_place_block(struct world *world, f32 x, f32 y, f32 z, 
                  enum block_type block_type)
{
    vec3 point = {{ x, y, z }};
    struct chunk *chunk = world_get_chunk(world, x, y, z);
    if (chunk) {
        vec3 block_pos = world_get_block_position(world, x, y, z);
        ivec3 block = ivec3_vec3(vec3_floor(block_pos));
        
        u32 i = (block.z * CHUNK_SIZE + block.y) * CHUNK_SIZE + block.x;
        chunk->blocks[i] = block_type;
        world_unload_chunk(world, chunk);

        struct chunk *next_chunk = 0;
        vec3 offset, next;
        for (u32 i = 0; i < 3; i++) {
            offset = VEC3(i == 0, i == 1, i == 2);
            next = vec3_add(point, offset);
            next_chunk = world_get_chunk(world, next.x, next.y, next.z);
            world_unload_chunk(world, next_chunk);

            next = vec3_sub(point, offset);
            next_chunk = world_get_chunk(world, next.x, next.y, next.z);
            world_unload_chunk(world, next_chunk);
        }
    }
}
