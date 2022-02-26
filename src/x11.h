#ifndef X11_H
#define X11_H

struct x11_window {
    Display *display;
    Drawable drawable;
    Visual *visual;
    XIM xim;
    XIC xic;
    GC gc;
    Atom net_wm_name;
    Atom wm_delete_win;
    u32 width, height;
    uint is_open;
};

i32 x11_window_init(struct x11_window *window);
void x11_window_finish(struct x11_window *window);
void x11_window_poll_events(struct x11_window *window);

#endif /* X11_H */ 
