#ifdef ANYPS5_SYSTEM_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL_build_config.h>
#endif

#include <cstdio>

int main() {
#ifdef ANYPS5_SYSTEM_SDL3
    if (!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_HIDAPI, true)) {
        std::fputs("pad_sdl: system SDL3 HIDAPI is disabled by a runtime hint\n", stderr);
        return 1;
    }
    std::puts("pad_sdl: system SDL3 HIDAPI policy enabled; controller hardware is required to verify its backend");
    return 0;
#else
#ifdef SDL_JOYSTICK_HIDAPI
    std::puts("pad_sdl: SDL drives controllers through HIDAPI");
    return 0;
#else
    std::fputs("pad_sdl: SDL is built without HIDAPI, so a DualSense gets no light bar, touchpad or trigger effects\n", stderr);
    return 1;
#endif
#endif
}
