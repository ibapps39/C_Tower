// SDL includes
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

//[==MACROS, Macro functions, Definitions, Constants==]
//[===================================================]

// DATA STRUCTS (structs, enums, etc)
typedef struct Vec3 {float x; float y; float z;} Vec3;
typedef struct Vec4 {float x; float y; float z; float w;} Vec4;

//[==GLOBALS==]
// Game global, shared state - relevant to all players
typedef struct Game
{
    // Leadeing score, player
    // Leading Team
    // Time left
    // Highest Level open
} Game;
//[===================================================]

//[==FUNCTIONS========================================]

// platform.h - No SDL or Raylib headers allowed here!
typedef struct PlatformRenderer PlatformRenderer; // Opaque struct

// Platform lifecycle
PlatformRenderer* Platform_Init(int width, int height, const char* title);
void Platform_Shutdown(PlatformRenderer* p);
bool Platform_ShouldClose(PlatformRenderer* p);

// Input
bool Platform_IsKeyPressed(PlatformRenderer* p, int key);

// Timing
float Platform_GetTime(PlatformRenderer* p);


// 3D Drawing (High level)
void Platform_BeginFrame(PlatformRenderer* p);
void Platform_EndFrame(PlatformRenderer* p);
void Platform_DrawCube(PlatformRenderer* p, Vec3 pos, Vec3 size, uint32_t color);

//[===================================================]