#pragma once

#include "pc/input/AdaptiveTriggers.h"
#include <array>
#include <cmath>

// FLAG PC-platform leaf: Steam Controller (2nd generation, "Triton") haptic output
// reports passed through SDL_SendGamepadEffect. Requires the vendor/SDL fork's
// Triton SendJoystickEffect (libsdl-org/SDL#16398 backport), which forwards these
// fixed-size HID output reports unchanged. Layouts follow SDL's
// src/joystick/hidapi/steam/controller_structs.h (packed, little-endian).
//
// The controller has no trigger resistance. Each physical trigger's resolved
// AdaptiveTriggers effect is presented on the same side's haptic actuator:
// vibration modes become a carrier tone amplitude-modulated at the effect's
// frequency; a smooth load (Feedback mode) can only be marked by a short tick
// when it engages.
//
// The reports' first payload byte selects an actuator, not just a side. Ids
// from Pixel1011's firmware notes (github.com/Pixel1011/SteamHapticsPlayer,
// rangeSrc/range.cpp): 0 left trackpad, 1 right trackpad, 3 both grip motors,
// 4 right grip motor, 6 both trackpads; 2 is inferred as the left grip motor.
// 5 drives everything and restarts the controller, so it is never sent.
namespace CgsInput::AdaptiveTriggers::SteamHaptics
{
    enum : std::uint8_t
    {
        E_REPORT_COMMAND = 0x82,  // ID_OUT_REPORT_HAPTIC_COMMAND
        E_REPORT_LFO_TONE = 0x83, // ID_OUT_REPORT_HAPTIC_LFO_TONE
    };
    enum : std::uint8_t
    {
        E_ACTUATOR_LEFT_TRACKPAD = 0, E_ACTUATOR_RIGHT_TRACKPAD = 1, E_ACTUATOR_LEFT_GRIP = 2,
        E_ACTUATOR_BOTH_GRIPS = 3, E_ACTUATOR_RIGHT_GRIP = 4, E_ACTUATOR_BOTH_TRACKPADS = 6
    };
    enum : std::uint8_t { E_COMMAND_OFF = 0, E_COMMAND_TICK = 1 }; // haptic_type_t
    constexpr unsigned KU_COMMAND_BYTES = 4;   // HID_HAPTIC_COMMAND_REPORT_BYTES
    constexpr unsigned KU_TONE_BYTES = 10;     // HID_HAPTIC_LFO_TONE_REPORT_BYTES
    // Tones carry their own expiry so a stalled game cannot leave one playing;
    // the sender renews an unchanged tone well before it runs out.
    constexpr std::uint16_t KU_TONE_MS = 200;
    constexpr std::uint64_t KU_RENEW_MS = 100;
    constexpr int KI_MIN_GAIN_DB = -30;        // quieter levels are released

    struct Report
    {
        std::array<std::uint8_t, KU_TONE_BYTES> mau{};
        std::uint8_t muSize = 0; // 0 = nothing to send

        bool operator==(const Report& lrOther) const
        {
            return muSize == lrOther.muSize && mau == lrOther.mau;
        }
        bool operator!=(const Report& lrOther) const { return !(*this == lrOther); }
        bool IsTone() const { return muSize == KU_TONE_BYTES; }
    };

    // Strongest zone, 0..8: the actuator has one amplitude, not ten travel zones.
    inline unsigned Level(const Effect& lrEffect)
    {
        unsigned luLevel = 0;
        for (const auto luStrength : lrEffect.mauStrength) luLevel = std::max<unsigned>(luLevel, luStrength);
        return std::min(luLevel, 8u);
    }

    // Full strength maps to the configured gain; each halving of the level is -6 dB.
    inline int GainDb(unsigned luLevel, const Settings& lrSettings)
    {
        if (!luLevel) return KI_MIN_GAIN_DB - 1;
        const float lfDb = static_cast<float>(lrSettings.miSteamGainDb) + 20.0f * std::log10(luLevel / 8.0f);
        return static_cast<int>(std::lround(lfDb));
    }

    inline Report Command(std::uint8_t luSide, std::uint8_t luCommand, int liGainDb = 0)
    {
        Report lReport;
        lReport.muSize = KU_COMMAND_BYTES;
        lReport.mau[0] = E_REPORT_COMMAND;
        lReport.mau[1] = luSide;
        lReport.mau[2] = luCommand;
        lReport.mau[3] = static_cast<std::uint8_t>(static_cast<std::int8_t>(std::clamp(liGainDb, -128, 127)));
        return lReport;
    }

    inline Report Tone(std::uint8_t luSide, int liGainDb, unsigned luToneHz, unsigned luLfoHz)
    {
        Report lReport;
        lReport.muSize = KU_TONE_BYTES;
        lReport.mau[0] = E_REPORT_LFO_TONE;
        lReport.mau[1] = luSide;
        lReport.mau[2] = static_cast<std::uint8_t>(static_cast<std::int8_t>(std::clamp(liGainDb, -128, 127)));
        lReport.mau[3] = static_cast<std::uint8_t>(luToneHz);
        lReport.mau[4] = static_cast<std::uint8_t>(luToneHz >> 8);
        lReport.mau[5] = static_cast<std::uint8_t>(KU_TONE_MS);
        lReport.mau[6] = static_cast<std::uint8_t>(KU_TONE_MS >> 8);
        lReport.mau[7] = static_cast<std::uint8_t>(luLfoHz);
        lReport.mau[8] = static_cast<std::uint8_t>(luLfoHz >> 8);
        lReport.mau[9] = 100; // lfo_depth, percent: the effect frequency is the felt texture
        return lReport;
    }

    inline bool IsTone(const Effect& lrEffect, const Settings& lrSettings)
    {
        return lrEffect.meMode == Mode::Vibration && lrEffect.muFrequency &&
            GainDb(Level(lrEffect), lrSettings) >= KI_MIN_GAIN_DB;
    }

    // The reports that present lrEffect after lrPrevious on one side, in send
    // order. Leaving a tone stops the actuator explicitly instead of waiting for
    // expiry; a Feedback load ticks once when it engages, then stays silent.
    inline std::array<Report, 2> Translate(const Effect& lrEffect, const Effect& lrPrevious,
                                           std::uint8_t luSide, const Settings& lrSettings)
    {
        std::array<Report, 2> laReports{};
        const int liGain = GainDb(Level(lrEffect), lrSettings);
        if (IsTone(lrEffect, lrSettings))
        {
            laReports[0] = Tone(luSide, liGain, static_cast<unsigned>(lrSettings.miSteamToneHz), lrEffect.muFrequency);
            return laReports;
        }
        unsigned luCount = 0;
        if (IsTone(lrPrevious, lrSettings)) laReports[luCount++] = Command(luSide, E_COMMAND_OFF);
        if (lrEffect.meMode == Mode::Feedback && lrSettings.mbSteamResistanceTicks && liGain >= KI_MIN_GAIN_DB &&
            (lrPrevious.meMode != Mode::Feedback || lrPrevious.meCue != lrEffect.meCue))
            laReports[luCount++] = Command(luSide, E_COMMAND_TICK, liGain);
        return laReports;
    }
}
