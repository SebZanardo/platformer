#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"


#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define WINDOW_CAPTION "platformer"
#define FPS 0
#define MIN_FPS 60

#define CELL_WIDTH 16
#define CELL_HEIGHT 16
#define LEVEL_WIDTH WINDOW_WIDTH / CELL_WIDTH
#define LEVEL_HEIGHT WINDOW_HEIGHT / CELL_HEIGHT
#define SMOL 0.01f

#define MAX_KEYBINDS 4


// Ensure enum length matches MAX_KEYBINDS
typedef enum {
    RIGHT,
    LEFT,
    DOWN,
    UP,
} Action;


typedef struct {
    int keybinds[MAX_KEYBINDS];
    Rectangle aabb;
    Vector2 vel;
    Vector2 max_vel;
    Vector2 min_vel;
    float speed;
    float friction;
    float gravity;
    float min_jump_speed;
    float max_jump_speed;
    float fall_multiplier;
    float coyote_time;
    float coyote_time_left;
    float jump_buffer;
    float jump_buffer_left;
    bool is_grounded;
} Player;


typedef enum {
    EMPTY,
    SOLID,
} CellType;


bool inside_level(int x, int y);


int main(void) {
    // Load level from file
    FILE* file_ptr = fopen("src/level.txt", "r");

    if (file_ptr == NULL) {
        printf("ERROR: Could not open file!\n");
        return 1;
    }

    int level[LEVEL_HEIGHT][LEVEL_WIDTH] = {0};

    // TODO: Make this better. Load file into array and resize array???
    int x = 0;
    int y = 0;
    int cell_type;
    while (fscanf(file_ptr, "%d", &cell_type) == 1 && y < LEVEL_HEIGHT) {
        level[y][x] = cell_type;
        x++;
        if (fgetc(file_ptr) == '\n' || x >= LEVEL_WIDTH) {
            x = 0;
            y++;
        }
    }

    fclose(file_ptr);

    const Vector2 WINDOW_CENTRE = (Vector2){
        WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f
    };

    // max_dt to stop lag spikes that cause inaccurate movement and collision
    const float max_dt = 1.0f / MIN_FPS;

    // Initialise game
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_CAPTION);
    HideCursor();
    SetTargetFPS(FPS);

    Player player = {0};
    player.keybinds[RIGHT] = 262;
    player.keybinds[LEFT] = 263;
    player.keybinds[DOWN] = 264;
    player.keybinds[UP] = 265;
    player.aabb = (Rectangle){
        WINDOW_CENTRE.x, WINDOW_CENTRE.y,
        CELL_WIDTH - 1.0f, CELL_HEIGHT - 1.0f
    };
    player.vel = Vector2Zero();
    player.max_vel = (Vector2){200.0f, 400.0f};
    player.min_vel = Vector2Negate(player.max_vel);
    player.speed = 600.0f;
    player.friction = 0.0001f;  // Between 0 - 1, higher means lower friction
    float min_jump = CELL_HEIGHT * 1.5f;
    float max_jump = CELL_HEIGHT * 4.5f;
    float time_to_jump_apex = 0.4f;
    player.gravity = (2.0f * max_jump) / powf(time_to_jump_apex, 2.0f);
    player.min_jump_speed = -powf(2.0f * fabs(player.gravity) * min_jump, 0.5f);
    player.max_jump_speed = -fabs(player.gravity) * time_to_jump_apex;
    player.fall_multiplier = 1.9f;
    player.coyote_time = 0.1f;
    player.coyote_time_left = 0.0f;
    player.jump_buffer = 0.15f;
    player.jump_buffer_left = 0.0f;
    player.is_grounded = false;

    Camera2D camera = {0};
    camera.target = Vector2Zero();
    camera.offset = Vector2Zero();
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    char keycode[50] = "KEYCODE: 0";

    float top_left_x;
    float bottom_right_x;
    float top_left_y;
    float bottom_right_y;

    while (!WindowShouldClose()) {
        // UPDATE -------------------------------------------------------------
        float dt = GetFrameTime();
        if (dt > max_dt) dt = max_dt;

        int keycode_pressed = GetKeyPressed();
        if (keycode_pressed) sprintf(keycode, "KEYCODE: %d", keycode_pressed);

        bool jump_pressed = IsKeyPressed(player.keybinds[UP]);
        Vector2 input_dir = (Vector2){0.0f, 0.0f};
        if (IsKeyDown(player.keybinds[LEFT])) input_dir.x -= 1;
        if (IsKeyDown(player.keybinds[RIGHT])) input_dir.x += 1;
        if (IsKeyDown(player.keybinds[UP])) input_dir.y -= 1;
        if (IsKeyDown(player.keybinds[DOWN])) input_dir.y += 1;

        Vector2 move_dir = (Vector2){0.0f, 0.0f};
        if (player.vel.x > 0) move_dir.x = 1;
        else if (player.vel.x < 0) move_dir.x = -1;
        if (player.vel.y > 0) move_dir.y = 1;
        else if (player.vel.y < 0) move_dir.y = -1;

        // Set Buffers
        if (player.is_grounded) player.coyote_time_left = player.coyote_time;
        else if (player.coyote_time_left > 0) player.coyote_time_left -= dt;

        if (jump_pressed) player.jump_buffer_left = player.jump_buffer;
        else if (player.jump_buffer_left > 0) player.jump_buffer_left -= dt;

        // Player movement
        if (input_dir.x != move_dir.x) {
            player.vel.x = Lerp(0.0f, player.vel.x, powf(player.friction, dt));
            if (fabs(player.vel.x) < 1) player.vel.x = 0;
        }

        if (
            player.jump_buffer_left > 0 && player.is_grounded ||
            jump_pressed && player.coyote_time_left > 0
        ) {
            player.vel.y = player.max_jump_speed;
            player.is_grounded = false;
            player.coyote_time_left = 0;
            player.jump_buffer_left = 0;
        }

        if (
            move_dir.y < 0 &&
            input_dir.y != -1 &&
            player.vel.y < player.min_jump_speed
        ) {
            player.vel.y = player.min_jump_speed;
        }

        float fall_speed = player.gravity;
        if (move_dir.y > 0) {
            fall_speed *= player.fall_multiplier;
        }

        // Accurate deltatime (Jonas Tyroller)
        // https://www.youtube.com/watch?v=yGhfUcPjXuE
        Vector2 half_acc = (Vector2) {
            input_dir.x * player.speed * dt * 0.5f,
            fall_speed * dt * 0.5f
        };

        player.vel = Vector2Add(player.vel, half_acc);
        player.vel = Vector2Clamp(player.vel, player.min_vel, player.max_vel);

        // Check for collision, handle each axis separately
        player.aabb.x += player.vel.x * dt;

        // Calculate cells in level that player is in and check for collision
        top_left_x = player.aabb.x / CELL_WIDTH;
        bottom_right_x = (player.aabb.x + player.aabb.width) / CELL_WIDTH;
        top_left_y = player.aabb.y / CELL_HEIGHT;
        bottom_right_y = (player.aabb.y + player.aabb.height) / CELL_HEIGHT;
        for (int y = top_left_y; y <= bottom_right_y; y++) {
            for (int x = top_left_x; x <= bottom_right_x; x++) {
                if (!inside_level(x, y)) continue;

                int cell_type = level[y][x];
                if (cell_type == EMPTY) continue;

                if (player.vel.x > 0) {
                    player.aabb.x = x * CELL_WIDTH - player.aabb.width - SMOL;
                }
                else if (player.vel.x < 0) {
                    player.aabb.x = (x + 1) * CELL_WIDTH + SMOL;
                }
                player.vel.x = 0;
            }
        }

        player.aabb.y += player.vel.y * dt;
        player.is_grounded = false;

        // Calculate cells in level that player is in and check for collision
        top_left_x = player.aabb.x / CELL_WIDTH;
        bottom_right_x = (player.aabb.x + player.aabb.width) / CELL_WIDTH;
        top_left_y = player.aabb.y / CELL_HEIGHT;
        bottom_right_y = (player.aabb.y + player.aabb.height) / CELL_HEIGHT;
        for (int y = top_left_y; y <= bottom_right_y; y++) {
            for (int x = top_left_x; x <= bottom_right_x; x++) {
                if (!inside_level(x, y)) continue;

                int cell_type = level[y][x];
                if (cell_type == EMPTY) continue;

                if (player.vel.y > 0) {
                    player.aabb.y = y * CELL_HEIGHT - player.aabb.height - SMOL;
                    player.is_grounded = true;
                }
                else if (player.vel.y < 0) {
                    player.aabb.y = (y + 1) * CELL_HEIGHT + SMOL;
                }
                player.vel.y = 0;
            }
        }

        player.vel = Vector2Add(player.vel, half_acc);
        player.vel = Vector2Clamp(player.vel, player.min_vel, player.max_vel);

        // RENDER -------------------------------------------------------------
        BeginDrawing();

        ClearBackground(BLACK);

        BeginMode2D(camera);
        // Draw level
        for (int y = 0; y < LEVEL_HEIGHT; y++) {
            for (int x = 0; x < LEVEL_WIDTH; x++) {
                int cell_type = level[y][x];
                if (cell_type == EMPTY) continue;
                Rectangle cell_rect = {
                    x * CELL_WIDTH,
                    y * CELL_HEIGHT,
                    CELL_WIDTH,
                    CELL_HEIGHT
                };
                DrawRectangleLinesEx(cell_rect, 2.0f, BLUE);
            }
        }

        // Draw player
        DrawRectangleRec(player.aabb, MAGENTA);

        EndMode2D();

        DrawFPS(0, 0);
        DrawText(keycode, 0, 20, 20, MAGENTA);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}


bool inside_level(int x, int y) {
    return (x >= 0 && x < LEVEL_WIDTH && y >= 0 && y < LEVEL_HEIGHT);
}
