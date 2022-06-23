#include <assert.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <errno.h>
#include <fcntl.h>
#include <GL/gl.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server.h>
#include <xcb/composite.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "waycraft/types.h"
#include "waycraft/memory.h"
#include "waycraft/block.h"
#include "waycraft/renderer.h"
#include "waycraft/inventory.h"
#include "waycraft/world.h"
#include "waycraft/game.h"
#include "waycraft/xwayland.h"
#include "waycraft/compositor.h"
#include "waycraft/gl.h"
#include "waycraft/log.h"
#include "waycraft/stb_image.h"
#include "waycraft/timer.h"
#include "waycraft/xdg-shell-protocol.h"

#include "waycraft/gl.c"
#include "waycraft/log.c"
#include "waycraft/math.c"
#include "waycraft/memory.c"
#include "waycraft/noise.c"
#include "waycraft/xwayland.c"
#include "waycraft/compositor.c"
#include "waycraft/backend_x11.c"
#include "waycraft/renderer.c"
#include "waycraft/block.c"
#include "waycraft/debug.c"
#include "waycraft/inventory.c"
#include "waycraft/timer.c"
#include "waycraft/world.c"
#include "waycraft/xdg-shell-protocol.c"
#include "waycraft/game.c"

int
main(void)
{
    return x11_main();
}
