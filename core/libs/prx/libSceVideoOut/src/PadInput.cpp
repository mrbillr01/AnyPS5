#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#include <SDL3/SDL.h>
#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/PadInputTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"

PadInput::PadInput() : PadInput(Pad::LoadInputMapping()) {
}

PadInput::PadInput(Pad::InputConfiguration configuration) : keyboardMouse(std::move(configuration)) {
    openFirstAvailableController();
}

PadInput::~PadInput() {
    closeController();
}

void PadInput::openFirstAvailableController() {
    if (controller != nullptr) return;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");
    if ((SDL_WasInit(SDL_INIT_GAMEPAD) & SDL_INIT_GAMEPAD) == 0) {
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            APS5_LOG_ERR("Pad: SDL game controller init failed: %s", SDL_GetError());
            return;
        }
    }
    int count = 0;
    SDL_JoystickID* devices = SDL_GetGamepads(&count);
    for (int index = 0; index < count && controller == nullptr; ++index) openController(devices[index]);
    SDL_free(devices);
}

void PadInput::openController(int deviceIndex) {
    if (controller != nullptr || !SDL_IsGamepad(deviceIndex)) return;
    controller = SDL_OpenGamepad(deviceIndex);
    if (controller == nullptr) {
        APS5_LOG_ERR("Pad: could not open game controller %d: %s", deviceIndex, SDL_GetError());
        return;
    }
    const char* name = SDL_GetGamepadName(controller);
    APS5_LOG_OUT("Pad: connected game controller: %s (type %d, sensors accel=%d gyro=%d, touchpads=%d, led=%d, trigger rumble=%d)",
        name != nullptr ? name : "unknown", static_cast<int>(SDL_GetGamepadType(controller)),
        SDL_GamepadHasSensor(controller, SDL_SENSOR_ACCEL) == true, SDL_GamepadHasSensor(controller, SDL_SENSOR_GYRO) == true,
        SDL_GetNumGamepadTouchpads(controller), SDL_GetBooleanProperty(SDL_GetGamepadProperties(controller), SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false), SDL_GetBooleanProperty(SDL_GetGamepadProperties(controller), SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false));
    enableSensors();
    outputPending = true;
}

void PadInput::enableSensors() {
    if (controller == nullptr) return;
    const bool wanted = outputState.motionEnabled ? true : false;
    if (SDL_GamepadHasSensor(controller, SDL_SENSOR_ACCEL) == true) SDL_SetGamepadSensorEnabled(controller, SDL_SENSOR_ACCEL, wanted);
    if (SDL_GamepadHasSensor(controller, SDL_SENSOR_GYRO) == true) SDL_SetGamepadSensorEnabled(controller, SDL_SENSOR_GYRO, wanted);
}

void PadInput::closeController() {
    if (controller == nullptr) return;
    SDL_CloseGamepad(controller);
    controller = nullptr;
    controllerState = {};
}

void PadInput::applyOutput() {
    PadOutputState fetched;
    if (PadFetchOutput_nid_postfix(&outputSequence, &fetched)) {
        const bool motionChanged = fetched.motionEnabled != outputState.motionEnabled;
        outputState = fetched;
        outputPending = true;
        if (motionChanged) enableSensors();
    }
    if (controller == nullptr) return;
    const auto now = std::chrono::steady_clock::now();
    const bool isPs5 = SDL_GetGamepadType(controller) == SDL_GAMEPAD_TYPE_PS5;
    const auto large = isPs5 ? outputState.vibrationLarge : std::max(outputState.vibrationLarge, outputState.audioVibrationLarge);
    const auto small = isPs5 ? outputState.vibrationSmall : std::max(outputState.vibrationSmall, outputState.audioVibrationSmall);
    const bool rumbling = large != 0 || small != 0;
    const bool triggerRumble = outputState.trigger[0].fallback != 0 || outputState.trigger[1].fallback != 0;
    if (!outputPending) {
        if ((rumbling || (triggerRumble && !isPs5)) && now >= nextRumbleRefresh) outputPending = true;
        else return;
    }
    outputPending = false;
    nextRumbleRefresh = now + std::chrono::milliseconds(700);
    constexpr Uint32 rumbleMs = 2000;
    SDL_RumbleGamepad(controller, static_cast<Uint16>(large * 257), static_cast<Uint16>(small * 257), rumbling ? rumbleMs : 0);
    if (SDL_GetBooleanProperty(SDL_GetGamepadProperties(controller), SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false)) {
        if (outputState.lightBarValid) SDL_SetGamepadLED(controller, outputState.lightBar[0], outputState.lightBar[1], outputState.lightBar[2]);
        else SDL_SetGamepadLED(controller, 0, 64, 255);
    }
    if (outputState.triggerTouched) {
        if (isPs5) {
            Uint8 effect[47] = {};
            effect[0] = 0x04 | 0x08;
            std::memcpy(effect + 10, outputState.trigger[1].effect, 11);
            std::memcpy(effect + 21, outputState.trigger[0].effect, 11);
            SDL_SendGamepadEffect(controller, effect, sizeof(effect));
        } else if (SDL_GetBooleanProperty(SDL_GetGamepadProperties(controller), SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN, false)) {
            SDL_RumbleGamepadTriggers(controller, static_cast<Uint16>(outputState.trigger[0].fallback * 257), static_cast<Uint16>(outputState.trigger[1].fallback * 257), triggerRumble ? rumbleMs : 0);
        }
    }
}

