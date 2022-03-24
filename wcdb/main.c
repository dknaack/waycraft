#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>

#include <waycraft/types.h>
#include <wcdb/xdg-shell-protocol.h>

#include "xdg-shell-protocol.c"

struct wcdb_client {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *xdg_wm_base;

    struct wl_shm_pool *shm_pool;
    struct wl_buffer *buffer;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;

    u32 *pixels;
    u8 is_active;
};

static void
randname(char *buf)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int
create_shm_file(void)
{
	int retries = 100;
	do {
		char name[] = "/wl_shm-XXXXXX";
		randname(name + sizeof(name) - 7);
		--retries;
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);
	return -1;
}

static int
allocate_shm_file(size_t size)
{
	int fd = create_shm_file();
	if (fd < 0)
		return -1;
	int ret;
	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static void
do_nothing()
{
}

static void
xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, u32 serial)
{
    struct wcdb_client *client = data;
    i32 width = 256;
    i32 height = 256;
    i32 stride = width * 4;

    xdg_surface_ack_configure(xdg_surface, serial);

    struct wl_buffer *buffer = client->buffer;
    if (!buffer) {
        i32 shm_pool_size = height * stride * 2;

        struct wl_shm_pool *pool = client->shm_pool;
        if (!pool) {

            i32 fd = allocate_shm_file(shm_pool_size);
            i32 prot = PROT_READ | PROT_WRITE;
            client->pixels = mmap(0, shm_pool_size, prot, MAP_SHARED, fd, 0);

            client->shm_pool = pool = wl_shm_create_pool(client->shm, fd, shm_pool_size);
        }

        i32 index = 0;
        i32 offset = height * stride * index;
        buffer = wl_shm_pool_create_buffer(pool, offset, width, height, stride,
                                           WL_SHM_FORMAT_XRGB8888);

        struct wl_surface *surface = client->surface;
        wl_surface_attach(surface, buffer, 0, 0);
        wl_surface_damage(surface, 0, 0, UINT32_MAX, UINT32_MAX);
        wl_surface_commit(surface);

        client->buffer = buffer;
    }

    struct wl_surface *surface = client->surface;
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void
xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                       i32 width, i32 height, struct wl_array *states)
{
    struct wcdb_client *client = data;
    i32 is_active = 0;
    i32 *state;
    wl_array_for_each(state, states) {
        if (*state == XDG_TOPLEVEL_STATE_ACTIVATED) {
            is_active = 1;
        }
    }

    client->is_active = is_active;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure        = xdg_toplevel_configure,
    .close            = do_nothing,
    .configure_bounds = do_nothing,
};

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, u32 serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static const struct wl_callback_listener wl_surface_frame_listener;

static void
wl_surface_frame_done(void *data, struct wl_callback *cb, u32 time)
{
    struct wcdb_client *client = data;

    wl_callback_destroy(cb);
    cb = wl_surface_frame(client->surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, client);

    i32 height = 256;
    i32 width = 256;

    u32 *pixels = client->pixels;
    u32 white = client->is_active ? 0xff666666 : 0xffeeeeee;
    u32 black = client->is_active ? 0xffeeeeee : 0xff666666;
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            if ((x + y / 8 * 8) % 16 < 8) {
                pixels[y * width + x] = black;
            } else {
                pixels[y * width + x] = white;
            }
        }
    }

    struct wl_surface *surface = client->surface;
    wl_surface_damage(surface, 0, 0, UINT32_MAX, UINT32_MAX);
    wl_surface_commit(surface);
}

static const struct wl_callback_listener wl_surface_frame_listener = {
    .done = wl_surface_frame_done,
};

static void
wl_registry_global(void *data, struct wl_registry *registry, u32 name,
                   const char *interface, u32 version)
{
    struct wcdb_client *client = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        struct wl_compositor *compositor = wl_registry_bind(
            registry, name, &wl_compositor_interface, 4);
        struct wl_surface *surface = 
            wl_compositor_create_surface(compositor);

        client->compositor = compositor;
        client->surface = surface;
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        client->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        struct xdg_wm_base *xdg_wm_base = client->xdg_wm_base = wl_registry_bind(
            registry, name, &xdg_wm_base_interface, version);
        xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, client);
    }
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = wl_registry_global,
    .global_remove = do_nothing,
};

int
main(void)
{
    struct wcdb_client client = {0};

    const char *display_env = getenv("WAYLAND_DISPLAY");
    client.display = wl_display_connect(display_env);
    if (!client.display) {
        fprintf(stderr, "Failed to connect to wayland display: %s\n",
                display_env);
        return 1;
    }

    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &wl_registry_listener, &client);

    wl_display_roundtrip(client.display);
    client.xdg_surface = xdg_wm_base_get_xdg_surface(client.xdg_wm_base, client.surface);
    xdg_surface_add_listener(client.xdg_surface, &xdg_surface_listener, &client);
    client.xdg_toplevel = xdg_surface_get_toplevel(client.xdg_surface);
    xdg_toplevel_add_listener(client.xdg_toplevel, &xdg_toplevel_listener, &client);
    xdg_toplevel_set_title(client.xdg_toplevel, "wcdb");
    wl_surface_commit(client.surface);

    struct wl_callback *cb = wl_surface_frame(client.surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, &client);

    while (wl_display_dispatch(client.display)) {

    }

    return 0;
}
