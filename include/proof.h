#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

//[==MACROS==]
typedef Vector4 Vec4;
typedef Vector3 Vec3;
typedef Vector2 Vec2;
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
#define DEFAULT_PLAYER_SPEED 10000
#define PLAYER_JUMP         (-2.00f * GRAVITY)
#define FLOOR               0.00f
#define DEFAULT_GAME_PAD 0
#define CAM_MOUSE_SENSITIVITY  0.003f
#define CAM_PAD_SENSITIVITY    0.05f
#define CAM_ARM_LENGTH_MULT         2.0f  // multiplier of cam_height
#define DEFAULT_NUMBER_OF_STAGES    4
#define DEFAULT_PLAYER_WIDTH        5.0f
#define DEFAULT_PLAYER_HEIGHT       10.0f
#define DEFAULT_PLAYER_DEPTH        5.0f

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
    Vec3            center_pos;
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
    float           size; //size of hitbox
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
    Vec3 top;
    Vec3 bottom;
    float height;
    float radius;
    float score_req;
    uint8_t stage_num; //0-3
    Shader* shader;
    Color color;
    StatusEffects current_effect;
} Stage;
//[===========]

//[==GLOBALS==]
enum { NUM_STAGES = 4 };
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
Stage STAGES[NUM_STAGES];

Player PLAYERS[MAX_PLAYERS];
int    PLAYER_COUNT = 0;
int    RES_X        = 700;
int    RES_Y        = 700;
double MATCH_TIME   = DEFAULT_MATCH_TIME;
//[===========]

//[==HELPERS==]

Hitbox create_hitbox(Vec3 lowest_pt, float width, float height, float depth) {
    const Vec3 transformed = Vector3Add(lowest_pt, (Vec3){width, height, depth});
    return (Hitbox){
        (BoundingBox){
            .min = lowest_pt,
            .max = transformed
        },
        NO_HIT
    };
}

// center box
int update_aabb(BoundingBox* b, Vec3 p) {
    float half_x = (b->max.x - b->min.x) * 0.5f;
    float height = (b->max.y - b->min.y);
    float half_z = (b->max.z - b->min.z) * 0.5f;

    b->min = (Vec3){ p.x - half_x, p.y, p.z - half_z };
    b->max = (Vec3){ p.x + half_x, p.y + height,  p.z + half_z };

    return 1;
}
// passes PLAYERS array index (pidx), and position
int update_hitbox_pos(uint8_t pidx, Vec3 p) {
    update_aabb(&PLAYERS[pidx].hitbox.box, p);
    return 1;
}

void apply_effect(Player* p, StatusEffects e) {
    p->active_effect = e;
}

uint8_t is_barrier_collision(Stage* stage, Hitbox* incoming_player)
{
    if (CheckCollisionBoxSphere(incoming_player->box, stage->center, stage->radius)) return 1;
    return 0;
}
// fabfs()'s distances
inline float get_distance_xz(const Vec3 target_pt, const Vec3 source_pt) {
    float dx = fabsf(target_pt.x - source_pt.x);
    float dz = fabsf(target_pt.z - source_pt.z);
    return sqrtf(dx*dx + dz*dz);
}
//[===========]
uint8_t is_player_hit(Player* source, Player* target) {
    if (!source || !target || !target->active) return NO_HIT;
    return CheckCollisionBoxes(source->hitbox.box, target->hitbox.box);
}

// hit should be the return of iis_player_hit(atk'ng player, atk'd targeted player) result, 1 = true, 0 = false
uint8_t on_player_attacked(Player* source, Player* target, Attack* atk, uint8_t hit)
{
    //double check there's actually an attack
    if (!hit) return 0;
    target->hitbox.hit = HIT;
    if(atk->effect != NONE) apply_effect(target, atk->effect);
    return 1;
}

