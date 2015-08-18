#include "game.h"
#include "disp.h"
#include <stdio.h>
#include <malloc.h>

static volatile unsigned int    init_sync = 0;

// Exported functions

__attribute__((constructor)) void render()
{
    // Wait for main initialization
    while (!init_sync);

    Game *game = gameInstance();

    // Render slices as soon as it's available
    while (1) {
        dispRenderSlice(game);
    }
}

__attribute__((constructor)) void main()
{
    unsigned int w, h, p;
    unsigned int i, j;

    giet_tty_alloc(0);
    giet_tty_printf("[RAYCAST] entering main()\n");

    // Initialize heap for each cluster
    giet_procs_number(&w, &h, &p);
    for (i = 0; i < w; i++)
        for (j = 0; j < h; j++)
            heap_init(i, j);

    // Initialize game
    dispInit();

    init_sync = 1;

    while (1) {
        gameRun();
    }
}
