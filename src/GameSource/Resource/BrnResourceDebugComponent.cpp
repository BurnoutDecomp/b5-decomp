#include "GameSource/Resource/BrnResourceDebugComponent.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::DebugComponent::InterpolateColours @ 0x82661410
//
// Per-channel linear interpolation between two packed 32-bit colours in 0xAARRGGBB byte
// order, used by the GameData debug component's RenderHUD (caller @0x82661410's xref) to
// fade the resource-usage bar colours by a [0,1] factor.
//
// The X360 body unpacks each colour into its four 8-bit channels (alpha = byte>>24,
// red = (byte>>16)&0xFF, green = (byte>>8)&0xFF, blue = byte&0xFF), promotes each to
// double, computes `chT * factor + ch0 * (1 - factor)` per channel, rounds to nearest by
// adding 0.5 and truncating toward zero (fctidz), then repacks the four rounded channel
// bytes back into 0xAARRGGBB. The two shared rodata floats it uses are flt_82001C98 == 1.0f
// (the `1 - factor` complement) and flt_82001DA0 == 0.5f (the round-to-nearest bias).
//
// Endpoint mapping (from the asm: the `* f1` term is the factor-weighted operand and the
// `fmadds ..., f0, ...` term is the (1 - factor)-weighted operand): the FIRST colour arg
// (r5) is weighted by `factor`, the SECOND (r6) by `1 - factor`. So factor == 1 yields
// luColourT, factor == 0 yields luColour0.

namespace BrnResource
{
    namespace
    {
        // Round a non-negative interpolated channel to the nearest integer the way the X360
        // does: add 0.5 then truncate toward zero (fctidz), and keep it in [0,255].
        u8 RoundChannel(f64 lfChannel)
        {
            const s32 liRounded = static_cast<s32>(lfChannel + 0.5);
            return static_cast<u8>(liRounded);
        }

        // Linearly interpolate one 8-bit channel: chT * factor + ch0 * (1 - factor).
        u8 LerpChannel(u8 lu8ChannelT, u8 lu8Channel0, f32 lfFactor)
        {
            const f64 lfFactorT = static_cast<f64>(lfFactor);
            const f64 lfFactor0 = 1.0 - lfFactorT;
            const f64 lfResult =
                static_cast<f64>(lu8ChannelT) * lfFactorT +
                static_cast<f64>(lu8Channel0) * lfFactor0;
            return RoundChannel(lfResult);
        }
    }

    uint32_t DebugComponent::InterpolateColours(uint32_t luColourT, uint32_t luColour0, f32 lfFactor)
    {
        const u8 lu8AlphaT = static_cast<u8>(luColourT >> 24);
        const u8 lu8RedT   = static_cast<u8>(luColourT >> 16);
        const u8 lu8GreenT = static_cast<u8>(luColourT >> 8);
        const u8 lu8BlueT  = static_cast<u8>(luColourT);

        const u8 lu8Alpha0 = static_cast<u8>(luColour0 >> 24);
        const u8 lu8Red0   = static_cast<u8>(luColour0 >> 16);
        const u8 lu8Green0 = static_cast<u8>(luColour0 >> 8);
        const u8 lu8Blue0  = static_cast<u8>(luColour0);

        const u8 lu8Alpha = LerpChannel(lu8AlphaT, lu8Alpha0, lfFactor);
        const u8 lu8Red   = LerpChannel(lu8RedT,   lu8Red0,   lfFactor);
        const u8 lu8Green = LerpChannel(lu8GreenT, lu8Green0, lfFactor);
        const u8 lu8Blue  = LerpChannel(lu8BlueT,  lu8Blue0,  lfFactor);

        return (static_cast<uint32_t>(lu8Alpha) << 24) |
               (static_cast<uint32_t>(lu8Red)   << 16) |
               (static_cast<uint32_t>(lu8Green) << 8)  |
                static_cast<uint32_t>(lu8Blue);
    }
}
