#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
//[==MACROS, Macro functions, Definitions, Constants==]
typedef Vector3 Vec3;
#define MAX_PLAYERS       8
#define MAX_TEAMS         4
#define GRAVITY           -9.8f
#define FRICTION          0.85f
#define HIT 1
#define NO_HIT 0
#define UP_VECTOR (Vec3){0, 1, 0}
//[---------------------------------------------------]
// [==DATA STRUCTS (structs, enums, etc)==============]


typedef struct PlayerCosmetics
{
Texture* tex;
ModelAnimation* current_animation;
Model* model;
} PlayerCosmo;

typedef struct Hitbox {
    BoundingBox box;
    u_int8_t hit;
} Hitbox;

typedef enum PlayerType { PLAYER, BOT } PlayerType;

typedef enum StatusEffects { NONE, SLOWED, CONFUSED, STUNNED } StatusEffects;

// Source of Truth
typedef struct Player {
    unsigned char   active;
    char*           name;
    Vec3            pos;
    Vec3            vel;
    Vec3            accel;
    Hitbox          hitbox;
    Camera3D*       cam;
    PlayerCosmo*    cosmetic;
    PlayerType      type;
    StatusEffects   active_effect;
    unsigned int    score;
    unsigned int    players_index;
} Player;

typedef struct Team {
    unsigned int        player_indices[MAX_PLAYERS];
    unsigned int        team_score;
    unsigned int        n_mates;
    Color               color;
} Team;

typedef struct Attack {
    int           point_damage;
    int           knockback;
    int           duration; 
    StatusEffects effect;
} Attack;
//[---------------------------------------------------]
//[==GLOBALS==========================================]

// Game global, shared state - relevant to all players
typedef struct G_State
{
    int     top_score;
    char*   top_player;
    Color   winning_team;
    double  game_time;
    int     gates_open;
} G_State;
G_State GAME_STATE;

Player Players[MAX_PLAYERS]; // Array of players (AoS)
int TOTAL_PLAYERS = 0;

//[---------------------------------------------------]
// RULE! A FUNCTION IS A FUNCTION, -- PREFER -- PURE FUNCTION OVER NOT
//[==HELPER FUNCTIONS=================================] // could be inlined
Hitbox create_hitbox(Vec3 pos, float hw, float hh, float hd) 
{
    return (Hitbox){ 
        (BoundingBox)
        {
            pos.x-hw, pos.y-hh, pos.z-hd, 
            pos.x+hw, pos.y+hh, pos.z+hd
        }, 
        NO_HIT
    };
};
int update_aabb(BoundingBox* b, Vec3 p)
{
    b->min.x = p.x;
    b->min.y = p.y;
    b->min.z = p.z;
    b->max.x = p.x;
    b->max.y = p.y;
    b->max.z = p.z;
    return 1;
}
int update_hitbox(u_int8_t pidx, u_int8_t hit, Vec3 p)
{
    update_aabb(&Players[pidx].hitbox.box, p);
    Players[pidx].hitbox.hit = hit;
    return 1;
}

//[---------------------------------------------------]
//[==FUNCTIONS========================================]
void apply_effect(Player* p, StatusEffects e) {
    p->active_effect = e;
}
u_int8_t attack(Attack atk, Player* source, Player* target) {
    if (!source || !target || !(target->active)) return NO_HIT;
    float dist = Vector3Distance(source->pos, target->pos);
    if (dist > Vector3Length(target->hitbox.box.max) ) return NO_HIT;
    // Vec3 dir = Vector3Normalize(Vector3Subtract(target->pos, source->pos));
    if (atk.effect != NONE) apply_effect(target, atk.effect);
    return HIT;
}
#define PLAYER_JUMP -2.00f*GRAVITY
int controls(Player* p, float default_speed, float dt)
{
    float speed = default_speed;
    if (!p->active)                     return 0;
    if (p->active_effect == STUNNED)    return 0;
    if (p->active_effect == SLOWED)     speed *= 0.5f;

    float px = p->cam->position.x;
    float py = p->cam->position.y;
    float pz = p->cam->position.z;
    float tx = p->cam->target.x;
    float ty = p->cam->target.y;
    float tz = p->cam->target.z;

    Vec3 fwd  = Vector3Normalize((Vec3){ tx-px, 0, tz-pz });
    Vec3 right = Vector3CrossProduct(fwd, UP_VECTOR);
    Vec3 a = {0};

    // keyboard
    if (IsKeyDown(KEY_W)) { a.x += fwd.x * speed;  a.z += fwd.z * speed; }
    if (IsKeyDown(KEY_S)) { a.x -= fwd.x * speed;  a.z -= fwd.z * speed; }
    if (IsKeyDown(KEY_A)) { a.x -= right.x * speed; a.z -= right.z * speed; }
    if (IsKeyDown(KEY_D)) { a.x += right.x * speed; a.z += right.z * speed; }
    if (IsKeyDown(KEY_SPACE) && p->pos.y <= 0.01f) p->vel.y = PLAYER_JUMP;

    // gamepad (pad 0)
    if (IsGamepadAvailable(0)) {
        float lx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float ly = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        a.x += (fwd.x * -ly + right.x * lx) * speed;
        a.z += (fwd.z * -ly + right.z * lx) * speed;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) && p->pos.y <= 0.01f)
            p->vel.y = PLAYER_JUMP;
    }
    p->accel = a;
    return 1;
}
int cam_handle(Player* p, const float CAM_HEIGHT)
{
    
     // mouse look - orbit cam around player
    Vector2 md = GetMouseDelta();
    float yaw   = md.x * 0.003f;
    float pitch = md.y * 0.003f;
    Vec3 offset = Vector3Subtract(p->cam->position, (Vec3){p->pos.x, p->pos.y + CAM_HEIGHT, p->pos.z});
    offset = Vector3RotateByAxisAngle(offset, (Vec3){0,1,0}, -yaw);
    // clamp pitch
    Vec3 right = Vector3CrossProduct(offset, UP_VECTOR);
    offset = Vector3RotateByAxisAngle(offset, Vector3Normalize(right), -pitch);
    p->cam->position = Vector3Add((Vec3){p->pos.x, p->pos.y + CAM_HEIGHT, p->pos.z}, offset);
    p->cam->target   = (Vec3){ p->pos.x, p->pos.y + 1.0f, p->pos.z };
 
    // gamepad right stick look
    if (IsGamepadAvailable(0)) {
        float rx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
        Vec3 off2 = Vector3Subtract(p->cam->position, (Vec3){p->pos.x, p->pos.y + CAM_HEIGHT, p->pos.z});
        off2 = Vector3RotateByAxisAngle(off2, (Vec3){0,1,0}, -rx * 0.05f);
        p->cam->position = Vector3Add((Vec3){p->pos.x, p->pos.y + CAM_HEIGHT, p->pos.z}, off2);
    }
}
int move(Vec3* p, Vec3* v, Vec3* a, float dt) {
    v->x += a->x * dt;
    v->y += (a->y + GRAVITY) * dt;
    v->z += a->z * dt;
    v->x *= FRICTION;
    v->z *= FRICTION;
    p->x += v->x * dt;
    p->y += v->y * dt;
    p->z += v->z * dt;
    if (p->y < 0.0f) { p->y = 0.0f; v->y = 0.0f; } // floor clamp
    return 1;
}
//[---------------------------------------------------]
