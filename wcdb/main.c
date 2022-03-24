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

    struct wl_buffer *buffer;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
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

    xdg_surface_ack_configure(xdg_surface, serial);

    struct wl_buffer *buffer = client->buffer;
    struct wl_surface *surface = client->surface;
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, u32 serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void
wl_registry_global(void *data, struct wl_registry *registry, u32 name,
                   const char *interface, u32 version)
{
    struct wcdb_client *client = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        struct wl_compositor *compositor = wl_registry_bind(
            registry, name, &wl_compositor_interface, 5);
        struct wl_surface *surface = 
            wl_compositor_create_surface(compositor);

        client->compositor = compositor;
        client->surface = surface;
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        struct wl_shm *shm = client->shm = 
            wl_registry_bind(registry, name, &wl_shm_interface, 1);

        i32 width = 1920;
        i32 height = 1080;
        i32 stride = width * 4;
        i32 shm_pool_size = height * stride * 2;

        i32 fd = allocate_shm_file(shm_pool_size);
        i32 prot = PROT_READ | PROT_WRITE;
        u8 *pool_data = mmap(0, shm_pool_size, prot, MAP_SHARED, fd, 0);

        struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, shm_pool_size);

        i32 index = 0;
        i32 offset = height * stride * index;
        struct wl_buffer *buffer = wl_shm_pool_create_buffer(
            pool, offset, width, height, stride, WL_SHM_FORMAT_XRGB8888);

        u32 *pixels = (u32 *)pool_data;
        for (u32 y = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++) {
                if ((x + y / 8 * 8) % 16 < 8) {
                    pixels[y * width + x] = 0xff666666;
                } else {
                    pixels[y * width + x] = 0xffeeeeee;
                }
            }
        }

        struct wl_surface *surface = client->surface;
        wl_surface_attach(surface, buffer, 0, 0);
        wl_surface_damage(surface, 0, 0, UINT32_MAX, UINT32_MAX);
        wl_surface_commit(surface);

        client->buffer = buffer;
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        struct xdg_wm_base *xdg_wm_base = wl_registry_bind(
            registry, name, &xdg_wm_base_interface, version);
        xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, client);

        struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(
            xdg_wm_base, client->surface);
        xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, client);
        struct xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(
            xdg_surface);
        xdg_toplevel_set_title(xdg_toplevel, "wcdb");

        client->xdg_wm_base = xdg_wm_base;
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

    client.display = wl_display_connect(0);
    if (!client.display) {
        fprintf(stderr, "Failed to connect to wayland display\n");
        return 1;
    }

    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &wl_registry_listener, &client);

    wl_display_roundtrip(client.display);

    while (wl_display_dispatch(client.display)) {

    }

    return 0;
}
