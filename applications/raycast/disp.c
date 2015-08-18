#include "disp.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include <hard_config.h>
#include <user_sqt_lock.h>

#define FIELD_OF_VIEW   (70.f * M_PI / 180.f)   // Camera field of view
#define TEX_SIZE        (32)                    // Texture size in pixels
#define CEILING_COLOR   (0xBB)                  // lite gray
#define FLOOR_COLOR     (0x33)                  // dark gray

// Globals

static unsigned char*           buf[2];         // framebuffer
static void *                   sts[2];         // for fbf_cma
static unsigned int             cur_buf;        // current framebuffer
static volatile unsigned int    slice_x;        // slice index (shared)
static sqt_lock_t               slice_x_lock;   // slice index lock
static volatile unsigned int    slice_cnt;      // slice count (shared)

// Textures indexed by block number
static unsigned char *g_tex[] =
{
    NULL, // 0
    NULL, // rock
    NULL, // door
    NULL, // handle
    NULL, // wood
};

// Local functions

static void dispDrawColumnTex(int x, int y0, int y1, unsigned char *line)
{
    int y = (y0 >= 0 ? y0 : 0);
    int ymax = (y1 < FBUF_Y_SIZE ? y1 : FBUF_Y_SIZE);

    for (; y < ymax; y++) {
        // Find texture coordinate
        int ty = (y - y0) * TEX_SIZE / (y1 - y0);

        buf[cur_buf][y * FBUF_X_SIZE + x] = line[ty];
    }
}

static void dispDrawColumnSolid(int x, int y0, int y1, unsigned char color)
{
    int y = (y0 >= 0 ? y0 : 0);
    int ymax = (y1 < FBUF_Y_SIZE ? y1 : FBUF_Y_SIZE);

    for (; y < ymax; y++) {
        buf[cur_buf][y * FBUF_X_SIZE + x] = color;
    }
}

static void dispDrawSlice(Game *game, int x, int height, int type, int tx)
{
    // Ceiling
    dispDrawColumnSolid(x,
                        0,
                        (FBUF_Y_SIZE - height) / 2,
                        CEILING_COLOR);

    // Wall
    unsigned char *tex = g_tex[type];

    if (tex) {
        // Draw a texture slice
        dispDrawColumnTex(x,
                          (FBUF_Y_SIZE - height) / 2,
                          (FBUF_Y_SIZE + height) / 2,
                          &tex[tx * TEX_SIZE]);
    }
    else {
        // Draw a solid color slice
        dispDrawColumnSolid(x,
                            (FBUF_Y_SIZE - height) / 2,
                            (FBUF_Y_SIZE + height) / 2,
                            0xFF);
    }

    // Floor
    dispDrawColumnSolid(x,
                        (FBUF_Y_SIZE + height) / 2,
                        FBUF_Y_SIZE,
                        FLOOR_COLOR);
}

static float dispRaycast(Game *game, int *type, float *tx, float angle)
{
    float x = game->player.x;
    float y = game->player.y;

    // Camera is inside a block.
    // Return a minimal distance to avoid a division by zero.
    if ((gameLocate(floor(x), floor(y))) != 0) {
        *type = 0;
        *tx = 0.f;
        return 0.0001f;
    }

    // Precompute
    float vsin = sin(angle);
    float vcos = cos(angle);
    float vtan = vsin / vcos;

    // Calculate increments
    int incix = (vcos > 0.f) - (vcos < 0.f);
    int inciy = (vsin > 0.f) - (vsin < 0.f);
    float incfx = inciy / vtan;
    float incfy = incix * vtan;

    // Calculate start position
    int ix = floor(x) + (incix > 0);
    int iy = floor(y) + (inciy > 0);
    float fx = x + incfx * fabs(floor(y) + (inciy > 0) - y);
    float fy = y + incfy * fabs(floor(x) + (incix > 0) - x);

    // Find the first colliding tile in each direction
    while (incix && gameLocate(ix - (incix < 0), fy) == 0)
    {
        ix += incix;
        fy += incfy;
    }
    while (inciy && gameLocate(fx, iy - (inciy < 0)) == 0)
    {
        fx += incfx;
        iy += inciy;
    }

    // Find the shortest ray
    float dx = (incix) ? ((ix - x) / vcos) : 0xFFFF;
    float dy = (inciy) ? ((iy - y) / vsin) : 0xFFFF;

    if (dx < dy)
    {
        // Get block type
        *type = gameLocate(ix - (incix < 0), floor(fy));

        // Get wall texture coordinate [0;1]
        *tx = fy - floor(fy);
        if (incix < 0)
            *tx = 1 - *tx;

        return dx;
    }
    else
    {
        // Get block type
        *type = gameLocate(floor(fx), iy - (inciy < 0));

        // Get wall texture coordinate [0;1]
        *tx = fx - floor(fx);
        if (inciy > 0)
            *tx = 1 - *tx;

        return dy;
    }
}

