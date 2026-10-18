#include "prx/libSceVideoOut/include/ImeTextInput.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"

#include <cstring>

#include <SDL3/SDL_keyboard.h>

void ImeTextInput::HandleEvent(const SDL_Event& event, unsigned windowId) {
    ImeHostInput input{};
    switch (event.type) {
        case SDL_EVENT_TEXT_INPUT:
            if (event.text.windowID != windowId) return;
            if (!event.text.text) return;
            std::strncpy(input.text, event.text.text, sizeof(input.text) - 1);
            input.kind = ImeHostInputKind::Text;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.windowID != windowId) return;
            switch (event.key.scancode) {
                case SDL_SCANCODE_BACKSPACE: input.kind = ImeHostInputKind::Backspace; break;
                case SDL_SCANCODE_DELETE: input.kind = ImeHostInputKind::Delete; break;
                case SDL_SCANCODE_LEFT: input.kind = ImeHostInputKind::Left; break;
                case SDL_SCANCODE_RIGHT: input.kind = ImeHostInputKind::Right; break;
                case SDL_SCANCODE_HOME: input.kind = ImeHostInputKind::Home; break;
                case SDL_SCANCODE_END: input.kind = ImeHostInputKind::End; break;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                    if (event.key.repeat != 0) return;
                    input.kind = ImeHostInputKind::Enter;
                    break;
                case SDL_SCANCODE_ESCAPE:
                    if (event.key.repeat != 0) return;
                    input.kind = ImeHostInputKind::Escape;
                    break;
                default: return;
            }
            break;
        default:
            return;
    }
    ImePublishInput_nid_postfix(input);
}
