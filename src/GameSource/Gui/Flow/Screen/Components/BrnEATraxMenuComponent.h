#pragma once

// ===================================================================================
// BrnGui::EATraxMenuComponent  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnEATraxMenuComponent.h
//
// The EATrax (in-game music) menu component. It tracks a per-track "mode" (one of four
// values) for up to KI_MAX_TRAX_IN_GAME (128) tracks, encoded as two bits across two
// parallel 128-bit bit sets (CgsContainers::FastBitArray<128>).
//
// Layout/behaviour proven from BURNOUT_X360_ARTIST.XEX:
//   * GetTrackMode @0x82426A60 / SetTrackMode @0x82426DA0 assert the track index against
//     KI_MAX_TRAX_IN_GAME (BrnEATraxMenuComponent.h:281) and against the bit-set capacity
//     128 (CgsFastBitArray.h:396/431/452), reading/writing 64-bit fields with
//     `8*(idx>>6)` field indexing and a `1 << (idx & 63)` mask -- exactly FastBitArray<128>.
//   * Two bit sets: the "low" set at this+0xA0 (160) and the "high" set at this+0xB0 (176).
//     Each FastBitArray<128> is 2 x u64 == 16 bytes, so the low set (0xA0..0xAF) is
//     immediately followed by the high set (0xB0..0xBF).
//   * The 2-bit mode is read/written INVERTED: a SET high bit contributes 0 (else 2), a SET
//     low bit contributes 0 (else 1):
//        mode 0 -> high=set,   low=set
//        mode 1 -> high=set,   low=clear
//        mode 2 -> high=clear, low=set
//        mode 3 -> high=clear, low=clear
//     and GetTrackMode returns (high ? 0 : 2) + (low ? 0 : 1).
//
// Only the two bit sets are accessed by these methods; the GUI-component head and the
// fields preceding the bit sets are not reconstructed by this TU and are modelled as a
// leading reserved byte-span at their proven offset. All access is by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"   // FastBitArray<128>
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT (track-index bound)

namespace BrnGui
{
    class EATraxMenuComponent
    {
    public:
        // BrnEATraxMenuComponent.h:281 - "(liTrackIndex >= 0) && (liTrackIndex < KI_MAX_TRAX_IN_GAME)".
        static const s32 KI_MAX_TRAX_IN_GAME = 128;

        // Per-track playback/availability mode. The numeric values are the X360 GetTrackMode
        // return values (0..3); SetTrackMode's `default:` arm fires "Invalid mode".
        enum E_TrackMode
        {
            E_TRACKMODE_0 = 0,
            E_TRACKMODE_1 = 1,
            E_TRACKMODE_2 = 2,
            E_TRACKMODE_3 = 3,
        };

        // @0x82426A60 - return the 2-bit mode for track liTrackIndex (decoded inverted from
        // the high/low bit sets, see header banner).
        s32 GetTrackMode(s32 liTrackIndex) const;

        // @0x82426DA0 - set the 2-bit mode for track liTrackIndex (encoded inverted into the
        // high/low bit sets). An out-of-range leMode fires "Invalid mode".
        EATraxMenuComponent& SetTrackMode(s32 liTrackIndex, s32 leMode);

    private:
        // GUI-component head + the fields preceding the two bit sets. Not reconstructed by
        // this TU; modelled as a reserved span so the bit sets land at their proven offsets
        // (+0xA0 / +0xB0). Access to anything in here is never made by name in this TU.
        u8 maComponentHeadReserved[0xA0];                  // +0x00 .. +0x9F

        CgsContainers::FastBitArray<128> maTrackModeLowBits;   // +0xA0 (low mode bit)
        CgsContainers::FastBitArray<128> maTrackModeHighBits;  // +0xB0 (high mode bit)
    };
}
