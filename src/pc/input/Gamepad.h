#pragma once

#include <cstdint>
#include "pc/input/AdaptiveTriggers.h"

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

    // FLAG PC-platform leaf: copied feedback mailbox, separate from the device
    // lock. The world publishes values; only the input path sends SDL effects.
    void PublishFeedback(const AdaptiveTriggers::Sample& lrSample);
    bool WantsFeedback(); // cheap world-side gate; diagnostics also enables observations
    void SetFeedbackAllowed(bool lbAllowed);
    void SetFeedbackPhotoMode(bool lbPhotoMode); // copied director state; no device access
    void SuspendFeedback(); // immediate neutral output in a blocking assert
    // Also remembers the path: the input thread re-reads [AdaptiveTriggers] when
    // config.ini changes, so tuning applies without restarting the game.
    unsigned LoadFeedbackSettings(const char* lpIniPath);
    void SetFeedbackLogger(void (*lpLogger)(const char*));
    struct FeedbackStatus
    {
        enum Support { NoDevice, Unsupported, Supported, Rejected } meSupport = NoDevice;
        enum Output { NoOutput, DualSense, SteamController } meOutput = NoOutput;
        std::uint32_t muDevice = 0;
        std::uint64_t muPackets = 0, muFailures = 0;
        AdaptiveTriggers::Cue meLeft = AdaptiveTriggers::Cue::Off;
        AdaptiveTriggers::Cue meRight = AdaptiveTriggers::Cue::Off;
        bool mbWireless = false;
    };
    FeedbackStatus GetFeedbackStatus();

    // config.ini [ControllerKeyboard], loaded once at startup. Invalid entries
    // are skipped and counted. Keyboard events/state belong to this game window.
    unsigned LoadKeyboardMappings(const char* lpIniPath);
    void SetKeyboardWindow(void* lpWindow);
    void UpdateKeyboardMappings(); // also used by the blocking assert loop
    bool MappedKeyDown(int liVirtualKey);
}
