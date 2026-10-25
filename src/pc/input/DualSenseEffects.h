#pragma once

#include "pc/input/AdaptiveTriggers.h"
#include <array>
#include <cstring>

// FLAG PC-platform leaf: SDL3 PS5 effect payload (not a USB/Bluetooth HID report).
// SDL owns report framing, CRCs and transport. Only trigger enable bits are set.
// Packet offsets: libsdl-org/SDL release-3.4.18 test/testcontroller.c DS5EffectsState_t.
// Zone encoding adapted from John "Nielk1" Klein's MIT-licensed trigger factories:
// https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db
// See docs/licenses/DualSenseTriggerEffects.txt for the retained license notice.
namespace CgsInput::AdaptiveTriggers
{
    using TriggerBytes = std::array<std::uint8_t, 11>;
    using Packet = std::array<std::uint8_t, 47>;

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

    inline Packet Encode(const Output& lrOutput)
    {
        Packet lPacket{};
        lPacket[0] = 0x04 | 0x08; // right / left trigger; no rumble, audio or LED flags
        const auto lRight = Encode(lrOutput.mRight);
        const auto lLeft = Encode(lrOutput.mLeft);
        std::memcpy(lPacket.data() + 10, lRight.data(), lRight.size());
        std::memcpy(lPacket.data() + 21, lLeft.data(), lLeft.size());
        return lPacket;
    }
}
