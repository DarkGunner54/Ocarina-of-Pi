#include "game_input.h"
#include <cstring>
#include <cstdio>
#include <iostream>

GameInput& GameInput::instance() { static GameInput input; return input; }

GameInput::GameInput()
    : controller_(nullptr)
    , state_()
    , config_()
{
    config_.deadzone = 8000;
    config_.active_zone = 25000;
    config_.max_value = 32767;
    config_.auto_map = true;
}

bool GameInput::initialize() {
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    }

    if (SDL_NumJoysticks() > 0) {
        std::cout << "[GameInput] Attempting controller mapping load" << std::endl;
    }

    if (!openController()) {
        std::cout << "[GameInput] No controller detected — keyboard will be used" << std::endl;
    }

    clearState();
    return true;
}

void GameInput::shutdown() {
    closeController();

    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
    }
}

bool GameInput::openController() {
    int num_joysticks = SDL_NumJoysticks();

    for (int i = 0; i < num_joysticks; i++) {
        if (!SDL_IsGameController(i)) {
            continue;
        }

        controller_ = SDL_GameControllerOpen(i);
        if (controller_) {
            std::cout << "[GameInput] Controller detected: "
                      << SDL_GameControllerName(controller_) << std::endl;

            if (SDL_GameControllerGetAttached(controller_)) {
                std::cout << "[GameInput] Controller mapping: "
                          << SDL_GameControllerMapping(controller_) << std::endl;
            }

            return true;
        } else {
            std::cerr << "[GameInput] Failed to open controller "
                      << SDL_JoystickNameForIndex(i) << ": "
                      << SDL_GetError() << std::endl;
        }
    }

    return false;
}

void GameInput::closeController() {
    if (controller_) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
        std::cout << "[GameInput] Controller closed" << std::endl;
    }
}

void GameInput::clearState() {
    memset(&state_, 0, sizeof(state_));
    state_.control_stick.x = 0;
    state_.control_stick.y = 0;
    state_.c_stick_raw.x = 0;
    state_.c_stick_raw.y = 0;
    state_.dpad.up = false;
    state_.dpad.down = false;
    state_.dpad.left = false;
    state_.dpad.right = false;
    state_.c_buttons.up = false;
    state_.c_buttons.down = false;
    state_.c_buttons.left = false;
    state_.c_buttons.right = false;
}

void GameInput::poll() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                break;

            case SDL_CONTROLLERAXISMOTION:
                if (controller_) {
                    processControllerAxis(event.caxis);
                }
                break;

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
                if (controller_) {
                    processControllerButton(event.cbutton);
                }
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                std::cout << "[GameInput] Controller removed" << std::endl;
                closeController();
                break;

            case SDL_CONTROLLERDEVICEREMAPPED:
                std::cout << "[GameInput] Controller remapped" << std::endl;
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                bool pressed = (event.type == SDL_KEYDOWN);
                SDL_Keycode key = event.key.keysym.sym;
                uint32_t button = 0;

                switch (key) {
                    case SDLK_z: button = static_cast<uint32_t>(ControllerButton::A); break;
                    case SDLK_x: button = static_cast<uint32_t>(ControllerButton::B); break;
                    case SDLK_RETURN: button = static_cast<uint32_t>(ControllerButton::START); break;
                    case SDLK_LSHIFT: button = static_cast<uint32_t>(ControllerButton::Z); break;
                    case SDLK_LEFTBRACKET: button = static_cast<uint32_t>(ControllerButton::L); break;
                    case SDLK_RIGHTBRACKET: button = static_cast<uint32_t>(ControllerButton::R); break;
                    case SDLK_UP: case SDLK_w: button = static_cast<uint32_t>(ControllerButton::UP); break;
                    case SDLK_DOWN: case SDLK_s: button = static_cast<uint32_t>(ControllerButton::DOWN); break;
                    case SDLK_LEFT: case SDLK_a: button = static_cast<uint32_t>(ControllerButton::LEFT); break;
                    case SDLK_RIGHT: case SDLK_d: button = static_cast<uint32_t>(ControllerButton::RIGHT); break;
                    default: break;
                }

                if (button != 0) {
                    if (pressed) {
                        state_.buttons |= button;
                    } else {
                        state_.buttons &= ~button;
                    }
                }
                break;
            }

            default:
                break;
        }
    }
}

