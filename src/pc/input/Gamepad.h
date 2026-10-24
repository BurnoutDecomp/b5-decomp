#pragma once

#include <cstdint>

// FLAG PC-platform leaf: SDL supplies the host device; these records retain the
// XDK wire format consumed by the recovered DeviceX360Pad and ManagerX360.
namespace CgsInput::PCGamepad
{
    struct Controls
    {
        std::uint16_t wButtons;
        std::uint8_t bLeftTrigger;
        std::uint8_t bRightTrigger;
        std::int16_t sThumbLX, sThumbLY, sThumbRX, sThumbRY;
    };

    struct State
    {
        std::uint32_t dwPacketNumber;
        Controls Gamepad;
    };
    static_assert(sizeof(Controls) == 12 && sizeof(State) == 16);

    // Initialize and Shutdown run at hardware bring-up/teardown. Reads and motor
    // sends may run on the input thread; one lock protects the selected handle.
    bool Initialize();
    void Shutdown();
    void RequestProbe();
    bool Read(State& lrState);
    std::uint32_t SelectedId();
    bool HasRumble();
    void SetInputAllowed(bool lbAllowed);
    std::uint32_t SetRumble(std::uint16_t luLeft, std::uint16_t luRight);

    // config.ini [ControllerKeyboard], loaded once at startup. Invalid entries
    // are skipped and counted. Keyboard events/state belong to this game window.
    unsigned LoadKeyboardMappings(const char* lpIniPath);
    void SetKeyboardWindow(void* lpWindow);
    void UpdateKeyboardMappings(); // also used by the blocking assert loop
    bool MappedKeyDown(int liVirtualKey);
}
