#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_gamecontroller.h>
#include <cstdint>
#include <string>

enum class ControllerButton : uint32_t {
    A = 0x0001,
    B = 0x0002,
    START = 0x0800,
    Z = 0x0010,
    L = 0x0040,
    R = 0x0080,
    UP = 0x0100,
    DOWN = 0x0200,
    LEFT = 0x0400,
    RIGHT = 0x0800,
};

struct AnalogState {
    int16_t x;
    int16_t y;
};

struct DPadState {
    bool up;
    bool down;
    bool left;
    bool right;
};

struct ControllerState {
    uint32_t buttons;
    AnalogState control_stick;
    AnalogState c_stick_raw;
    DPadState dpad;
    DPadState c_buttons;
};

struct ControllerConfig {
    int deadzone;
    int active_zone;
    int max_value;
    bool auto_map;
};

class GameInput {
public:
    static GameInput& instance();

    bool initialize();
    void shutdown();

    void poll();
    bool isButtonPressed(ControllerButton button) const;
    bool isButtonHeld(ControllerButton button) const;

    const ControllerState& getState() const { return state_; }
    bool hasController() const { return controller_ != nullptr; }

    void setN64Input(uint32_t buttons, int16_t stick_x, int16_t stick_y,
                         int16_t cstick_x, int16_t cstick_y);
    uint32_t getN64Buttons() const { return state_.buttons; }

    AnalogState getControlStick() const { return state_.control_stick; }
    AnalogState getCStickRaw() const { return state_.c_stick_raw; }
    DPadState getDPad() const { return state_.dpad; }
    DPadState getCButtons() const { return state_.c_buttons; }

private:
    GameInput();

    bool openController();
    void closeController();
    void processControllerAxis(SDL_ControllerAxisEvent& event);
    void processControllerButton(SDL_ControllerButtonEvent& event);
    void stickToDPad(int16_t raw_x, int16_t raw_y, DPadState& out_state);
    void applyDeadzone(int16_t raw_x, int16_t raw_y, int16_t* out_x, int16_t* out_y);
    void clearState();

    SDL_GameController* controller_;
    ControllerState state_;
    ControllerConfig config_;
};

#endif
