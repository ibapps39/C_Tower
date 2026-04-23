#include "platform.h"
#include <raylib.h>

struct PlatformRenderer {
    Camera camera; // Raylib specific data
};

PlatformRenderer* Platform_Init(int w, int h, const char* t) {
    InitWindow(w, h, t);
    PlatformRenderer* p = malloc(sizeof(PlatformRenderer));
    p->camera = (Camera){ {0, 10, 10}, {0, 0, 0}, {0, 1, 0}, 45.0f, 0 };
    return p;
}

void Platform_DrawCube(PlatformRenderer* p, Vec3 pos, Vec3 size, uint32_t color) {
    DrawCube((Vector3){pos.x, pos.y, pos.z}, size.x, size.y, size.z, GetColor(color));
}
void Platform_DrawCapsule()