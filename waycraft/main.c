#include "compositor.c"
#include "debug.c"
#include "game.c"
#include "gl.c"
#include "inventory.c"
#include "math.c"
#include "memory.c"
#include "noise.c"
#include "timer.c"
#include "world.c"
#include "x11_backend.c"
#include "xdg-shell-protocol.c"
#include "xwayland.c"

int
main(void)
{
    return x11_main();
}