void GameInput::processControllerAxis(SDL_ControllerAxisEvent& event) {
    switch (event.axis) {
        case SDL_CONTROLLER_AXIS_LEFTX:
            state_.control_stick.x = event.value;
            break;

        case SDL_CONTROLLER_AXIS_LEFTY:
            state_.control_stick.y = event.value;
            break;

        case SDL_CONTROLLER_AXIS_RIGHTX:
            state_.c_stick_raw.x = event.value;
            break;

        case SDL_CONTROLLER_AXIS_RIGHTY:
            state_.c_stick_raw.y = event.value;
            break;

        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
            if (event.value > config_.active_zone) {
                state_.buttons |= static_cast<uint32_t>(ControllerButton::L);
            } else {
                state_.buttons &= ~static_cast<uint32_t>(ControllerButton::L);
            }
            break;

        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
            if (event.value > config_.active_zone) {
                state_.buttons |= static_cast<uint32_t>(ControllerButton::R);
            } else {
                state_.buttons &= ~static_cast<uint32_t>(ControllerButton::R);
            }
            break;

        default:
            break;
    }

    stickToDPad(state_.control_stick.x, state_.control_stick.y, state_.dpad);
    stickToDPad(state_.c_stick_raw.x, state_.c_stick_raw.y, state_.c_buttons);
}

void GameInput::processControllerButton(SDL_ControllerButtonEvent& event) {
    uint32_t button = 0;
    bool pressed = (event.state == SDL_PRESSED);

    switch (event.button) {
        case SDL_CONTROLLER_BUTTON_A:
            button = static_cast<uint32_t>(ControllerButton::A);
            break;
        case SDL_CONTROLLER_BUTTON_B:
            button = static_cast<uint32_t>(ControllerButton::B);
            break;
        case SDL_CONTROLLER_BUTTON_START:
            button = static_cast<uint32_t>(ControllerButton::START);
            break;
        case SDL_CONTROLLER_BUTTON_BACK:
            button = static_cast<uint32_t>(ControllerButton::Z);
            break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            button = static_cast<uint32_t>(ControllerButton::L);
            break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            button = static_cast<uint32_t>(ControllerButton::R);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            if (pressed) state_.dpad.up = true;
            else state_.dpad.up = false;
            return;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            if (pressed) state_.dpad.down = true;
            else state_.dpad.down = false;
            return;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            if (pressed) state_.dpad.left = true;
            else state_.dpad.left = false;
            return;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            if (pressed) state_.dpad.right = true;
            else state_.dpad.right = false;
            return;
        default:
            return;
    }

    if (button != 0) {
        if (pressed) {
            state_.buttons |= button;
        } else {
            state_.buttons &= ~button;
        }
    }
}

void GameInput::stickToDPad(int16_t raw_x, int16_t raw_y, DPadState& out_state) {
    int16_t dx, dy;
    applyDeadzone(raw_x, raw_y, &dx, &dy);

    if (dx > config_.active_zone) {
        out_state.right = true;
        out_state.left = false;
    } else if (dx < -config_.active_zone) {
        out_state.left = true;
        out_state.right = false;
    } else {
        out_state.left = false;
        out_state.right = false;
    }

    if (dy > config_.active_zone) {
        out_state.up = true;
        out_state.down = false;
    } else if (dy < -config_.active_zone) {
        out_state.down = true;
        out_state.up = false;
    } else {
        out_state.up = false;
        out_state.down = false;
    }
}

void GameInput::applyDeadzone(int16_t raw_x, int16_t raw_y, int16_t* out_x, int16_t* out_y) {
    *out_x = raw_x;
    *out_y = raw_y;

    if (raw_x > -config_.deadzone && raw_x < config_.deadzone) {
        *out_x = 0;
    }
    if (raw_y > -config_.deadzone && raw_y < config_.deadzone) {
        *out_y = 0;
    }
}

bool GameInput::isButtonPressed(ControllerButton button) const {
    return (state_.buttons & static_cast<uint32_t>(button)) != 0;
}

bool GameInput::isButtonHeld(ControllerButton button) const {
    return (state_.buttons & static_cast<uint32_t>(button)) != 0;
}

void GameInput::setN64Input(uint32_t buttons, int16_t stick_x, int16_t stick_y,
                                      int16_t cstick_x, int16_t cstick_y) {
    state_.buttons = buttons;
    state_.control_stick.x = stick_x;
    state_.control_stick.y = stick_y;
    state_.c_stick_raw.x = cstick_x;
    state_.c_stick_raw.y = cstick_y;
    stickToDPad(stick_x, stick_y, state_.dpad);
    stickToDPad(cstick_x, cstick_y, state_.c_buttons);
}
