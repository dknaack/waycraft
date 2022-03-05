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
    f32 x = 0.5 * (value + 1);

    return x;
}

static void
chunk_init(struct chunk *chunk, u8 *blocks, i32 cx, i32 cy, i32 cz)
{
    chunk->blocks = blocks;
    for (i32 z = 0; z < CHUNK_SIZE; z++) {
        for (i32 x = 0; x < CHUNK_SIZE; x++) {
            f32 nx = BLOCK_SIZE * (cx + x + 0.5);
            f32 nz = BLOCK_SIZE * (cz + z + 0.5);
            i32 height = normalize_height(noise_layered_2d(nx, nz)) * CHUNK_SIZE - cy;

            i32 ymax = MAX(0, MIN(CHUNK_SIZE, height));
            for (i32 y = 0; y < ymax; y++) {
                u32 i = (z * CHUNK_SIZE + y) * CHUNK_SIZE + x;
                if (y + 1 == height && cy + y > 0) {
                    blocks[i] = BLOCK_GRASS;
                } else if (y + 2 >= height && (cy + y > 3 || cy + y < -2)) {
                    blocks[i] = BLOCK_DIRT;
                } else if (y + 2 >= height) {
                    blocks[i] = BLOCK_SAND;
                } else {
                    blocks[i] = BLOCK_STONE;
                }
            }
        }
    }
}

