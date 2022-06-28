#include <dlfcn.h>
#include <sys/stat.h>

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
#include "waycraft/math.c"
#include "waycraft/memory.c"
#include "waycraft/compositor.c"

struct game_code {
	struct platform_memory memory;
	char *path;
	game_update_t *update;
	ino_t ino;
	void *handle;
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

#include "waycraft/x11.c"

int
main(void)
{
	struct game_code game = {0};
	game.path = "./build/libgame.so";
	game.memory.size = MB(256);
	game.memory.data = calloc(game.memory.size, 1);

    return x11_main(game);
}
