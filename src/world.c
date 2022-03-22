#include <math.h>

#include "types.h"
#include "memory.h"
#include "world.h"
#include "gl.h"
#include "debug.h"
#include "timer.h"

static void
mesh_push_quad(struct mesh *mesh, 
        v3 pos0, v3 pos1, v3 pos2, v3 pos3,
        v2 uv0, v2 uv1, v2 uv2, v2 uv3)
{
    u32 vertex_count = mesh->vertex_count;
    struct vertex *out_vertex = mesh->vertices + vertex_count;
    u32 *out_index = mesh->indices + mesh->index_count;

    out_vertex->position = pos0;
    out_vertex->texcoord = uv0;
    out_vertex++;

    out_vertex->position = pos1;
    out_vertex->texcoord = uv1;
    out_vertex++;

    out_vertex->position = pos2;
    out_vertex->texcoord = uv2;
    out_vertex++;

    out_vertex->position = pos3;
    out_vertex->texcoord = uv3;
    out_vertex++;

    *out_index++ = vertex_count;
    *out_index++ = vertex_count + 2;
    *out_index++ = vertex_count + 1;
    *out_index++ = vertex_count + 2;
    *out_index++ = vertex_count + 3;
    *out_index++ = vertex_count + 1;

    mesh->index_count += 6;
    mesh->vertex_count += 4;
}

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
chunk_init(struct chunk *chunk, u16 *blocks, i32 cx, i32 cy, i32 cz)
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

static v3
world_get_chunk_position(const struct world *world, const struct chunk *chunk)
{
    v3 world_position = world->position;
    i32 index  = chunk - world->chunks;
    i32 width  = world->width;
    i32 height = world->height;
    i32 depth  = world->depth;

    v3 result;
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
            v3 chunk_pos = world_get_chunk_position(world, chunk);

            u64 offset = chunk_index * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
            u16 *blocks = world->blocks + offset;
            chunk_init(chunk, blocks, chunk_pos.x, chunk_pos.y, chunk_pos.z);
            world_unload_chunk(world, chunk);
        }
    }

    return chunk;
}

static v3
world_get_block_position(const struct world *world, f32 x, f32 y, f32 z)
{
    v3 relative_pos = v3_sub(V3(x, y, z), world->position);
    v3 block_pos = v3_modf(relative_pos, CHUNK_SIZE);

    return block_pos;
}

u32
world_at(struct world *world, f32 x, f32 y, f32 z)
{
    u32 result = 0;

    struct chunk *chunk = world_get_chunk(world, x, y, z);

    if (chunk) {
        v3 block_pos = world_get_block_position(world, x, y, z);
        result = chunk_at(chunk, block_pos.x, block_pos.y, block_pos.z);
    }

    return result;
}

static void
block_texcoords(enum block_type block, v2 *uv)
{
    uv[0] = v2_mulf(v2_add(V2(block, 0), V2(0, 0)), 1 / 16.f);
    uv[1] = v2_mulf(v2_add(V2(block, 0), V2(1, 0)), 1 / 16.f);
    uv[2] = v2_mulf(v2_add(V2(block, 0), V2(0, 1)), 1 / 16.f);
    uv[3] = v2_mulf(v2_add(V2(block, 0), V2(1, 1)), 1 / 16.f);
}

static void
block_texcoords_top(enum block_type block, v2 *uv)
{
    switch (block) {
    case BLOCK_GRASS:
        block_texcoords(BLOCK_GRASS_TOP, uv);
        break;
    case BLOCK_MONITOR_UP:
        block_texcoords(BLOCK_MONITOR_FRONT, uv);
        break;
    case BLOCK_MONITOR_DOWN:
        block_texcoords(BLOCK_MONITOR_BACK, uv);
        break;
    case BLOCK_MONITOR_RIGHT:
    case BLOCK_MONITOR_LEFT:
    case BLOCK_MONITOR_FORWARD:
    case BLOCK_MONITOR_BACKWARD:
        block_texcoords(BLOCK_MONITOR_SIDE, uv);
        break;
    default:
        block_texcoords(block, uv);
        break;
    }
}