// what stage are you in the radius of
uint8_t get_stage_in(Vec3* p)
{
    for (int i = NUM_STAGES - 1; i >= 0; i--)
    {
        if (Vector3Distance(*p, STAGES[i].center) <= STAGES[i].radius)
            return i;
    }
    return 0;
}
//
uint8_t resolve_floor_collision(Player* p, uint8_t stage_n) {
    Stage s = STAGES[stage_n];
    Vec3* feet_pt = &p->hitbox.box.min;
    if(feet_pt->y < FLOOR) feet_pt->y = FLOOR;
    if(feet_pt->y < s.height) p->center_pos.y = s.height+p->size;
    return 1;
}

Vec2 get_horiz_dir(Vec3* vel_vec)
{
    Vec2 dir = { .x = vel_vec->x, .y = vel_vec->z };
    if(dir.x == 0 && dir.y == 0) return dir;
    if( (fabsf(vel_vec->x) <= 0.01f) && (fabsf(vel_vec->z) < 0.01f) ) return dir;
    return Vector2Normalize(dir);
}

uint8_t floor_collisions() {
    for (size_t i = 0; i < PLAYER_COUNT; i++)
    {
        Player* p = &PLAYERS[i];
        if (!p || !p->active) return 0;

        // Resolve Y-axis (Floor)
        float surface_y = STAGES[get_stage_in(&p->center_pos)].height;
        if (p->center_pos.y < surface_y) {
            p->center_pos.y = surface_y;
            p->vel.y = 0.0f;
}

        // After moving the center_pos, sync the hitbox
        update_hitbox_pos(p->pid, p->center_pos);
    }
    
    return 1;
}

uint8_t move(Vec3* p, Vec3* v, Vec3* a, float dt) {
    v->x += a->x * dt;
    v->y += (a->y + GRAVITY) * dt;
    v->z += a->z * dt;

    v->x *= FRICTION;
    v->z *= FRICTION;
    
    p->x += v->x * dt;
    p->y += v->y * dt;
    p->z += v->z * dt;
    return 1;
}

uint8_t controls(Player* p, float set_speed, float dt) {
    if (!p || !p->active)                   return 0;
    if (p->active_effect == STUNNED)         return 0;
    float speed = (p->active_effect == SLOWED) ? set_speed * 0.5f : set_speed;

    Vec3 fwd   = Vector3Normalize((Vec3){
        p->cam->target.x - p->cam->position.x, 0,
        p->cam->target.z - p->cam->position.z });
    Vec3 right = Vector3CrossProduct(fwd, UP_VECTOR);
    Vec3 a     = {0};

    if (IsKeyDown(KEY_W)) { a.x += fwd.x   * speed; a.z += fwd.z   * speed; }
    if (IsKeyDown(KEY_S)) { a.x -= fwd.x   * speed; a.z -= fwd.z   * speed; }
    if (IsKeyDown(KEY_A)) { a.x -= right.x * speed; a.z -= right.z * speed; }
    if (IsKeyDown(KEY_D)) { a.x += right.x * speed; a.z += right.z * speed; } 
    
    // FIX: Keyboard jump now acts as an impulse (like gamepad) and requires being on the floor
    if (IsKeyDown(KEY_SPACE)) { 
        p->vel.y = PLAYER_JUMP;
    }

    if (IsGamepadAvailable(0)) {
        float lx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float ly = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        a.x += (fwd.x * -ly + right.x * lx) * speed;
        a.z += (fwd.z * -ly + right.z * lx) * speed;
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) && p->center_pos.y <= FLOOR + 0.01f)
        { 
            p->vel.y = PLAYER_JUMP; //impulse
        }
    }
    p->accel = a;
    move(&p->center_pos, &p->vel, &p->accel, dt); 
    return 1;
}
uint8_t cam_handle(Player* p) {
    if (!p) return 0;

    Vector2 md  = GetMouseDelta();
    float pad_x = IsGamepadAvailable(DEFAULT_GAME_PAD)
                ? GetGamepadAxisMovement(DEFAULT_GAME_PAD, GAMEPAD_AXIS_RIGHT_X) * CAM_PAD_SENSITIVITY
                : 0.0f;

    Vec3 offset = Vector3Subtract(p->cam->position, p->cam->target);
    offset = Vector3RotateByAxisAngle(offset, UP_VECTOR, -(md.x * CAM_MOUSE_SENSITIVITY + pad_x));

    Vec3 right = Vector3Normalize(Vector3CrossProduct(offset, UP_VECTOR));
    offset = Vector3RotateByAxisAngle(offset, right, -(md.y * CAM_MOUSE_SENSITIVITY));

    if (Vector3Angle(offset, UP_VECTOR) < 0.2f || Vector3Angle(offset, Vector3Negate(UP_VECTOR)) < 0.2f)
        offset = Vector3Subtract(p->cam->position, p->cam->target);

    Vec3 target      = { p->center_pos.x, p->center_pos.y + 1.5f, p->center_pos.z };
    p->cam->position = Vector3Add(target, offset);
    p->cam->target   = target;

    return 1;
}