static u32
chunk_at(const struct chunk *chunk, f32 x, f32 y, f32 z)
{
    i32 ix = x;
    i32 iy = y;
    i32 iz = z;

    if ((0 <= ix && ix < CHUNK_SIZE) && (0 <= iy && iy < CHUNK_SIZE) && 
            (0 <= iz && iz < CHUNK_SIZE)) {
        return chunk->blocks[(iz * CHUNK_SIZE + iy) * CHUNK_SIZE + ix];
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

    if ((0 <= cx && cx < world->width) && 
            (0 <= cy && cy < world->height) && 
            (0 <= cz && cz < world->depth)) {
        u32 width = world->width;
        u32 height = world->height;
        u32 chunk_index = (cz * height + cy) * width + cx;
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
mesh_push_quad(struct mesh *mesh, 
        vec3 pos0, vec3 pos1, vec3 pos2, vec3 pos3,
        vec2 uv0, vec2 uv1, vec2 uv2, vec2 uv3)
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

static void
chunk_generate_mesh(struct chunk *chunk, i32 cx, i32 cy, i32 cz, const struct world *world, struct mesh *mesh)
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
    i32 depth  = world->depth;
    i32 height = world->height;
    i32 width  = world->width;

    f32 size = BLOCK_SIZE;
    f32 xmin, ymin, zmin;
    world_chunk_position(world, chunk, &xmin, &ymin, &zmin);
    for (i32 z = zmin; z < zmin + CHUNK_SIZE; z++) {
        for (i32 y = ymin; y < ymin + CHUNK_SIZE; y++) {
            for (i32 x = xmin; x < xmin + CHUNK_SIZE; x++) {
                vec2 uv[4];

                vec3 pos0 = VEC3(
                        size * (x + 0.5 - width / 2.), 
                        size * (y + 0.5 - height), 
                        size * (z + 0.5 - depth / 2.));
                vec3 pos1 = VEC3(
                        size * (x - 0.5 - width / 2.),
                        size * (y + 0.5 - height),
                        size * (z + 0.5 - depth / 2.));
                vec3 pos2 = VEC3(
                        size * (x + 0.5 - width / 2.),
                        size * (y - 0.5 - height),
                        size * (z + 0.5 - depth / 2.));
                vec3 pos3 = VEC3(
                        size * (x - 0.5 - width / 2.),
                        size * (y - 0.5 - height),
                        size * (z + 0.5 - depth / 2.));

                vec3 pos4 = VEC3(
                        size * (x + 0.5 - width / 2.), 
                        size * (y + 0.5 - height), 
                        size * (z - 0.5 - depth / 2.));
                vec3 pos5 = VEC3(
                        size * (x - 0.5 - width / 2.),
                        size * (y + 0.5 - height),
                        size * (z - 0.5 - depth / 2.));
                vec3 pos6 = VEC3(
                        size * (x + 0.5 - width / 2.),
                        size * (y - 0.5 - height),
                        size * (z - 0.5 - depth / 2.));
                vec3 pos7 = VEC3(
                        size * (x - 0.5 - width / 2.),
                        size * (y - 0.5 - height),
                        size * (z - 0.5 - depth / 2.));

                u32 block_type = world_at(world, x, y, z);
                switch (block_type){
                case BLOCK_AIR:
                    continue;
                case BLOCK_GRASS:
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
                    break;
                default:
                    block_texcoords(block_type, uv);
                    if (!world_at(world, x, y - 1, z)) {
                        mesh_push_quad(mesh, pos7, pos6, pos3, pos2,
                                uv[0], uv[1], uv[2], uv[3]);
                    }

static void
chunk_init(struct chunk *chunk, i32 cx, i32 cy, i32 cz)
{
    u32 size = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

    // TODO: allocate memory outside chunk
    u8 *block = chunk->blocks = calloc(size, sizeof(u8));
    for (u32 z = 0; z < CHUNK_SIZE; z++) {
        for (u32 y = 0; y < CHUNK_SIZE; y++) {
            for (u32 x = 0; x < CHUNK_SIZE; x++) {
                f32 nx = 0.1 * (cx + x + 0.5);
                f32 nz = 0.1 * (cz + z + 0.5);

                f32 value = noise_layered_2d(nx, nz);
                *block++ = (value + 1.f) / 2.f * CHUNK_SIZE > y;
            }
        }
    }
}

i32
world_init(struct world *world)
{
    struct mesh mesh = {0};

    u32 size = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
    mesh.vertices = calloc(size * 64, sizeof(struct vertex));
    mesh.indices = calloc(size * 36, sizeof(u32));
    if (!mesh.vertices || !mesh.indices) {
        return -1;
    }

    u32 depth = world->depth;
    u32 width = world->width;
    for (u32 z = 0; z < depth; z++) {
        for (u32 x = 0; x < width; x++) {
            struct chunk *chunk = &world->chunks[z * width + x];
            chunk_init(chunk, x * CHUNK_SIZE, 0, z * CHUNK_SIZE);
        }
    }

    for (u32 z = 0; z < depth; z++) {
        for (u32 x = 0; x < width; x++) {
            mesh.index_count = 0;
            mesh.vertex_count = 0;

            struct chunk *chunk = &world->chunks[z * width + x];
            chunk_generate_mesh(chunk, x * width, 0, z * depth, world, &mesh);

            u32 vao, vbo, ebo;
            gl.GenBuffers(1, &ebo);
            gl.GenBuffers(1, &vbo);
            gl.GenVertexArrays(1, &vao);
            gl.BindVertexArray(vao);

            gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
            gl.BufferData(GL_ARRAY_BUFFER, mesh.vertex_count * 
                    sizeof(*mesh.vertices), mesh.vertices, GL_STATIC_DRAW);

            gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.index_count * 
                    sizeof(*mesh.indices), mesh.indices, GL_STATIC_DRAW);

            gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 
                    sizeof(struct vertex), 
                    (const void *)offsetof(struct vertex, position));
            gl.EnableVertexAttribArray(0);
            gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 
                    sizeof(struct vertex), 
                    (const void *)offsetof(struct vertex, texcoord));
            gl.EnableVertexAttribArray(1);

            chunk->index_count = mesh.index_count;
            chunk->vao = vao;
            chunk->vbo = vbo;
            chunk->ebo = ebo;
        }
    }

    gl.BindVertexArray(0);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    gl.BindBuffer(GL_ARRAY_BUFFER, 0);
    free(mesh.vertices);
    free(mesh.indices);

    {
        i32 width, height, comp;
        u8 *data;

        if (!(data = stbi_load("res/planks.png", &width, &height, &comp, 3))) {
            fprintf(stderr, "Failed to load texture\n");
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
    }

    return 0;
}

void
world_update(struct world *world, vec3 player_position)
{
}

void
world_render(const struct world *world)
{
    u32 width = world->width;
    for (u32 z = 0; z < world->depth; z++) {
        for (u32 x = 0; x < world->width; x++) {
            struct chunk *chunk = &world->chunks[z * width + x];

            gl.BindVertexArray(chunk->vao);
            gl.DrawElements(GL_TRIANGLES, chunk->index_count, 
                    GL_UNSIGNED_INT, 0);
        }
    }
}

void
world_finish(struct world *world)
{
    u32 width = world->width;
    for (u32 z = 0; z < world->depth; z++) {
        for (u32 x = 0; x < world->width; x++) {
            struct chunk *chunk = &world->chunks[z * width + x];

            gl.DeleteVertexArrays(1, &chunk->vao);
            gl.DeleteBuffers(1, &chunk->vbo);
            gl.DeleteBuffers(1, &chunk->ebo);
            free(chunk->blocks);
        }
    }

    gl.DeleteTextures(1, &world->texture);
}
