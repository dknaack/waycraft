#ifndef MESH_H
#define MESH_H

struct vertex {
    vec3 position;
    vec2 texcoord;
};

struct mesh {
    struct vertex *vertices;
    u32 *indices;

    u32 vertex_count;
    u32 index_count;
};

static void mesh_push_quad(struct mesh *mesh, 
        vec3 pos0, vec3 pos1, vec3 pos2, vec3 pos3,
        vec2 uv0,  vec2 uv1,  vec2 uv2,  vec2 uv3);

#endif /* MESH_H */ 
