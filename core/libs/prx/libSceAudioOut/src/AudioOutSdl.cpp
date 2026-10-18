#include "prx/libSceAudioOut/include/AudioOutSdl.hpp"

#include <SDL3/SDL.h>
#include <mutex>

int InitializeSdlAudio_nid_no_patch() {
    static std::mutex initializationMutex;
    std::lock_guard lock(initializationMutex);
    return SDL_InitSubSystem(SDL_INIT_AUDIO) ? 0 : -1;
}
