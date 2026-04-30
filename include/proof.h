#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

//[==MACROS==]
typedef Vector3 Vec3;
#define MAX_PLAYERS         8
#define MAX_TEAMS           4
#define GRAVITY             -9.8f
#define FRICTION            0.85f
#define HIT                 1
#define NO_HIT              0
#define UP_VECTOR           (Vec3){0, 1, 0}
#define TARGET_FPS          144
#define DEFAULT_FPS         60
#define DEFAULT_MATCH_TIME  600
#define DEFAULT_PLAYER_SPEED 1
#define PLAYER_JUMP         (-2.00f * GRAVITY)
#define FLOOR               0.00f
#define CAM_MOUSE_SENSITIVITY  0.003f
#define CAM_PAD_SENSITIVITY    0.05f
#define CAM_ARM_LENGTH_MULT         2.0f  // multiplier of cam_height
//[===========]

//[==STRUCTS==]
typedef struct PlayerCosmetics {
    Texture*        tex;
    ModelAnimation* current_animation;
    Model*          model;
} PlayerCosmo;

typedef struct Hitbox {
    BoundingBox box;
    uint8_t     hit;
} Hitbox;

typedef enum PlayerType    { UNASSIGNED, PLAYER, BOT }           PlayerType;
typedef enum StatusEffects { NONE, SLOWED, CONFUSED, STUNNED }   StatusEffects;

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
    uint8_t         pid;
    uint8_t         unlocked_stages; //0-3
} Player;

typedef struct Team {
    uint8_t       player_indices[MAX_PLAYERS];
    uint8_t       n_mates;
    unsigned int  team_score;
    Color         color;
} Team;

typedef struct Attack {
    uint8_t       point_damage;
    uint8_t       knockback;
    uint8_t       duration;
    StatusEffects effect;
} Attack;
typedef struct Stage
{
    Vec3 center;
    float radius;
    float score_req;
    uint8_t stage_num; //0-3
    uint8_t unlocked;
} Stage;
//[===========]

//[==GLOBALS==]
typedef struct G_State {
    int      top_score;
    char*    top_player;
    Color    winning_team;
    double   game_time;
    uint8_t  gates_open;
    uint8_t  game_over;
} G_State;

G_State GAME_STATE = {
    .game_time    = 0.00,
    .gates_open   = 0,
    .top_player   = "NONE",
    .top_score    = 0,
    .winning_team = {0, 0, 0, 0},
    .game_over    = 0
};
Stage STAGES[4];

Player PLAYERS[MAX_PLAYERS];
int    PLAYER_COUNT = 0;
int    RES_X        = 700;
int    RES_Y        = 700;
double MATCH_TIME   = DEFAULT_MATCH_TIME;
//[===========]

//[==HELPERS==]
Hitbox create_hitbox(Vec3 pos, float hw, float hh, float hd) {
    return (Hitbox){
        (BoundingBox){
            { pos.x - hw, pos.y - hh, pos.z - hd },
            { pos.x + hw, pos.y + hh, pos.z + hd }
        },
        NO_HIT
    };
}

// moves box so its center is at p, preserving half-extents
int update_aabb(BoundingBox* b, Vec3 p) {
    float hwx = (b->max.x - b->min.x) * 0.5f;
    float hhy = (b->max.y - b->min.y) * 0.5f;
    float hdz = (b->max.z - b->min.z) * 0.5f;
    b->min = (Vec3){ p.x - hwx, p.y - hhy, p.z - hdz };
    b->max = (Vec3){ p.x + hwx, p.y + hhy, p.z + hdz };
    return 1;
}

int update_hitbox_pos(uint8_t pidx, Vec3 p) {
    update_aabb(&PLAYERS[pidx].hitbox.box, p);
    return 1;
}
//[===========]

//[==FUNCTIONS==]
void apply_effect(Player* p, StatusEffects e) {
    p->active_effect = e;
}
bool stage_barrier_collision(Stage s, Hitbox* h, Vec3 pos)
{
    if (CheckCollisionBoxSphere(h->box, pos, s.radius)) return 1;
    return 0;
}
uint8_t attack(Attack atk, Player* source, Player* target) {
    if (!source || !target || !target->active) return NO_HIT;
    if (!CheckCollisionBoxes(source->hitbox.box, target->hitbox.box)) return NO_HIT;
    target->hitbox.hit = HIT;
    if (atk.effect != NONE) apply_effect(target, atk.effect);
    return HIT;
}

int move(Vec3* p, Vec3* v, Vec3* a, float dt) {
    dt = dt*10;
    if (a->x == 0 && a->y == 0 && a->z == 0) return 0;
    v->x += a->x * dt;
    v->y += (a->y + GRAVITY) * dt;
    v->z += a->z * dt;
    v->x *= FRICTION;
    v->z *= FRICTION;
    p->x += v->x * dt;
    p->y += v->y * dt;
    p->z += v->z * dt;
    if (p->y < 0.0f) { p->y = 0.0f; v->y = 0.0f; }
    return 1;
}

