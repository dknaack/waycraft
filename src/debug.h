#ifndef DEBUG_H
#define DEBUG_H

void debug_init(void);
void debug_render(mat4 view, mat4 projection);
void debug_set_color(f32 r, f32 g, f32 b);
void debug_line(vec3 start, vec3 end);
void debug_cube(vec3 min, vec3 max);

#endif /* DEBUG_H */ 
