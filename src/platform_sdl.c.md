#include "platform.h"
#include <SDL3/SDL.h>
// Here you would include your SDL_GPU setup code

struct PlatformRenderer {
    SDL_Window* window;
    SDL_GPUDevice* device;
    // SDL3 GPU pipelines, shaders, etc.
};

void Platform_DrawCube(PlatformRenderer* p, Vec3 pos, Vec3 size, uint32_t color) {
    // Here you'd bind your 3D pipeline and push constants to the GPU
}