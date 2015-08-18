#ifndef __GAME_H
#define __GAME_H

#include <stdint.h>
#include <stdbool.h>

#define M_PI            (3.14159f)
#define COLLIDE_GAP     (0.2f)
#define PLAYER_MOVE     (0.13f)
#define PLAYER_ROT      (0.1f)
#define TIME_TOTAL      (30)

typedef struct
{
    float x;
    float y;
    float dir;
} Player;

typedef struct
{
    uint8_t tile[10][10];
    uint8_t w;
    uint8_t h;
    float startX;
    float startY;
    float startDir;
} Map;

typedef struct
{
    Player player;
    Map *map;
    int mapId;
    int timeLeft;
} Game;

int gameLocate(int x, int y);
void gameRun();
void gameTick();
Game *gameInstance();

#endif // __GAME_H
