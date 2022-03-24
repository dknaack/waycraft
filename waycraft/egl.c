#include "egl.h"

void
egl_finish(struct egl *egl)
{
    eglDestroyContext(egl->display, egl->context);
    eglDestroySurface(egl->display, egl->surface);
    eglTerminate(egl->display);
}