// create_player: value, no pointer deref
Player create_player(char* name, Vec3 feet_spawn_pt, PlayerType type) {
    Player p        = {0};
    p.name          = name;
    
    float hh = DEFAULT_PLAYER_HEIGHT/2.0f;

    if (feet_spawn_pt.y <= FLOOR) {
        feet_spawn_pt.y = FLOOR + hh;
    }
    
    p.center_pos    = (Vec3){feet_spawn_pt.x, feet_spawn_pt.y - hh, feet_spawn_pt.z};
    p.vel           = (Vec3){0};
    p.accel         = (Vec3){0};
    p.hitbox        = create_hitbox(feet_spawn_pt, DEFAULT_PLAYER_WIDTH, DEFAULT_PLAYER_HEIGHT, DEFAULT_PLAYER_DEPTH);
    p.type          = type;
    p.active_effect = NONE;
    p.score         = 0;
    p.active        = (type != UNASSIGNED) ? 1 : 0;
    p.cosmetic      = NULL;
    p.unlocked_stages = 0;

    Camera3D* cam   = (Camera3D*)malloc(sizeof(Camera3D));
    
    float cam_h = (p.hitbox.box.max.y - p.hitbox.box.min.y); 
    
    cam->position   = (Vec3){ feet_spawn_pt.x, cam_h, feet_spawn_pt.z + cam_h };
    cam->target     = (Vec3){ feet_spawn_pt.x, feet_spawn_pt.y, feet_spawn_pt.z }; 
    cam->up         = UP_VECTOR;
    cam->fovy       = 120.0f;
    cam->projection = CAMERA_PERSPECTIVE;
    p.cam           = cam;
    p.size = Vector3Distance(p.hitbox.box.min, p.hitbox.box.max);
    return p;
}