int cam_handle(Player* p, float cam_height) {
    if (!p) return 0;

    Vec3 origin = { p->pos.x, p->pos.y + cam_height, p->pos.z };
    
    float rx    = IsGamepadAvailable(0) ? GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * CAM_PAD_SENSITIVITY : 0;
    Vector2 md  = GetMouseDelta();
    float yaw   = md.x * CAM_MOUSE_SENSITIVITY + rx;
    float pitch = md.y * CAM_MOUSE_SENSITIVITY;

    Vec3 offset = Vector3Subtract(p->cam->position, origin);
    // lock arm length so cam never drifts away
    offset = Vector3Scale(Vector3Normalize(offset), cam_height * CAM_ARM_LENGTH_MULT);
    offset = Vector3RotateByAxisAngle(offset, UP_VECTOR, -yaw);
    offset = Vector3RotateByAxisAngle(offset, Vector3Normalize(Vector3CrossProduct(offset, UP_VECTOR)), -pitch);

    p->cam->position = Vector3Add(origin, offset);
    p->cam->target   = (Vec3){ p->pos.x, p->pos.y + 1.0f, p->pos.z };
    return 1;
}

int controls(Player* p, float default_speed, float dt) {
    if (!p || !p->active)                   return 0;
    if (p->active_effect == STUNNED)         return 0;
    float speed = (p->active_effect == SLOWED) ? default_speed * 0.5f : default_speed;

    float cam_height = (p->hitbox.box.max.y - p->hitbox.box.min.y); // full height
    Vec3 fwd   = Vector3Normalize((Vec3){
        p->cam->target.x - p->cam->position.x, 0,
        p->cam->target.z - p->cam->position.z });
    Vec3 right = Vector3CrossProduct(fwd, UP_VECTOR);
    Vec3 a     = {0};

    if (IsKeyDown(KEY_W)) { a.x += fwd.x   * speed; a.z += fwd.z   * speed; }
    if (IsKeyDown(KEY_S)) { a.x -= fwd.x   * speed; a.z -= fwd.z   * speed; }
    if (IsKeyDown(KEY_A)) { a.x -= right.x * speed; a.z -= right.z * speed; }
    if (IsKeyDown(KEY_D)) { a.x += right.x * speed; a.z += right.z * speed; }
    if (IsKeyDown(KEY_SPACE) && p->pos.y <= 0.01f) p->vel.y = PLAYER_JUMP;

    if (IsGamepadAvailable(0)) {
        float lx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float ly = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        a.x += (fwd.x * -ly + right.x * lx) * speed;
        a.z += (fwd.z * -ly + right.z * lx) * speed;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) && p->pos.y <= 0.01f)
            p->vel.y = PLAYER_JUMP;
    }
    p->accel = a;
    move(&p->pos, &p->vel, &p->accel, dt);
    return 1;
}
// make_player: value, no pointer deref
Player make_player(char* name, Vec3 spawn, PlayerType type) {
    Player p        = {0};
    p.name          = name;
    p.pos           = spawn;
    p.vel           = (Vec3){0};
    p.accel         = (Vec3){0};
    p.hitbox        = create_hitbox(spawn, 0.5f, 1.0f, 0.5f);
    p.type          = type;
    p.active_effect = NONE;
    p.score         = 0;
    p.active        = (type != UNASSIGNED) ? 1 : 0;
    p.cosmetic      = NULL; // TODO Change
    p.unlocked_stages = 0;

    Camera3D* cam   = (Camera3D*)malloc(sizeof(Camera3D));
    float hh        = p.hitbox.box.max.y - p.hitbox.box.min.y; // full height
    cam->position   = (Vec3){ spawn.x, spawn.y + hh, spawn.z + hh };
    cam->target     = (Vec3){ spawn.x, spawn.y + 1.0f, spawn.z };
    cam->up         = UP_VECTOR;
    cam->fovy       = 120.0f;
    cam->projection = CAMERA_PERSPECTIVE;
    p.cam           = cam;

    return p;
}

void make_stages(Player p)
{
    for (size_t i = 1; i <= 4; i++)
    {
        int m = 4%i;
        Stage s = {0};
        s.radius = m == 0 ? p.hitbox.box.max.x * 3 : p.hitbox.box.max.x * 3 * m;
        s.score_req = 1000 * m + 1000;
        s.stage_num = i-1;
        s.unlocked = i-1 == 0 ? 1 : 0;
        s.center = (Vec3){0,0,0};
        STAGES[i-1] = s;
    }
}
uint8_t load_stages()
{

}

