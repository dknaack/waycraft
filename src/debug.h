#ifndef DEBUG_H
#define DEBUG_H

void debug_init(void);
void debug_render(mat4 view, mat4 projection);
void debug_set_color(f32 r, f32 g, f32 b);
void debug_line(v3 start, v3 end);
void debug_cube(v3 min, v3 max);

#endif /* DEBUG_H */ 
