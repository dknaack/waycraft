#ifndef BACKEND_H
#define BACKEND_H

struct backend_memory {
    void *data;
    u64 size;

    u32 is_initialized;
};

#endif /* BACKEND_H */ 