static void dispTranspose(unsigned char *buf, unsigned int size)
{
    int i, j;
    unsigned char pixel;

    for (i = 0; i < size; i++) {
        for (j = i + 1; j < size; j++) {
            int ai = i * size + j;
            int bi = j * size + i;

            pixel = buf[ai];
            buf[ai] = buf[bi];
            buf[bi] = pixel;
        }
    }
}

static unsigned char *dispLoadTexture(char *path)
{
    int fd;
    unsigned char *tex;

    tex = malloc(TEX_SIZE * TEX_SIZE);
    fd = giet_fat_open(path, O_RDONLY);
    if (fd < 0) {
        free(tex);
        return NULL;
    }

    giet_fat_read(fd, tex, TEX_SIZE * TEX_SIZE);
    giet_fat_close(fd);

    dispTranspose(tex, TEX_SIZE);

    giet_tty_printf("[RAYCAST] loaded tex %s\n", path);

    return tex;
}

// Exported functions

void dispInit()
{
    unsigned int w, h, p;

    // Initialize lock
    giet_procs_number(&w, &h, &p);
    sqt_lock_init(&slice_x_lock, w, h, p);

    // Allocate framebuffer
    buf[0] = malloc(FBUF_X_SIZE * FBUF_Y_SIZE);
    buf[1] = malloc(FBUF_X_SIZE * FBUF_Y_SIZE);
    sts[0] = malloc(64);
    sts[1] = malloc(64);

    // Initialize framebuffer
    giet_fbf_cma_alloc();
    giet_fbf_cma_init_buf(buf[0], buf[1], sts[0], sts[1]);
    giet_fbf_cma_start(FBUF_X_SIZE * FBUF_Y_SIZE);

    // Load textures
    g_tex[1] = dispLoadTexture("misc/rock_32.raw");
    g_tex[2] = dispLoadTexture("misc/door_32.raw");
    g_tex[3] = dispLoadTexture("misc/handle_32.raw");
    g_tex[4] = dispLoadTexture("misc/wood_32.raw");

    cur_buf = 0;
    slice_cnt = 0;
    slice_x = FBUF_X_SIZE;
}

int dispRenderSlice(Game *game)
{
    unsigned int x;
    int type;
    float angle, dist, tx;

    sqt_lock_acquire(&slice_x_lock);

    if (slice_x == FBUF_X_SIZE) {
        // No more work to do for this frame
        sqt_lock_release(&slice_x_lock);
        return 0;
    }
    else {
        // Keep slice coordinate
        x = slice_x++;
    }

    sqt_lock_release(&slice_x_lock);

    angle = game->player.dir - FIELD_OF_VIEW / 2.f +
            x * FIELD_OF_VIEW / FBUF_X_SIZE;

    // Cast a ray to get wall distance
    dist = dispRaycast(game, &type, &tx, angle);

    // Perspective correction
    dist *= cos(game->player.dir - angle);

    // Draw ceiling, wall and floor
    dispDrawSlice(game, x, FBUF_Y_SIZE / dist, type, tx * TEX_SIZE);

    // Signal this slice is done
    atomic_increment((unsigned int*)&slice_cnt, 1);

    return 1;
}

void dispRender(Game *game)
{
    int start = giet_proctime();

    // Start rendering
    slice_cnt = 0;
    slice_x = 0;

    // Render slices
    while (dispRenderSlice(game));

    // Wait for completion
    while (slice_cnt != FBUF_X_SIZE);

    // Flip framebuffer
    giet_fbf_cma_display(cur_buf);
    cur_buf = !cur_buf;
    giet_tty_printf("[RAYCAST] flip (took %d cycles)\n", giet_proctime() - start);
}