uint8_t init_stages(Player p)
{
    
    // Your hitbox half-width is 0.5, so base_r = 3.0f
    float base_r = p.hitbox.box.max.x * 60.0f; 

    // Stage 0 is the OUTERMOST ring, Stage 3 is the innermost core
    STAGES[0].radius = base_r * 4.0f; // 12.0
    STAGES[1].radius = base_r * 3.0f; // 9.0
    STAGES[2].radius = base_r * 2.0f; // 6.0
    STAGES[3].radius = base_r * 1.0f; // 3.0

    for (u_int8_t i = 0; i < 4; i++) {
        float scalar = i*0.005*base_r;
        STAGES[i].center = (Vec3){0,FLOOR,0};
        STAGES[i].bottom = (Vec3){0,FLOOR,0};
        STAGES[i].top    = (Vec3){0,scalar,0};
        STAGES[i].height = scalar;
        STAGES[i].current_effect = NONE;
        STAGES[i].score_req = i * 1000;
        STAGES[i].shader = NULL;
        STAGES[i].stage_num = i;
        STAGES[i].color = BLANK;
    }
    return 1;
}
// Set stage colors, shaders, etc
uint8_t set_stages(void)
{
    STAGES[0].color = WHITE;
    STAGES[0].current_effect = NONE;
    
    STAGES[1].color = BLUE;
    STAGES[1].current_effect = NONE;
    
    STAGES[2].color = YELLOW;
    STAGES[2].current_effect = NONE;
    
    STAGES[3].color = PURPLE;
    STAGES[3].current_effect = NONE;
    Vec4 blank = {BLANK.r, BLANK.g, BLANK.b, BLANK.a}; // this doesnt need to be here, maybe macro
    for (size_t i = 0; i < NUM_STAGES; i++)
    {
        Stage s = STAGES[i];
        Vec4 cval = { s.color.r, s.color.g, s.color.b, s.color.a };
        if(Vector4Equals(cval, blank)) { return 0; }
    }
    return 1;
}
void manage_unlocked_stages(uint8_t player, uint8_t stage) 
{
    if (PLAYERS[player].score >= STAGES[stage].score_req)
    {
        PLAYERS[player].unlocked_stages = stage;
    }
}
//only called if near wall/barrier edge
uint8_t can_proceed(Player* p, Stage s) 
{
    return p->unlocked_stages >= s.stage_num;
}
uint8_t stage_manager()
{
    Stage s0 = STAGES[0];
    Stage s1 = STAGES[1];
    Stage s2 = STAGES[2];
    Stage s3 = STAGES[3];

    for (size_t j = 0; j < 4; j++)
    {
        for (size_t i = 0; i < PLAYER_COUNT; i++)
        {
            manage_unlocked_stages(i, j);
        }
    }
    return 1;
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
    PLAYERS[p->pid] = create_player("NULL", (Vec3){0}, UNASSIGNED);
    return 1;
}
uint8_t update_PLAYERS(void) {
    for (u_int8_t i = 0; i < MAX_PLAYERS; i++) {
        if (PLAYERS[i].type == UNASSIGNED) continue;
        update_hitbox_pos(i, PLAYERS[i].center_pos);
    }
    return 1;
}
void draw_PLAYERS()
{
    Player* p = &PLAYERS[0];
    static uint8_t i = 0;
    static unsigned int tmp_width = 0;
    static unsigned int tmp_height = 0;
    static unsigned int tmp_length = 0;
    static float tmp_radius = 0.0f;
    if(!i)
    {
            tmp_width  = PLAYERS[0].hitbox.box.max.x-PLAYERS[0].hitbox.box.min.x;
            tmp_height = PLAYERS[0].hitbox.box.max.y-PLAYERS[0].hitbox.box.min.y;
            tmp_length = PLAYERS[0].hitbox.box.max.z-PLAYERS[0].hitbox.box.min.z;
            float dx = PLAYERS[0].hitbox.box.max.x - PLAYERS[0].hitbox.box.min.x;
            float dz = PLAYERS[0].hitbox.box.max.z - PLAYERS[0].hitbox.box.min.z;
            tmp_radius = fminf(dx, dz) * 0.5f;
            i = 1;
    }
    // DRAW PLAYERS TEMP
    Vec3 top = {p->center_pos.x, p->hitbox.box.max.y, p->center_pos.z};
    Vec3 bot = {p->center_pos.x, p->hitbox.box.min.y, p->center_pos.z};
    DrawCylinder(bot, DEFAULT_PLAYER_WIDTH, DEFAULT_PLAYER_WIDTH, tmp_height, 10, YELLOW);
    for (int i = 1; i < MAX_PLAYERS; i++) {
        //if (PLAYERS[i].type == UNASSIGNED) continue;
        DrawCube(PLAYERS[i].center_pos, tmp_width, tmp_width, tmp_length, RED);
    }
}

uint8_t place_players(void)
{
    Stage s = STAGES[3];
    for (size_t i = 0; i < PLAYER_COUNT; i++)
    {
        Vec3 pt = {
            .x = GetRandomValue(s.center.x - s.radius, s.center.x + s.radius),
            .y = FLOOR+PLAYERS[i].hitbox.box.min.y,
            .z = GetRandomValue(s.center.z - s.radius, s.center.z + s.radius)
        };
        PLAYERS[i].center_pos = pt;
        PLAYERS[i].active = 1;
    }
    return 1;
}
uint8_t bot_tick(Player* p, float dt) {
    
    if (!p || !p->active || p->active_effect == STUNNED || p->type == UNASSIGNED || p->type == PLAYER) return 0;
    
    Player* target      = &PLAYERS[0];
    
    // for (int i = 0; i < MAX_PLAYERS; i++) {
    //     if (&PLAYERS[i] == p || !PLAYERS[i].active) continue;
    //     float d = Vector3Distance(p->pos, PLAYERS[i].pos);
    //     if (d < closest) { closest = d; target = &PLAYERS[i]; }
    // }
    if (!target) return 0;
    // move towards it
    Vec3 dir    = Vector3Normalize(Vector3Subtract(target->center_pos, p->center_pos));
    float d     = Vector3Distance(p->center_pos, PLAYERS[0].center_pos);
    p->accel    = d > EPSILON ? (Vec3){ dir.x * DEFAULT_PLAYER_SPEED, 0, dir.z * DEFAULT_PLAYER_SPEED } : (Vec3){0};
    move(&p->center_pos, &p->vel, &p->accel, dt);
    Attack a = { 10, 3, 1, NONE };
    on_player_attacked(p, target, &a, is_player_hit(p, target));
    return 1;
}