uint8_t add_player(Player p, PlayerType type) {
    p.type = type;
    p.active = (type != UNASSIGNED) ? 1 : 0;
    if (PLAYER_COUNT < MAX_PLAYERS) {
        p.pid = PLAYER_COUNT;
        PLAYERS[PLAYER_COUNT++] = p;
        return 1;
    }
    // find empty slot
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (PLAYERS[i].type == UNASSIGNED) {
            p.pid      = i;
            PLAYERS[i] = p;
            return 1;
        }
    }
    printf("No free player slots\n");
    return 0;
}

uint8_t remove_player(Player* p) {
    if (!p) return 0;
    free(p->cam);
    PLAYERS[p->pid] = make_player("NULL", (Vec3){0}, UNASSIGNED);
    return 1;
}
uint8_t update_PLAYERS(void) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (PLAYERS[i].type == UNASSIGNED) continue;
        update_hitbox_pos(i, PLAYERS[i].pos);
    }
    return 1;
}
void draw_PLAYERS()
{
    unsigned int tmp_width = abs(PLAYERS[0].hitbox.box.max.x - PLAYERS[0].hitbox.box.min.x);
    unsigned int tmp_height = abs(PLAYERS[0].hitbox.box.max.y - PLAYERS[0].hitbox.box.min.y);
    unsigned int tmp_length = abs(PLAYERS[0].hitbox.box.max.z - PLAYERS[0].hitbox.box.min.z);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        //if (PLAYERS[i].type == UNASSIGNED) continue;
        DrawCube(PLAYERS[i].pos, tmp_width, tmp_width, tmp_length, i == 0 ? BLUE : RED);
    }
}
// for start of match
BoundingBox get_placeable_area(Stage s)
{
    BoundingBox dim;
    dim.min = (Vec3){s.center.x - s.radius, s.center.y, s.center.z - s.radius};
    dim.max = (Vec3){s.center.x + s.radius, s.center.y, s.center.z + s.radius};
    return dim;
}
Vec3 get_placeable_pt(BoundingBox outer, BoundingBox inner, float y_level)
{
    Vec3 test_pt;
    goto get_point;

    get_point:
    test_pt.x = GetRandomValue((int)outer.min.x + 1, (int)outer.max.x - 1);
    test_pt.z = GetRandomValue((int)outer.min.z + 1, (int)outer.max.z - 1);
    test_pt.y = y_level;
    goto loop;

    loop:
    {
        bool is_valid_x = (test_pt.x < inner.min.x - 1 && test_pt.x > outer.min.x + 1)
                       || (test_pt.x > inner.max.x + 1 && test_pt.x < outer.max.x - 1);

        bool is_valid_z = (test_pt.z < inner.min.z - 1 && test_pt.z > outer.min.z + 1)
                       || (test_pt.z > inner.max.z + 1 && test_pt.z < outer.max.z - 1);

        bool is_valid = is_valid_x && is_valid_z;
        if (!is_valid) { goto get_point; }
    }
    return test_pt;
}
uint8_t place_players()
{
    BoundingBox outer = get_placeable_area(STAGES[0]);
    BoundingBox inner = get_placeable_area(STAGES[1]);

    for (int i = 0; i < PLAYER_COUNT; i++)
    {
        PLAYERS[i].pos = get_placeable_pt(outer, inner, FLOOR);
    }
    return 1;
}
uint8_t bot_tick(Player* p, float dt) {
    
    if (!p || !p->active || p->active_effect == STUNNED || p->type == UNASSIGNED || p->type == PLAYER) return 0;
    
    Player* target      = NULL;
    float   closest     = 1e9f;
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (&PLAYERS[i] == p || !PLAYERS[i].active) continue;
        float d = Vector3Distance(p->pos, PLAYERS[i].pos);
        if (d < closest) { closest = d; target = &PLAYERS[i]; }
    }
    if (!target) return 0;
    // move towards it
    Vec3 dir = Vector3Normalize(Vector3Subtract(target->pos, p->pos));
    p->accel = (Vec3){ dir.x * DEFAULT_PLAYER_SPEED, 0, dir.z * DEFAULT_PLAYER_SPEED };
    move(&p->pos, &p->vel, &p->accel, dt);
    if (CheckCollisionBoxes(p->hitbox.box, target->hitbox.box)) {
        Attack a = { 10, 3, 1, NONE };
        attack(a, p, target);
    }
    return 1;
}

void game_over()
{
    DrawText("Game Over", RES_X/2, RES_Y/2, 20, WHITE);
}
void cleanup(void) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (PLAYERS[i].type != UNASSIGNED && PLAYERS[i].cam)
            free(PLAYERS[i].cam);
}
//[==============]