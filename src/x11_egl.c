#include "egl.h"
#include "x11_egl.h"

i32
x11_egl_init(struct egl *egl, struct x11_window *window)
{
    egl->display = eglGetDisplay(window->display);
    if (egl->display == EGL_NO_DISPLAY) {
        goto error_get_display;
    }

    i32 major, minor;
    if (!eglInitialize(egl->display, &major, &minor)) {
        goto error_initialize;
    }

    EGLint config_attributes[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    EGLConfig config;
    EGLint config_count;
    if (!eglChooseConfig(egl->display, config_attributes, &config, 1, 
            &config_count)) {
        goto error_choose_config;
    }

    if (config_count != 1) {
        goto error_choose_config;
    }

    egl->surface = eglCreateWindowSurface(egl->display,
            config, window->drawable, 0);

    if (!eglBindAPI(EGL_OPENGL_API)) {
        goto error_bind_api;
    }

    EGLint context_attributes[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_NONE
    };

    egl->context = eglCreateContext(egl->display, config, 
            EGL_NO_CONTEXT, context_attributes);
    if (egl->context == EGL_NO_CONTEXT) {
        goto error_context;
    }

    eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context);

    return 0;
error_context:
    eglDestroySurface(egl->display, egl->surface);
error_bind_api:
error_choose_config:
error_initialize:
    eglTerminate(egl->display);
error_get_display:
    return -1;
}
