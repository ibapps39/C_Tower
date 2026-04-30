#include "proof.h"



int main(void) {
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    InitWindow(RES_X, RES_Y, "Arena");
    SetTargetFPS(DEFAULT_FPS);
    DisableCursor();

    MATCH_TIME = DEFAULT_MATCH_TIME;
 
    // spawn my player
    PLAYERS[0] = make_player("User", (Vec3){0, FLOOR, 0}, PLAYER);
    PLAYER_COUNT = 1;
    PLAYERS[0].type = PLAYER;
    Player* my_human_player = &PLAYERS[0];

    //bots
    for (size_t i = 1; i < MAX_PLAYERS; i++)
    {
        PLAYERS[i] = make_player("BOT", (Vec3){i*3.00f, FLOOR, 0}, BOT);
        PLAYER_COUNT++;
        PLAYERS[i].type = BOT;
    }
    
    

    while (!WindowShouldClose()) {
        
        float dt = GetFrameTime();
        GAME_STATE.game_time = MATCH_TIME - GetTime();
        if (GAME_STATE.game_time <= 0) GAME_STATE.game_over = 1;
        if (GAME_STATE.game_over)
        {
            game_over();
        }
        
        // input + move local player
        controls(my_human_player, DEFAULT_PLAYER_SPEED, dt);
        cam_handle(my_human_player, my_human_player->hitbox.box.max.y*2);
        for(int i = 1; i<MAX_PLAYERS; i++) bot_tick(&PLAYERS[i], dt); 
        // attack input
 
        // move_bots

        // ipdaupdatete hitboxes, more?
        update_PLAYERS();
        

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(*my_human_player->cam);
                DrawGrid(40, 1.0f);
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (PLAYERS[i].type == UNASSIGNED) continue;
                    Color c = (i == 0) ? GREEN : RED;
                    DrawBoundingBox(PLAYERS[i].hitbox.box, c);
                }
            EndMode3D();
            DrawText(TextFormat("Time: %.1f", GAME_STATE.game_time), 10, 10, 20, WHITE);
        EndDrawing();
    }
 
    cleanup();
    CloseWindow();
    return 0;
}