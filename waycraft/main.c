#include <waycraft/types.h>
#include <waycraft/memory.h>
#include <waycraft/backend.h>
#include <waycraft/game.h>
#include <waycraft/compositor.h>
#include <waycraft/gl.h>
#include <waycraft/log.h>
#include <waycraft/stb_image.h>
#include <waycraft/timer.h>
#include <waycraft/xdg-shell-protocol.h>

#include "waycraft/gl.c"
#include "waycraft/log.c"
#include "waycraft/math.c"
#include "waycraft/memory.c"
#include "waycraft/compositor.c"
#include "waycraft/backend_x11.c"
#include "waycraft/game.c"

int
main(void)
{
    return x11_main();
}
