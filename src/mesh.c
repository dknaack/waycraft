#include "mesh.h"

void
mesh_push_quad(struct mesh *mesh, 
        v3 pos0, v3 pos1, v3 pos2, v3 pos3,
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
