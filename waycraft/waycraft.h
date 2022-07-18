#include <waycraft/types.h>
#include <waycraft/util.h>
#include <waycraft/platform.h>
#include <waycraft/compositor.h>
#include <waycraft/gl.h>
#include <waycraft/stb_image.h>

struct game_code {
	struct platform_memory memory;
	char *path;
	game_update_t *update;
	ino_t ino;
	void *handle;
};

struct egl_context {
	EGLDisplay *display;
	EGLSurface *surface;
	EGLContext *context;
};

struct platform_event_array {
	struct platform_event *at;
	u32 max_count;
	u32 count;
};

struct platform_task {
	platform_task_callback_t *callback;
	void *data;
};

struct platform_task_queue {
	struct platform_task tasks[256];
	pthread_mutex_t lock;
	pthread_cond_t new_task;
	u32 completed_task_count;
	u32 task_count;
	u32 next_read;
	u32 next_write;
};

static void game_load(struct game_code *game);
static i32 egl_init(struct egl_context *egl, usize window);
static void egl_finish(struct egl_context *egl);
static struct platform_event *push_event(
	struct platform_event_array *events, u32 type);
