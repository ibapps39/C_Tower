int main(void) {
    InitWindow(1280, 720, "Arena");
    SetTargetFPS(60);
    DisableCursor();
 
    // spawn players
    Vec3 spawns[4] = { {-5,0,-5}, {5,0,-5}, {-5,0,5}, {5,0,5} };
    PLAYER_COUNT = 4;
    Players[0] = make_player("P1",  spawns[0], PLAYER);
    Players[1] = make_player("BOT1",spawns[1], BOT);
    Players[2] = make_player("BOT2",spawns[2], BOT);
    Players[3] = make_player("BOT3",spawns[3], BOT);
 
    make_teams(2);
 
    GAME_STATE.game_time = 120.0;
    GAME_STATE.running   = 1;
 
    PlayerState local = {
        .game               = &GAME_STATE,
        .player_points      = 0,
        .players_team       = Teams[0],
        .player_controlled  = Players[0],
    };
 
    while (!WindowShouldClose() && GAME_STATE.running) {
        float dt = GetFrameTime();
        GAME_STATE.game_time -= dt;
        if (GAME_STATE.game_time <= 0) GAME_STATE.running = 0;
 
        // input + move local player
        handle_input(&local, dt);
        Player* lp = local.player_controlled;
        move(&lp->pos, &lp->vel, &lp->accel, dt);
        update_hitbox(lp);
        follow_cam(lp);
 
        // attack input
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) {
            Attack swing = { 15, 5, 0.2, NONE };
            for (int i = 1; i < PLAYER_COUNT; i++)
                attack(swing, lp, Players[i]);
        }
 
        // bots
        for (int i = 1; i < PLAYER_COUNT; i++) {
            bot_tick(Players[i], dt);
            move(&Players[i]->pos, &Players[i]->vel, &Players[i]->accel, dt);
            update_hitbox(Players[i]);
        }
 
        update_game_state();
 
        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(*lp->cam);
                DrawGrid(40, 1.0f);
                draw_players();
            EndMode3D();
            draw_hud(&local);
        EndDrawing();
    }
 
    cleanup();
    CloseWindow();
    return 0;
}