#ifndef EGL_H
#define EGL_H

struct egl {
    EGLDisplay *display;
    EGLContext *context;
    EGLSurface *surface;
};

i32 egl_init(struct egl *egl, EGLDisplay *display);
void egl_finish(struct egl *egl);

#endif /* EGL_H */ 
