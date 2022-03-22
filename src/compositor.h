#ifndef COMPOSITOR_H
#define COMPOSITOR_H

struct egl;

void compositor_update(struct backend_memory *memory, struct egl *egl);
void compositor_finish(struct backend_memory *memory);

#endif /* COMPOSITOR_H */ 
