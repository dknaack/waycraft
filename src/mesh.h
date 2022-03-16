#ifndef MESH_H
#define MESH_H

struct vertex {
    v3 position;
    v2 texcoord;
};

struct mesh {
    struct vertex *vertices;
    u32 *indices;

    u32 vertex_count;
    u32 index_count;
};

static void mesh_push_quad(struct mesh *mesh, 
        v3 pos0, v3 pos1, v3 pos2, v3 pos3,
        v2 uv0,  v2 uv1,  v2 uv2,  v2 uv3);

#endif /* MESH_H */ 
