#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"


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
    Vector2 size;
    Vector2 half_size;
    Vector2 pos;
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


int main(void) {
    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 720;
    const Vector2 WINDOW_CENTRE = (Vector2){
        WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f
    };
    const int CELL_WIDTH = 16;
    const int CELL_HEIGHT = 16;
    const int FPS = 0;

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "platformer");
    HideCursor();
    SetTargetFPS(FPS);

    Player player = {0};
    player.keybinds[RIGHT] = 262;
    player.keybinds[LEFT] = 263;
    player.keybinds[DOWN] = 264;
    player.keybinds[UP] = 265;
    player.size = (Vector2){CELL_WIDTH, CELL_HEIGHT};
    player.half_size = Vector2Scale(player.size, 0.5f);
    player.pos = WINDOW_CENTRE;
    player.vel = Vector2Zero();
    player.max_vel = (Vector2){400.0f, 1000.0f};
    player.min_vel = Vector2Negate(player.max_vel);
    player.speed = 600.0f;
    player.friction = 0.001f;  // Between 0 - 1, higher means lower friction
    float min_jump = CELL_HEIGHT;
    float max_jump = CELL_HEIGHT * 8.0f;
    float time_to_jump_apex = 0.5f;
    player.gravity = (2.0f * max_jump) / powf(time_to_jump_apex, 2.0f);
    player.min_jump_speed = -powf(2.0f * fabs(player.gravity) * min_jump, 0.5f);
    player.max_jump_speed = -fabs(player.gravity) * time_to_jump_apex;
    player.fall_multiplier = 1.8f;
    player.coyote_time = 0.1f;
    player.coyote_time_left = 0.0f;
    player.jump_buffer = 0.15;
    player.jump_buffer_left = 0.0f;
    player.is_grounded = false;

    Camera2D camera = {0};
    camera.target = Vector2Zero();
    camera.offset = Vector2Zero();
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    char keycode[50] = "KEYCODE: 0";

    while (!WindowShouldClose()) {
        // UPDATE -------------------------------------------------------------
        float dt = GetFrameTime();

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

        // Player Movement
        // Accurate deltatime (Jonas Tyroller)
        // https://www.youtube.com/watch?v=yGhfUcPjXuE
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
        if (move_dir.y < 0 && input_dir.y != -1 && player.vel.y < player.min_jump_speed) {
            player.vel.y = player.min_jump_speed;
        }

        float fall_speed = player.gravity;
        if (move_dir.y > 0) fall_speed *= player.fall_multiplier;

        Vector2 half_acc = (Vector2) {
            input_dir.x * player.speed * dt * 0.5f,
            fall_speed * dt * 0.5f
        };

        player.vel = Vector2Add(player.vel, half_acc);
        player.vel = Vector2Clamp(player.vel, player.min_vel, player.max_vel);

        player.pos = Vector2Add(player.pos, Vector2Scale(player.vel, dt));

        player.vel = Vector2Add(player.vel, half_acc);
        player.vel = Vector2Clamp(player.vel, player.min_vel, player.max_vel);

        // Check for collision
        if (player.pos.x + player.half_size.x > WINDOW_WIDTH) {
            player.pos.x = WINDOW_WIDTH - player.half_size.x;
            player.vel.x = 0;
        } else if (player.pos.x - player.half_size.x < 0) {
            player.pos.x = player.half_size.x;
            player.vel.x = 0;
        }

        if (player.pos.y + player.half_size.y > WINDOW_HEIGHT) {
            player.pos.y = WINDOW_HEIGHT - player.half_size.y;
            player.vel.y = 0;
            player.is_grounded = true;
        } else if (player.pos.y - player.half_size.y < 0) {
            player.pos.y = player.half_size.y;
            player.vel.y = 0;
        }

        // RENDER -------------------------------------------------------------
        BeginDrawing();

        ClearBackground(BLACK);

        BeginMode2D(camera);
        Rectangle player_rect = {
            player.pos.x - player.half_size.x,
            player.pos.y - player.half_size.y,
            player.size.x,
            player.size.y
        };

        DrawRectangleLinesEx(player_rect, 2.0f, BLUE);
        DrawCircleV(player.pos, 2.0f, MAGENTA);
        EndMode2D();

        DrawFPS(0, 0);
        DrawText(keycode, 0, 20, 20, MAGENTA);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
