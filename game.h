#ifndef GAME_H
#define GAME_H

#include "stdlib.h"
#include "stdio.h"
#include <termios.h>
#include <unistd.h>

#define GRID_SIZE 10    // The size of the grid is 10x10 because of the borders
#define PLAYER 'O'
#define WALL '#'
#define FOUND ' '
#define UNFOUND '.'

typedef struct {
    int x;
    int y;
} Position;

Position* initialize_player();
char** initialize_grid(Position* player_pos);
void destroy_grid(char** grid);
void destroy_player(Position* player_pos);
void print_grid(char** grid);
void move_player(char** grid, Position* player_pos, char direction);


#endif