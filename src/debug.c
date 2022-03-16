#include "debug.h"
#include "gl.h"

#define DEBUG_VERTEX_BUFFER_SIZE MB(8)

struct debug_vertex {
    v3 position;
    v3 color;
};

struct debug_state {
    struct debug_vertex *vertices;
    u32 vertex_count;
    u32 program;
    u32 model;
    u32 view;
    u32 projection;
    u32 vertex_array;
    u32 vertex_buffer;
    v3 color;
};

static const u8 *debug_vertex_shader_source = (u8 *)
    "#version 330\n"
    "layout (location = 0) in v3 in_pos;"
    "layout (location = 1) in v3 in_color;"
    "uniform mat4 model;"
    "uniform mat4 view;"
    "uniform mat4 projection;"
    "out v3 color;"
    "void main() {"
    "   gl_Position = projection * view * model * v4(in_pos, 1.);"
    "   color = in_color;"
    "}"
    ;

static const u8 *debug_fragment_shader_source = (u8 *)
    "#version 330\n"
    "in v3 color;\n"
    "out v4 frag_color;\n"
    "void main() {"
    "   frag_color = v4(color, 1.);"
    "}"
    ;

static struct debug_state debug = {0};

void
debug_init(void)
{
    debug.vertices = calloc(DEBUG_VERTEX_BUFFER_SIZE, 1);
    debug.vertex_count = 0;

    gl.LineWidth(4.f);

    debug.program = gl_program_create(debug_vertex_shader_source, 
            debug_fragment_shader_source);
    debug.model      = gl.GetUniformLocation(debug.program, "model");
    debug.view       = gl.GetUniformLocation(debug.program, "view");
    debug.projection = gl.GetUniformLocation(debug.program, "projection");

    gl.GenVertexArrays(1, &debug.vertex_array);
    gl.GenBuffers(1, &debug.vertex_buffer);
    gl.BindVertexArray(debug.vertex_array);
    gl.BindBuffer(GL_ARRAY_BUFFER, debug.vertex_buffer);
    gl.BufferData(GL_ARRAY_BUFFER, DEBUG_VERTEX_BUFFER_SIZE, 0, GL_STREAM_DRAW);

    gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 
            sizeof(struct debug_vertex), 
            (const void *)offsetof(struct debug_vertex, position));
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 
            sizeof(struct debug_vertex), 
            (const void *)offsetof(struct debug_vertex, color));
    gl.EnableVertexAttribArray(1);
}

void
debug_set_color(f32 r, f32 g, f32 b)
{
    debug.color = V3(r, g, b);
}

void
debug_line(v3 start, v3 end)
{
    struct debug_vertex *vertex = debug.vertices + debug.vertex_count;
    assert(debug.vertex_count * 
           sizeof(struct debug_vertex) < DEBUG_VERTEX_BUFFER_SIZE);

    vertex->position = start;
    vertex->color    = debug.color;
    vertex++;

    vertex->position = end;
    vertex->color    = debug.color;
    vertex++;

    debug.vertex_count += 2;
}

void
debug_cube(v3 min, v3 max)
{
    v3 pos0 = V3(min.x, min.y, min.z);
    v3 pos1 = V3(max.x, min.y, min.z);
    v3 pos2 = V3(min.x, max.y, min.z);
    v3 pos3 = V3(max.x, max.y, min.z);
    v3 pos4 = V3(min.x, min.y, max.z);
    v3 pos5 = V3(max.x, min.y, max.z);
    v3 pos6 = V3(min.x, max.y, max.z);
    v3 pos7 = V3(max.x, max.y, max.z);

    debug_line(pos0, pos1);
    debug_line(pos0, pos2);
    debug_line(pos1, pos3);
    debug_line(pos2, pos3);

    debug_line(pos4, pos5);
    debug_line(pos4, pos6);
    debug_line(pos5, pos7);
    debug_line(pos6, pos7);

    debug_line(pos0, pos4);
    debug_line(pos1, pos5);
    debug_line(pos2, pos6);
    debug_line(pos3, pos7);
}

void
debug_render(m4x4 view, m4x4 projection)
{
    gl.Disable(GL_DEPTH_TEST);
    gl.PolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    gl.BindVertexArray(debug.vertex_array);
    gl.BindBuffer(GL_ARRAY_BUFFER, debug.vertex_buffer);
    gl.BufferSubData(GL_ARRAY_BUFFER, 0, 
            debug.vertex_count * sizeof(struct debug_vertex), debug.vertices);

    gl.UseProgram(debug.program);
    gl.UniformMatrix4fv(debug.model, 1, GL_FALSE, m4x4_id(1).e);
    gl.UniformMatrix4fv(debug.projection, 1, GL_FALSE, projection.e);
    gl.UniformMatrix4fv(debug.view, 1, GL_FALSE, view.e);

    gl.DrawArrays(GL_LINES, 0, debug.vertex_count);

    gl.PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gl.Enable(GL_DEPTH_TEST);

    debug.vertex_count = 0;
}