static void
block_texcoords_bottom(enum block_type block, v2 *uv)
{
    switch (block) {
    case BLOCK_GRASS:
        block_texcoords(BLOCK_DIRT, uv);
        break;
    case BLOCK_MONITOR_UP:
        block_texcoords(BLOCK_MONITOR_BACK, uv);
        break;
    case BLOCK_MONITOR_DOWN:
        block_texcoords(BLOCK_MONITOR_FRONT, uv);
        break;
    case BLOCK_MONITOR_RIGHT:
    case BLOCK_MONITOR_LEFT:
    case BLOCK_MONITOR_FORWARD:
    case BLOCK_MONITOR_BACKWARD:
        block_texcoords(BLOCK_MONITOR_SIDE, uv);
        break;
    default:
        block_texcoords(block, uv);
        break;
    }
}

static void
block_texcoords_right(enum block_type block, v2 *uv)
{
    switch (block) {
    case BLOCK_MONITOR_LEFT:
        block_texcoords(BLOCK_MONITOR_BACK, uv);
        break;
    case BLOCK_MONITOR_RIGHT:
        block_texcoords(BLOCK_MONITOR_FRONT, uv);
        break;
    case BLOCK_MONITOR_UP:
    case BLOCK_MONITOR_DOWN:
    case BLOCK_MONITOR_FORWARD:
    case BLOCK_MONITOR_BACKWARD:
        block_texcoords(BLOCK_MONITOR_SIDE, uv);
        break;
    default:
        block_texcoords(block, uv);
        break;
    }
}

static void
block_texcoords_left(enum block_type block, v2 *uv)
{
    switch (block) {
    case BLOCK_MONITOR_RIGHT:
        block_texcoords(BLOCK_MONITOR_BACK, uv);
        break;
    case BLOCK_MONITOR_LEFT:
        block_texcoords(BLOCK_MONITOR_FRONT, uv);
        break;
    case BLOCK_MONITOR_UP:
    case BLOCK_MONITOR_DOWN:
    case BLOCK_MONITOR_FORWARD:
    case BLOCK_MONITOR_BACKWARD:
        block_texcoords(BLOCK_MONITOR_SIDE, uv);
        break;
    default:
        block_texcoords(block, uv);
        break;
    }
}

static void
block_texcoords_front(enum block_type block, v2 *uv)
{
    switch (block) {
    case BLOCK_MONITOR_BACKWARD:
        block_texcoords(BLOCK_MONITOR_BACK, uv);
        break;
    case BLOCK_MONITOR_FORWARD:
        block_texcoords(BLOCK_MONITOR_FRONT, uv);
        break;
    case BLOCK_MONITOR_UP:
    case BLOCK_MONITOR_DOWN:
    case BLOCK_MONITOR_RIGHT:
    case BLOCK_MONITOR_LEFT:
        block_texcoords(BLOCK_MONITOR_SIDE, uv);
        break;
    default:
        block_texcoords(block, uv);
        break;
    }
}

static void
block_texcoords_back(enum block_type block, v2 *uv)
{
    switch (block) {
    case BLOCK_MONITOR_FORWARD:
        block_texcoords(BLOCK_MONITOR_BACK, uv);
        break;
    case BLOCK_MONITOR_BACKWARD:
        block_texcoords(BLOCK_MONITOR_FRONT, uv);
        break;
    case BLOCK_MONITOR_RIGHT:
    case BLOCK_MONITOR_LEFT:
    case BLOCK_MONITOR_UP:
    case BLOCK_MONITOR_DOWN:
        block_texcoords(BLOCK_MONITOR_SIDE, uv);
        break;
    default:
        block_texcoords(block, uv);
        break;
    }
}

static void
block_generate_mesh(enum block_type block, i32 x, i32 y, i32 z, 
                    struct world *world, struct mesh *mesh)
{
    v3 pos[8];
    v2 uv[4];

    pos[0] = V3(x + 0.5, y + 0.5, z + 0.5);
    pos[1] = V3(x - 0.5, y + 0.5, z + 0.5);
    pos[2] = V3(x + 0.5, y - 0.5, z + 0.5);
    pos[3] = V3(x - 0.5, y - 0.5, z + 0.5);

    pos[4] = V3(x + 0.5, y + 0.5, z - 0.5);
    pos[5] = V3(x - 0.5, y + 0.5, z - 0.5);
    pos[6] = V3(x + 0.5, y - 0.5, z - 0.5);
    pos[7] = V3(x - 0.5, y - 0.5, z - 0.5);

    u32 (*is_empty)(enum block_type block) = block_is_empty;