void PadInput::setMouseMode(bool enabled) {
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, "0");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "0");
    if (!SDL_SetWindowRelativeMouseMode(SDL_GetKeyboardFocus(), enabled)) throw std::runtime_error(std::string("Pad: relative mouse mode failed: ") + SDL_GetError());
    keyboardMouse.SetMouseMode(enabled);
}

PadInputState PadInput::sampleController() const {
    PadInputState result;
    if (controller == nullptr) return result;
    const auto readButton = [this](SDL_GamepadButton button) {
        return SDL_GetGamepadButton(controller, button) != 0;
    };
    const auto addButton = [&result, &readButton](SDL_GamepadButton source, Pad::PadButton button) {
        if (readButton(source)) result.buttons |= static_cast<std::uint32_t>(button);
    };
    addButton(SDL_GAMEPAD_BUTTON_SOUTH, Pad::PadButton::Cross);
    addButton(SDL_GAMEPAD_BUTTON_EAST, Pad::PadButton::Circle);
    addButton(SDL_GAMEPAD_BUTTON_WEST, Pad::PadButton::Square);
    addButton(SDL_GAMEPAD_BUTTON_NORTH, Pad::PadButton::Triangle);
    addButton(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, Pad::PadButton::L1);
    addButton(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, Pad::PadButton::R1);
    const auto type = SDL_GetGamepadType(controller);
    const bool viewPressed = type != SDL_GAMEPAD_TYPE_PS4 && type != SDL_GAMEPAD_TYPE_PS5 && readButton(SDL_GAMEPAD_BUTTON_BACK);
    if (viewPressed) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::TouchPad);
    addButton(SDL_GAMEPAD_BUTTON_START, Pad::PadButton::Options);
    addButton(SDL_GAMEPAD_BUTTON_LEFT_STICK, Pad::PadButton::L3);
    addButton(SDL_GAMEPAD_BUTTON_RIGHT_STICK, Pad::PadButton::R3);
    addButton(SDL_GAMEPAD_BUTTON_DPAD_UP, Pad::PadButton::Up);
    addButton(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Pad::PadButton::Right);
    addButton(SDL_GAMEPAD_BUTTON_DPAD_DOWN, Pad::PadButton::Down);
    addButton(SDL_GAMEPAD_BUTTON_DPAD_LEFT, Pad::PadButton::Left);
    addButton(SDL_GAMEPAD_BUTTON_TOUCHPAD, Pad::PadButton::TouchPad);

    const auto triggerValue = [this](SDL_GamepadAxis axis) {
        const auto value = std::clamp<int>(SDL_GetGamepadAxis(controller, axis), 0, 32767);
        return static_cast<std::uint8_t>((value * 255 + 16383) / 32767);
    };
    result.analogButtonsL2 = triggerValue(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    result.analogButtonsR2 = triggerValue(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    if (result.analogButtonsL2 != 0) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::L2);
    if (result.analogButtonsR2 != 0) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::R2);

    const auto stick = [this](SDL_GamepadAxis xAxis, SDL_GamepadAxis yAxis, int deadzonePercent) {
        return Pad::StickWithDeadzone(SDL_GetGamepadAxis(controller, xAxis), SDL_GetGamepadAxis(controller, yAxis), deadzonePercent);
    };
    const auto left = stick(SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, keyboardMouse.Configuration().settings.leftStickDeadzonePercent);
    const auto right = stick(SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY, keyboardMouse.Configuration().settings.rightStickDeadzonePercent);
    result.sticks = {left[0], left[1], right[0], right[1]};
    switch (SDL_GetGamepadType(controller)) {
        case SDL_GAMEPAD_TYPE_PS5: result.deviceKind = 1; break;
        case SDL_GAMEPAD_TYPE_PS4: result.deviceKind = 2; break;
        default: result.deviceKind = 3; break;
    }
    if (SDL_GamepadSensorEnabled(controller, SDL_SENSOR_ACCEL) == true && SDL_GamepadSensorEnabled(controller, SDL_SENSOR_GYRO) == true) {
        float accel[3];
        float gyro[3];
        if (SDL_GetGamepadSensorData(controller, SDL_SENSOR_ACCEL, accel, 3) && SDL_GetGamepadSensorData(controller, SDL_SENSOR_GYRO, gyro, 3)) {
            result.hasMotion = true;
            for (int i = 0; i < 3; ++i) { result.accel[i] = accel[i]; result.gyro[i] = gyro[i]; }
        }
    }
    if (SDL_GetNumGamepadTouchpads(controller) > 0) {
        for (int finger = 0; finger < 2; ++finger) {
            bool down = false;
            float x = 0.0f;
            float y = 0.0f;
            float pressure = 0.0f;
            if (!SDL_GetGamepadTouchpadFinger(controller, 0, finger, &down, &x, &y, &pressure) || down == 0) continue;
            result.touch[finger].active = true;
            result.touch[finger].x = static_cast<std::uint16_t>(std::clamp(x, 0.0f, 1.0f) * 1919.0f);
            result.touch[finger].y = static_cast<std::uint16_t>(std::clamp(y, 0.0f, 1.0f) * 942.0f);
        }
    }
    if (viewPressed && !result.touch[0].active && !result.touch[1].active) {
        result.touch[0] = {true, 960, 471, 0};
    }
    return result;
}

