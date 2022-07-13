#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

#include <waycraft/waycraft.h>

#include "waycraft/log.c"
#include "waycraft/compositor.c"
#include "waycraft/x11.c"
#include "waycraft/drm.c"

static void
game_load(struct game_code *game)
{
	struct stat st = {0};
	if (stat(game->path, &st) != -1) {
		if (game->ino != st.st_ino || !game->handle) {
			if (game->handle) {
				dlclose(game->handle);
				game->handle = NULL;
				game->update = NULL;
			}

			game->handle = dlopen(game->path, RTLD_LAZY);
			game->ino = st.st_ino;
		}

		if (game->handle) {
			*(void **)&game->update = dlsym(game->handle, "game_update");
		}
	}
}

static struct platform_event *
push_event(struct platform_event_array *events, u32 type)
{
	struct platform_event *result = NULL;

	if (events->count < events->max_count) {
		result = &events->at[events->count++];
		result->type = type;
	}

	return result;
}

static bool
execute_task(struct platform_task_queue *queue)
{
	bool executed_task = false;

	if (queue->next_read != queue->next_write) {
		executed_task = true;

		pthread_mutex_lock(&queue->lock);
		struct platform_task task = queue->tasks[queue->next_read];
		queue->next_read = (queue->next_read + 1) % queue->task_count;
		pthread_mutex_unlock(&queue->lock);

		assert(task.callback);
		task.callback(task.data);

		pthread_mutex_lock(&queue->lock);
		queue->completed_task_count++;
		pthread_mutex_unlock(&queue->lock);
	}

	return executed_task;
}

static void *
thread_proc(void *data)
{
	struct platform_task_queue *queue = data;
	sigset_t signal_set;
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &signal_set, NULL);

	for (;;) {
		if (!execute_task(queue)) {
			pthread_cond_wait(&queue->new_task, &queue->lock);
		}
	}

	return NULL;
}

static void
add_task(struct platform_task_queue *queue, platform_task_callback_t *callback, void *data)
{
	pthread_mutex_lock(&queue->lock);
	assert(queue->next_write + 1 != queue->next_read);

	struct platform_task *task = &queue->tasks[queue->next_write];
	task->callback = callback;
	task->data = data;

	queue->next_write = (queue->next_write + 1) % queue->task_count;
	queue->task_count++;

	pthread_cond_broadcast(&queue->new_task);
	pthread_mutex_unlock(&queue->lock);
}

static i32
egl_init(struct egl_context *egl, usize window)
{
	egl->display = eglGetDisplay(0);
	if (egl->display == EGL_NO_DISPLAY) {
		log_err("Failed to get the EGL display");
		goto error_get_display;
	}

	i32 major, minor;
	if (!eglInitialize(egl->display, &major, &minor)) {
		log_err("Failed to initialize EGL");
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
	if (!eglChooseConfig(egl->display, config_attributes, &config, 1, &config_count)) {
		log_err("Failed to choose config");
		goto error_choose_config;
	}

	if (config_count != 1) {
		goto error_choose_config;
	}

	egl->surface = eglCreateWindowSurface(egl->display, config, window, 0);

	if (!eglBindAPI(EGL_OPENGL_API)) {
		log_err("Failed to bind the opengl api");
		goto error_bind_api;
	}

	EGLint context_attributes[] = {
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 3,
		EGL_NONE
	};

	egl->context = eglCreateContext(egl->display, config, EGL_NO_CONTEXT,
		context_attributes);
	if (egl->context == EGL_NO_CONTEXT) {
		log_err("Failed to create the EGL context");
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

static void
egl_finish(struct egl_context *egl)
{
	eglDestroyContext(egl->display, egl->context);
	eglDestroySurface(egl->display, egl->surface);
	eglTerminate(egl->display);
}

int
main(void)
{
	i32 result = 0;

	// NOTE: initialize the task queue and threads
	struct platform_task_queue queue = {0};
	result = pthread_mutex_init(&queue.lock, NULL);
	assert(result == 0);

	result = pthread_cond_init(&queue.new_task, NULL);
	assert(result == 0);

	pthread_attr_t attr;
	result = pthread_attr_init(&attr);
	assert(result == 0);

	u32 processor_count = get_nprocs();
	pthread_t *threads = calloc(processor_count, sizeof(*threads));
	for (u32 i = 1; i < processor_count; i++) {
		result = pthread_create(&threads[i], &attr, thread_proc, &queue);
		assert(result == 0);

		pthread_detach(threads[i]);
	}

	struct opengl_api gl = {0};
	struct platform_api platform = {0};
	platform.add_task = add_task;
	platform.queue = &queue;

	// NOTE: initialize the game
	struct game_code game = {0};
	game.path = "./build/libgame.so";
	game.memory.size = MB(256);
	game.memory.data = calloc(game.memory.size, 1);
	game.memory.gl = &gl;
	game.memory.platform = &platform;

	// NOTE: initialize the compositor
	struct platform_memory compositor_memory = {0};
	compositor_memory.size = MB(64);
	compositor_memory.data = calloc(compositor_memory.size, 1);
	compositor_memory.gl = &gl;
	compositor_memory.platform = &platform;

	(void)arena_suballoc;
#if 0
	if ((result = drm_main(&game, &compositor_memory, &gl)) >= 0) {
	} else if ((result = wayland_main(&game, &compositor_memory, &gl)) >= 0) {
		// NOTE: successfully initialized wayland backend
	} else
#endif
		if ((result = x11_main(&game, &compositor_memory, &gl)) >= 0) {
		// NOTE: successfully initialized x11 backend
	} else {
		log_err("Failed to find backend");
		result = 1;
	}

	free(game.memory.data);
	free(compositor_memory.data);
	return result;
}
