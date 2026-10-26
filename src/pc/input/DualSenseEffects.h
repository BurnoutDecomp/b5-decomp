#pragma once

#include "pc/input/AdaptiveTriggers.h"
#include <array>
#include <cstring>

// FLAG PC-platform leaf: SDL3 PS5 effect payload (not a USB/Bluetooth HID report).
// SDL owns report framing, CRCs and transport. Trigger writes also preserve the
// active ordinary-rumble mode/amplitudes using the pinned SDL PS5 driver's policy.
// Packet offsets: libsdl-org/SDL release-3.4.18 test/testcontroller.c DS5EffectsState_t.
// Zone encoding adapted from John "Nielk1" Klein's MIT-licensed trigger factories:
// https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db
// See docs/licenses/DualSenseTriggerEffects.txt for the retained license notice.
namespace CgsInput::AdaptiveTriggers
{
    using TriggerBytes = std::array<std::uint8_t, 11>;
    using Packet = std::array<std::uint8_t, 47>;

    struct RumbleState
    {
        std::uint16_t muLeft = 0, muRight = 0;
        bool mbEnhanced = true;
    };

    inline TriggerBytes Encode(const Effect& lrEffect)
    {
        TriggerBytes lBytes{};
        lBytes[0] = static_cast<std::uint8_t>(Mode::Off);
        if (lrEffect.meMode != Mode::Feedback && lrEffect.meMode != Mode::Vibration) return lBytes;
        if (lrEffect.meMode == Mode::Vibration && !lrEffect.muFrequency) return lBytes;
        std::uint16_t luZones = 0;
        std::uint32_t luStrengths = 0;
        for (unsigned luZone = 0; luZone < 10; ++luZone)
        {
            const unsigned luStrength = std::min<unsigned>(lrEffect.mauStrength[luZone], 8);
            if (!luStrength) continue;
            luZones |= static_cast<std::uint16_t>(1u << luZone);
            luStrengths |= (luStrength - 1u) << (3u * luZone);
        }
        if (!luZones) return lBytes;
        lBytes[0] = static_cast<std::uint8_t>(lrEffect.meMode);
        lBytes[1] = static_cast<std::uint8_t>(luZones);
        lBytes[2] = static_cast<std::uint8_t>(luZones >> 8);
        for (unsigned i = 0; i < 4; ++i) lBytes[3 + i] = static_cast<std::uint8_t>(luStrengths >> (8 * i));
        if (lrEffect.meMode == Mode::Vibration) lBytes[9] = lrEffect.muFrequency;
        return lBytes;
    }

    inline Packet Encode(const Output& lrOutput, const RumbleState& lrRumble = {})
    {
        Packet lPacket{};
        lPacket[0] = 0x04 | 0x08; // right / left trigger
        const auto lRight = Encode(lrOutput.mRight);
        const auto lLeft = Encode(lrOutput.mLeft);
        std::memcpy(lPacket.data() + 10, lRight.data(), lRight.size());
        std::memcpy(lPacket.data() + 21, lLeft.data(), lLeft.size());
        const auto luLeft = static_cast<std::uint8_t>(lrRumble.muLeft >> 8);
        const auto luRight = static_cast<std::uint8_t>(lrRumble.muRight >> 8);
        if (luLeft || luRight)
        {
            lPacket[0] |= 0x02; // Keep emulated rumble selected instead of audio haptics.
            if (lrRumble.mbEnhanced)
            {
                lPacket[38] = 0x04; // Improved rumble: firmware >= 0x0224, Edge or unknown.
                lPacket[2] = luRight;
                lPacket[3] = luLeft;
            }
            else
            {
                lPacket[0] |= 0x01;
                lPacket[2] = luRight >> 1;
                lPacket[3] = luLeft >> 1;
            }
        }
        return lPacket;
    }
}