void PadInput::HandleEvent(const SDL_Event& event, DisplayWindow& window) {
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        openController(event.gdevice.which);
        return;
    }
    if (event.type == SDL_EVENT_GAMEPAD_REMOVED && controller != nullptr) {
        const auto instanceId = SDL_GetJoystickID(SDL_GetGamepadJoystick(controller));
        if (instanceId == event.gdevice.which) {
            closeController();
            openFirstAvailableController();
            publish();
        }
        return;
    }
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP ||
        event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION || event.type == SDL_EVENT_GAMEPAD_REMAPPED) {
        publish();
        return;
    }
    if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
        if (keyboardMouse.HandleEvent(event, std::chrono::steady_clock::now()).releaseMouse) setMouseMode(false);
        publish();
        return;
    }
    // APS5_NO_PAD_INPUT=1 keeps a measurement run from reacting to keys or mouse buttons that reach
    // its window (a stray press advances the title into another stage).
    static const bool ignoreInput = std::getenv("APS5_NO_PAD_INPUT") != nullptr;
    const bool input = event.type == SDL_EVENT_MOUSE_WHEEL || event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP ||
        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP;
    if (!input || ignoreInput) return;
    if (event.type == SDL_EVENT_KEY_DOWN && ImeCapturesKeyboard_nid_postfix()) return;
    const auto requests = keyboardMouse.HandleEvent(event, std::chrono::steady_clock::now());
    if (requests.toggleFullscreen && window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) window.ToggleFullscreen();
    if (requests.toggleMouse) setMouseMode(!keyboardMouse.MouseMode());
    if (event.type != SDL_EVENT_MOUSE_MOTION) publish();
}

void PadInput::Update() {
    if (controller != nullptr) SDL_UpdateGamepads();
    applyOutput();
    if (controller != nullptr) controllerState = sampleController();
    if (keyboardMouse.MouseMode() && SDL_GetKeyboardFocus() == nullptr) {
        keyboardMouse.Release();
        setMouseMode(false);
        publish();
        return;
    }
    const bool changed = keyboardMouse.Advance(std::chrono::steady_clock::now());
    if (controller != nullptr || keyboardMouse.MouseMode() || changed) publish();
}

void PadInput::publish() {
    PadPublishInput_nid_postfix(keyboardMouse.Compose(controllerState));
}
