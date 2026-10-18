#include "prx/libSceVideoOut/include/KeyboardInput.hpp"
#include "prx/libSceKeyboard/include/KeyboardState.hpp"

#include <SDL3/SDL_keyboard.h>

void KeyboardInput::HandleEvent(const SDL_Event& event, unsigned windowId) {
    KeyboardInputEvent input{};
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (event.key.windowID != windowId || !focused || event.key.repeat != 0) return;
            if (event.key.scancode <= SDL_SCANCODE_UNKNOWN || event.key.scancode > SDL_SCANCODE_RGUI) return;
            input.keyCode = static_cast<std::uint16_t>(event.key.scancode);
            input.pressed = event.type == SDL_EVENT_KEY_DOWN;
            if ((event.key.mod & SDL_KMOD_NUM) != 0) input.led |= KEYBOARD_LED_NUM_LOCK;
            if ((event.key.mod & SDL_KMOD_CAPS) != 0) input.led |= KEYBOARD_LED_CAPS_LOCK;
            if ((event.key.mod & SDL_KMOD_SCROLL) != 0) input.led |= KEYBOARD_LED_SCROLL_LOCK;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (event.window.windowID != windowId) return;
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                focused = false;
                input.resetKeys = true;
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
    KeyboardPublishInput_nid_postfix(input);
}
