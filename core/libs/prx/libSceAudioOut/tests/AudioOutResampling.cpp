#include <SDL3/SDL.h>
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern "C" {
int APS5_VABI sceAudioOutOpen(int, int, int, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceAudioOutOutput(int, const void*);
int APS5_VABI sceAudioOutClose(int);
}

namespace {

constexpr std::uint32_t grain = 256;
constexpr int hostRate = 48000;
SDL_AudioStream* conversion = nullptr;
std::vector<float> played;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void Collect() {
    const int bytes = SDL_GetAudioStreamAvailable(conversion);
    Require(bytes >= 0 && bytes % static_cast<int>(sizeof(float)) == 0, "invalid converted size");
    const auto first = played.size();
    played.resize(first + bytes / sizeof(float));
    if (bytes != 0) Require(SDL_GetAudioStreamData(conversion, played.data() + first, bytes) == bytes, "stream read failed");
}

void TestBoundary(std::uint32_t sourceRate) {
    played.clear();
    const int port = sceAudioOutOpen(0, 0, 0, grain, sourceRate, 4);
    Require(port > 0, "port creation failed");
    std::vector<float> data(grain * 2, 0.0f);
    data[(grain - 1) * 2] = 0.5f;
    data[(grain - 1) * 2 + 1] = 0.25f;
    Require(sceAudioOutOutput(port, data.data()) == static_cast<int>(grain), "first output failed");
    data.assign(grain * 2, 0.0f);
    for (int block = 0; block < 8; block++) Require(sceAudioOutOutput(port, data.data()) == static_cast<int>(grain), "trailing output failed");
    Require(sceAudioOutOutput(port, nullptr) == static_cast<int>(grain), "drain failed");
    Require(SDL_FlushAudioStream(conversion), "host conversion flush failed");
    Collect();
    Require(sceAudioOutClose(port) == 0, "close failed");
    conversion = nullptr;
    const auto boundary = static_cast<std::size_t>(std::ceil(static_cast<double>(grain) * hostRate / sourceRate));
    Require(played.size() / 2 > boundary + 16, "resampled tail missing");
    double tailEnergy = 0.0;
    for (std::size_t frame = boundary; frame < boundary + 16; frame++) {
        const float left = played[frame * 2];
        const float right = played[frame * 2 + 1];
        Require(std::isfinite(left) && std::isfinite(right), "nonfinite resampled output");
        Require(std::abs(left - 2.0f * right) < 1e-6f, "stereo channels changed during conversion");
        tailEnergy += static_cast<double>(left) * left;
    }
    Require(tailEnergy > 1e-7, "resampling discarded the impulse response at the grain boundary");
}

}

extern "C" bool SDLCALL SDL_InitSubSystem(SDL_InitFlags) { return true; }
extern "C" SDL_InitFlags SDLCALL SDL_WasInit(SDL_InitFlags) { return SDL_INIT_AUDIO; }
extern "C" bool SDLCALL SDL_ResumeAudioStreamDevice(SDL_AudioStream*) { return true; }
extern "C" int SDLCALL SDL_GetAudioStreamQueued(SDL_AudioStream*) {
    Collect();
    return 0;
}
extern "C" SDL_AudioStream* SDLCALL SDL_OpenAudioDeviceStream(SDL_AudioDeviceID, const SDL_AudioSpec* desired, SDL_AudioStreamCallback, void*) {
    const SDL_AudioSpec host{SDL_AUDIO_F32, 2, hostRate};
    conversion = SDL_CreateAudioStream(desired, &host);
    Require(conversion != nullptr, "host conversion stream creation failed");
    return conversion;
}

int main() {
    TestBoundary(32000);
    TestBoundary(44100);
    return 0;
}