//[==PLAYER COLLISION & REPULSION==]
uint8_t player_bump_collisions(void)
{
    for (int i = 0; i < MAX_PLAYERS; i++) {
        
        if (!PLAYERS[i].active || PLAYERS[i].type == UNASSIGNED) continue;

        for (int j = i + 1; j < MAX_PLAYERS; j++) {
            if (!PLAYERS[j].active || PLAYERS[j].type == UNASSIGNED) continue;

            // Hitbox collision
            if (!CheckCollisionBoxes(PLAYERS[i].hitbox.box, PLAYERS[j].hitbox.box))
                continue;

            // === REPULSION ===
            Vec3 diff = Vector3Subtract(PLAYERS[i].center_pos, PLAYERS[j].center_pos);
            float dist = Vector3Length(diff);

            if (dist < EPSILON) {
                diff = (Vec3){ 1.0f, 0.0f, 0.0f };
                dist = 1.0f;
            }

            Vec3 repel = Vector3Scale(Vector3Normalize(diff), 0.6f);

            PLAYERS[i].vel.x += repel.x;
            PLAYERS[i].vel.z += repel.z;
            PLAYERS[j].vel.x -= repel.x;
            PLAYERS[j].vel.z -= repel.z;

            // Separate overlapping positions immediately
            float overlap = 1.0f - dist;
            if (overlap > 0.0f) {
                PLAYERS[i].center_pos.x += repel.x * overlap * 0.5f;
                PLAYERS[i].center_pos.z += repel.z * overlap * 0.5f;
                PLAYERS[j].center_pos.x -= repel.x * overlap * 0.5f;
                PLAYERS[j].center_pos.z -= repel.z * overlap * 0.5f;
                
                // Keep using YOUR helper after separation
                update_hitbox_pos(i, PLAYERS[i].center_pos);
                update_hitbox_pos(j, PLAYERS[j].center_pos);
            }

            // === ATTACKER DETERMINATION (momentum-based) ===
            float i_momentum = Vector3DotProduct(PLAYERS[i].vel, Vector3Negate(repel));
            float j_momentum = Vector3DotProduct(PLAYERS[j].vel, repel);

            uint8_t pts = 10;

            if (i_momentum > j_momentum && PLAYERS[j].score >= pts) {
                // Player i is the attacker — gains points
                PLAYERS[i].score += pts;
                PLAYERS[j].score -= pts;
                PLAYERS[j].hitbox.hit = HIT;
            }
            else if (j_momentum > i_momentum && PLAYERS[i].score >= pts) {
                // Player j is the attacker — gains points
                PLAYERS[j].score += pts;
                PLAYERS[i].score -= pts;
                PLAYERS[i].hitbox.hit = HIT;
            }
            
        }
    }
    return 1;
}


//[==ALWAYS-VISIBLE STAGE FLOORS==]
void draw_stage(void)
{
    for (size_t i = 0; i < 4; i++)
    {
        Stage s = STAGES[i];
        // get rid of z-fighting
        Vec3 pos = { s.center.x, s.center.y + 0.5f + (i * 0.01f), s.center.z };
        //DrawCylinder(pos, s.radius, s.radius, 1, 10, s.color);
        DrawCylinderEx(s.bottom, s.top, s.radius, s.radius, 10, s.color);
    }
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
