#ifndef BACKEND_H
#define BACKEND_H

#include "types.h"

struct backend_memory {
    void *data;
    usize size;

    u32 is_initialized;
};

#endif /* BACKEND_H */
