#include "game.h"

char** initialize_grid(Position* player_pos) {
    char** grid = (char**)malloc(GRID_SIZE * sizeof(char*));
    for (int i = 0; i < GRID_SIZE; i++) {
        grid[i] = (char*)malloc(GRID_SIZE * sizeof(char));
        for (int j = 1; j < GRID_SIZE-1; j++) {
            grid[i][j] = UNFOUND;
        }
    }

    // Set the borders
    for (int i = 0; i < GRID_SIZE; i++) {
        grid[i][0] = WALL;
        grid[i][GRID_SIZE - 1] = WALL;
    }

    for (int j = 0; j < GRID_SIZE; j++) {
        grid[0][j] = WALL;
        grid[GRID_SIZE - 1][j] = WALL;
    }

    // Set the player position
    grid[player_pos->x][player_pos->y] = PLAYER;

    return grid;
}

Position* initialize_player() {
    Position* player_pos = (Position*)malloc(sizeof(Position));
    player_pos->x = 1; 
    player_pos->y = 1;
    return player_pos;
}

void destroy_grid(char** grid) {
    for (int i = 0; i < GRID_SIZE; i++) {
        free(grid[i]);
    }
    free(grid);
}

void destroy_player(Position* player_pos) {
    free(player_pos);
}

void print_grid(char** grid) {
    printf("\033[2J\033[H"); 

    printf("\nq to quit    w up    a left    s down    d right\n\n");
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            printf("%c ", grid[i][j]);
        }
        printf("\n");
    }
}

void move_player(char** grid, Position* player_pos, char direction) {
    int new_x = player_pos->x;
    int new_y = player_pos->y;

    switch (direction) {
        case '0':   //  left
            new_x--;
            break;
        case '1':   //  right 
            new_x++;
            break;
        case '2':   //  up 
            new_y--;
            break;
        case '3':   //  down 
            new_y++;
            break;
        default:
            return; 
    }

    if (grid[new_x][new_y] != WALL) {
        if (grid[new_x][new_y] == EVENT) {
            printf("Found a treasure!\n");
        } else {
            grid[player_pos->x][player_pos->y] = FOUND; 
            grid[new_x][new_y] = PLAYER;
            player_pos->x = new_x;
            player_pos->y = new_y;
        }

    }
}