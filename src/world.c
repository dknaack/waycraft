#include "types.h"
#include "world.h"
#include "gl.h"

static u32
chunk_at(const struct chunk *chunk, u32 x, u32 y, u32 z)
{
    if ((0 <= x && x < CHUNK_SIZE) && (0 <= y && y < CHUNK_SIZE) && 
            (0 <= z && z < CHUNK_SIZE)) {
        return chunk->blocks[(z * CHUNK_SIZE + y) * CHUNK_SIZE + x];
    } else {
        return 0;
    }
}

static u32
world_at(const struct world *world, u32 x, u32 y, u32 z)
{
    u32 cx = x / CHUNK_SIZE;
    u32 cz = z / CHUNK_SIZE;

    if (cx >= world->width || cz >= world->depth) {
        return 0;
    } else {
        u32 width = world->width;
        const struct chunk *chunk = &world->chunks[cz * width + cx];
        return chunk_at(chunk, x % CHUNK_SIZE, y % CHUNK_SIZE, z % CHUNK_SIZE);
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
    u32 depth  = world->depth;
    u32 height = world->height;
    u32 width  = world->width;

    f32 size = 0.1;
    for (u32 z = cz; z < cz + CHUNK_SIZE; z++) {
        for (u32 y = cy; y < cy + CHUNK_SIZE; y++) {
            for (u32 x = cx; x < cx + CHUNK_SIZE; x++) {
                if (world_at(world, x, y, z) != 0) {
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

                    if (!world_at(world, x, y - 1, z)) {
                        mesh_push_quad(mesh, pos6, pos7, pos2, pos3);
                    }

                    if (!world_at(world, x, y + 1, z)) {
                        mesh_push_quad(mesh, pos4, pos5, pos0, pos1);
                    }

                    if (!world_at(world, x + 1, y, z)) {
                        mesh_push_quad(mesh, pos4, pos0, pos6, pos2);
                    }

                    if (!world_at(world, x - 1, y, z)) {
                        mesh_push_quad(mesh, pos1, pos5, pos3, pos7);
                    }

                    if (!world_at(world, x, y, z + 1)) {
                        mesh_push_quad(mesh, pos0, pos1, pos2, pos3);
                    }

                    if (!world_at(world, x, y, z - 1)) {
                        mesh_push_quad(mesh, pos5, pos4, pos7, pos6);
                    }
                }
            }
        }
    }
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