    if (block == BLOCK_WATER) {
        is_empty = block_is_not_water;

        if (world_at(world, x, y + 1, z) == BLOCK_AIR) {
            f32 offset = 0.1f;

            pos[0].y -= offset;
            pos[1].y -= offset;
            pos[4].y -= offset;
            pos[5].y -= offset;
        }
    }


    block_texcoords_bottom(block, uv);
    if (is_empty(world_at(world, x, y - 1, z))) {
        mesh_push_quad(mesh, pos[7], pos[6], pos[3], pos[2],
                       uv[0], uv[1], uv[2], uv[3]);
    }

    block_texcoords_top(block, uv);
    if (is_empty(world_at(world, x, y + 1, z))) {
        mesh_push_quad(mesh, pos[4], pos[5], pos[0], pos[1],
                       uv[0], uv[1], uv[2], uv[3]);
    }

    block_texcoords_right(block, uv);
    if (is_empty(world_at(world, x + 1, y, z))) {
        mesh_push_quad(mesh, pos[4], pos[0], pos[6], pos[2],
                       uv[0], uv[1], uv[2], uv[3]);
    }

    block_texcoords_left(block, uv);
    if (is_empty(world_at(world, x - 1, y, z))) {
        mesh_push_quad(mesh, pos[1], pos[5], pos[3], pos[7],
                       uv[0], uv[1], uv[2], uv[3]);
    }

    block_texcoords_front(block, uv);
    if (is_empty(world_at(world, x, y, z + 1))) {
        mesh_push_quad(mesh, pos[0], pos[1], pos[2], pos[3],
                       uv[0], uv[1], uv[2], uv[3]);
    }

    block_texcoords_back(block, uv);
    if (is_empty(world_at(world, x, y, z - 1))) {
        mesh_push_quad(mesh, pos[5], pos[4], pos[7], pos[6],
                       uv[0], uv[1], uv[2], uv[3]);
    }
}

static void
chunk_generate_mesh(struct chunk *chunk, struct world *world, struct mesh *mesh)
{
    v3 min = world_get_chunk_position(world, chunk);
    v3 max = v3_add(min, V3(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE));

    for (i32 z = min.z; z < max.z; z++) {
        for (i32 y = min.y; y < max.y; y++) {
            for (i32 x = min.x; x < max.x; x++) {
                u32 block = world_at(world, x, y, z);
                if (block == BLOCK_AIR) {
                    continue;
                }

                block_generate_mesh(block, x, y, z, world, mesh);
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
    u32 world_size = WORLD_SIZE * WORLD_HEIGHT * WORLD_SIZE;
    world->chunks = arena_alloc(arena, world_size, struct chunk);
    world->width  = WORLD_SIZE;
    world->height = WORLD_HEIGHT;
    world->depth  = WORLD_SIZE;
    f32 offset = -WORLD_SIZE * CHUNK_SIZE / 2.f;
    f32 yoffset = -WORLD_HEIGHT * CHUNK_SIZE / 2.f;
    world->position = V3(offset, yoffset, offset);

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
    world->blocks = arena_alloc(arena, block_size, u16);

    world->unloaded_chunks = arena_alloc(arena, chunk_count, u32);
    world->unloaded_chunk_count = 0;

    while (chunk_count-- > 0) {
        world_unload_chunk(world, world->chunks + chunk_count);
    }

    return 0;
}

void
world_update(struct world *world, v3 player_position)
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
    v3 point = {{ x, y, z }};
    struct chunk *chunk = world_get_chunk(world, x, y, z);
    if (chunk) {
        v3 block_pos = world_get_block_position(world, x, y, z);
        v3i block = v3i_vec3(v3_floor(block_pos));

        u32 i = (block.z * CHUNK_SIZE + block.y) * CHUNK_SIZE + block.x;
        chunk->blocks[i] = block_type;
        world_unload_chunk(world, chunk);

        struct chunk *next_chunk = 0;
        v3 offset, next;
        for (u32 i = 0; i < 3; i++) {
            offset = V3(i == 0, i == 1, i == 2);
            next = v3_add(point, offset);
            next_chunk = world_get_chunk(world, next.x, next.y, next.z);
            world_unload_chunk(world, next_chunk);

            next = v3_sub(point, offset);
            next_chunk = world_get_chunk(world, next.x, next.y, next.z);
            world_unload_chunk(world, next_chunk);
        }
    }
}
