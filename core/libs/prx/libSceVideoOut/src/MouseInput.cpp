#include "prx/libSceVideoOut/include/MouseInput.hpp"
#include "prx/libSceMouse/include/MouseState.hpp"

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <cmath>
#include <cstdio>

void MouseInput::HandleEvent(const SDL_Event& event, unsigned windowId) {
    MouseInputEvent input{};
    switch (event.type) {
        case SDL_EVENT_MOUSE_MOTION:
            if (event.motion.windowID != windowId || !focused) return;
            input.x = scaled(event.motion.xrel, scaleX, remainderX);
            input.y = scaled(event.motion.yrel, scaleY, remainderY);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.windowID != windowId || !focused) return;
            switch (event.button.button) {
                case SDL_BUTTON_LEFT: input.button = 1; break;
                case SDL_BUTTON_RIGHT: input.button = 2; break;
                case SDL_BUTTON_MIDDLE: input.button = 4; break;
                default: return;
            }
            input.pressed = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (event.wheel.windowID != windowId || !focused) return;
            input.wheel = event.wheel.y;
            input.tilt = event.wheel.x;
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                input.wheel = -input.wheel;
                input.tilt = -input.tilt;
            }
            if (input.wheel == 0 && input.tilt == 0) return;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (event.window.windowID != windowId) return;
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                focused = false;
                input.resetButtons = true;
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                focused = true;
                input.connectionChange = true;
            } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                focused = false;
                input.connectionChange = true;
                input.connected = false;
            } else {
                return;
            }
            break;
        default:
            return;
    }
    MousePublishInput_nid_no_patch(input);
}

void MouseInput::SetPresentation(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t presentedWidth, std::uint32_t presentedHeight) {
    if (sourceWidth == 0 || sourceHeight == 0 || presentedWidth == 0 || presentedHeight == 0) return;
    const double x = static_cast<double>(sourceWidth) / static_cast<double>(presentedWidth);
    const double y = static_cast<double>(sourceHeight) / static_cast<double>(presentedHeight);
    if (x != scaleX || y != scaleY) {
        std::fprintf(stderr, "[mouse] video driver %s: guest %ux%u presented at %ux%u, delta scale %.4f x %.4f\n", SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "?", sourceWidth, sourceHeight, presentedWidth, presentedHeight, x, y);
        scaleX = x;
        scaleY = y;
        remainderX = 0.0;
        remainderY = 0.0;
    }
}

std::int32_t MouseInput::scaled(std::int32_t delta, double scale, double& remainder) {
    const double exact = static_cast<double>(delta) * scale + remainder;
    const double whole = std::trunc(exact);
    remainder = exact - whole;
    return static_cast<std::int32_t>(whole);
}

bool MouseInput::CaptureWanted(bool allowed) const {
    return allowed && focused && MouseIsOpen_nid_no_patch();
}

void MouseInput::UpdateCapture(bool allowed) {
    const bool wanted = CaptureWanted(allowed);
    if (wanted == captured && (SDL_GetWindowRelativeMouseMode(SDL_GetMouseFocus()) == true) == wanted) return;
    if (!SDL_SetWindowRelativeMouseMode(SDL_GetMouseFocus(), wanted)) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr, "[mouse] relative mouse mode unavailable: %s\n", SDL_GetError());
        }
        return;
    }
    float ignoredX = 0;
    float ignoredY = 0;
    SDL_GetRelativeMouseState(&ignoredX, &ignoredY);
    remainderX = 0.0;
    remainderY = 0.0;
    captured = wanted;
}
