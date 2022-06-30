#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

#include <waycraft/types.h>
#include <waycraft/memory.h>
#include <waycraft/platform.h>
#include <waycraft/compositor.h>
#include <waycraft/gl.h>
#include <waycraft/log.h>
#include <waycraft/stb_image.h>
#include <waycraft/timer.h>
#include <waycraft/xdg-shell-protocol.h>

#include "waycraft/log.c"
#include "waycraft/compositor.c"

struct game_code {
	struct platform_memory memory;
	char *path;
	game_update_t *update;
	ino_t ino;
	void *handle;
};

typedef void platform_task_callback_t(void *data);

struct task {
	platform_task_callback_t *callback;
	void *data;
};

struct platform_task_queue {
	struct task tasks[256];
	pthread_mutex_t lock;
	pthread_cond_t new_task;
	u32 completed_task_count;
	u32 task_count;
	u32 next_read;
	u32 next_write;
};

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

static bool
execute_task(struct platform_task_queue *queue)
{
	bool executed_task = false;

	if (queue->next_read != queue->next_write) {
		executed_task = true;

		pthread_mutex_lock(&queue->lock);
		struct task task = queue->tasks[queue->next_read];
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

	struct task *task = &queue->tasks[queue->next_write];
	task->callback = callback;
	task->data = data;

	queue->next_write = (queue->next_write + 1) % queue->task_count;
	queue->task_count++;

	pthread_cond_broadcast(&queue->new_task);
	pthread_mutex_unlock(&queue->lock);
}

#include "waycraft/x11.c"

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

    return x11_main(&game, &compositor_memory, &gl);
}
