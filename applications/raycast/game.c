#include "game.h"
#include "disp.h"
#include "ctrl.h"
#include <math.h>

// Globals

static Map map[] =
{
    { // map0
        .tile =
        {
            {1, 0, 0, 0, 0, 1, 0, 0, 1, 1},
            {0, 0, 0, 0, 0, 1, 0, 0, 0, 2},
            {0, 0, 0, 0, 1, 0, 0, 0, 1, 1},
            {0, 0, 0, 1, 3, 0, 3, 0, 0, 0},
            {0, 0, 0, 1, 3, 0, 3, 0, 0, 1},
            {0, 0, 0, 1, 3, 0, 3, 0, 0, 0},
            {0, 0, 0, 1, 1, 0, 3, 0, 0, 1},
            {4, 0, 0, 0, 0, 0, 1, 0, 0, 0},
            {4, 0, 0, 0, 0, 0, 1, 0, 0, 1},
            {0, 4, 4, 4, 4, 0, 0, 0, 1, 0}
        },
        .w = 10,
        .h = 10,
        .startX = 2.f,
        .startY = 3.f,
        .startDir = 70.f * M_PI / 180.f
    },
    { // map1
        .tile =
        {
            {0, 1, 0, 1, 0, 3, 0, 0, 0, 0},
            {0, 1, 0, 0, 0, 3, 0, 0, 0, 0},
            {0, 0, 0, 1, 0, 3, 0, 0, 0, 0},
            {1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
            {4, 2, 4, 1, 3, 0, 3, 3, 3, 0},
            {4, 0, 0, 1, 3, 3, 3, 0, 0, 0},
            {4, 0, 0, 1, 3, 0, 0, 0, 3, 3},
            {4, 0, 0, 0, 0, 0, 4, 0, 0, 3},
            {4, 0, 0, 0, 0, 0, 4, 0, 0, 0},
            {4, 4, 4, 4, 4, 4, 4, 0, 1, 0}
        },
        .w = 10,
        .h = 10,
        .startX = 0.5f,
        .startY = 0.5f,
        .startDir = 90.f * M_PI / 180.f
    },
    { // map2
        .tile =
        {
            {4, 4, 4, 4, 4, 4, 4, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 3, 0},
            {3, 0, 0, 0, 4, 4, 4, 0, 0, 0},
            {3, 0, 0, 4, 0, 0, 0, 1, 1, 0},
            {3, 0, 4, 2, 0, 0, 0, 0, 0, 0},
            {3, 0, 4, 2, 0, 0, 0, 0, 0, 0},
            {3, 0, 0, 4, 0, 0, 0, 1, 1, 0},
            {3, 0, 0, 0, 4, 4, 4, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 3, 0},
            {4, 4, 4, 4, 4, 4, 4, 0, 0, 0}
        },
        .w = 10,
        .h = 10,
        .startX = 4.5f,
        .startY = 5.f,
        .startDir = 0.f * M_PI / 180.f
    },
};

static Game game =
{
    .mapId = 0
};

static bool g_exit;

// Local functions

static void gameOnBlockHit(int type)
{
    g_exit = true;
}

static void gameCollision(float opx, float opy)
{
    static bool collided_x = false;
    static bool collided_y = false;

    float px = game.player.x;
    float py = game.player.y;
    int fpx = floor(px);
    int fpy = floor(py);
    bool colliding_x = false;
    bool colliding_y = false;
    int collide_type_x = 0;
    int collide_type_y = 0;
    int type;

    // Check for x axis collisions
    if      ((type = gameLocate(floor(px + COLLIDE_GAP), fpy))) {
        colliding_x = true, collide_type_x = type;
        game.player.x = fpx - COLLIDE_GAP + 1;
    }
    else if ((type = gameLocate(floor(px - COLLIDE_GAP), fpy))) {
        colliding_x = true, collide_type_x = type;
        game.player.x = fpx + COLLIDE_GAP;
    }

    // Check for y axis collisions
    if      ((type = gameLocate(fpx, floor(py + COLLIDE_GAP)))) {
        colliding_y = true, collide_type_y = type;
        game.player.y = fpy - COLLIDE_GAP + 1;
    }
    else if ((type = gameLocate(fpx, floor(py - COLLIDE_GAP)))) {
        colliding_y = true, collide_type_y = type;
        game.player.y = fpy + COLLIDE_GAP;
    }

    // Check if we're inside a wall
    if ((type = gameLocate(fpx, fpy))) {
        colliding_x = true, collide_type_x = type;
        colliding_y = true, collide_type_y = type;
        game.player.x = opx, game.player.y = opy;
    }

    // Take action when the player hits a block
    if (colliding_x && !collided_x)
        gameOnBlockHit(collide_type_x);
    if (colliding_y && !collided_y)
        gameOnBlockHit(collide_type_y);

    collided_x = colliding_x;
    collided_y = colliding_y;
}

static void gameLogic()
{
    float opx = game.player.x;
    float opy = game.player.y;

    ctrlLogic(&game);
    gameCollision(opx, opy);
}

static void gameInitMap()
{
    game.map = &map[game.mapId];
    game.player.x = game.map->startX;
    game.player.y = game.map->startY;
    game.player.dir = game.map->startDir;
    game.timeLeft = TIME_TOTAL;
}

// Exported functions

int gameLocate(int x, int y)
{
    if ((x < 0 || x >= game.map->w) ||
        (y < 0 || y >= game.map->h)) {
        // Outside the map bounds
        return 1;
    }

    return game.map->tile[y][x];
}

void gameRun()
{
    gameInitMap();

    g_exit = false;

    // Game loop
    while (!g_exit) {
        gameLogic();
        dispRender(&game);
    }

    if (game.timeLeft == 0) {
        // Time's up!
        game.mapId = 0;
    }
    else {
        // Go to next map
        game.mapId++;
    }
}

void gameTick()
{
    game.timeLeft--;

    if (game.timeLeft == 0) {
        g_exit = true;
    }
}

Game *gameInstance()
{
    return &game;
}
