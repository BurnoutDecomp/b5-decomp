#ifndef BRN_SOUND_LOGIC_MUSIC_EFFECT_EATRAX_DATA_H
#define BRN_SOUND_LOGIC_MUSIC_EFFECT_EATRAX_DATA_H

#include "types.hpp"

// =============================================================================
// BrnSound::Logic::MusicEffect::EaTraxData
//   GameSource/Sound/Global/BrnMusicEffect.h (DWARF home) +
//   GameSource/Sound/Global/BrnMusicEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// EaTraxData is the per-MusicEffect block that drives EA's interactive-music
// ("EaTrax") streaming layers. It lives as the `mEaTraxData` member of
// BrnSound::Logic::MusicEffect at offset +460; MusicEffect::Attach @ 0x8269CC60
// calls `mEaTraxData.Prepare( GetBrnLogicModule() )` (asserting the result), so
// Prepare wires the block to the owning sound logic module and resets all of its
// per-attach streaming state.
//
// This TU bodies ONE ledger function:
//   BrnSound::Logic::MusicEffect::EaTraxData::Prepare  @ 0x82697538
//
// LAYOUT (recovered store-for-store from the Prepare asm; offsets are this
// struct's field offsets, all reached BY NAME in the .cpp):
//   std r11(=0), 0x00 / 0x08 / 0x10 / 0x18   -> four 8-byte zero stores
//   stw r10(=-1), 0x30 / 0x34 / 0x38         -> three s32 = -1
//   stw r4(=lpLogicModule), 0x3C             -> mpLogicModule
//   stw r11(=0), 0x40                         -> s32 = 0
//   stb r11(=0), 0x44                         -> u8 = 0
// (The Hex-Rays `*a1 = 0xFFFFFFFF00000000uLL` is the documented HIDWORD-of-qword
// misread of `li r11,0 ; std r11`; the stores write ZERO, confirmed in the asm.)
//
// FLAG (un-DWARF'd member names/types): no DecFIGS DWARF hint exists for
// BrnMusicEffect.h, so the four 8-byte slots at 0x00..0x1F and the trailing
// scalars are modelled as HONEST named fields at the asm-observed offsets and
// widths -- NOT as a guessed concrete handle type. Their exact element types
// (EaTrax stream/stem handles, etc.) are UNVERIFIED and DEFERRED; only the
// widths/offsets/zero-or-(-1) reset values are X360 facts. Sizeof is pinned to
// the asm-observed extent (last store stb @0x44 => >= 0x45 bytes).
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// mpLogicModule is held as an opaque pointer (Prepare only stores it; it never
// dereferences it). Forward declaration keeps this leaf TU's surface minimal.
namespace Module { struct SoundLogicModule; }

namespace MusicEffect
{

// BrnMusicEffect.h (DWARF home). Per-attach EaTrax streaming-layer state block.
struct EaTraxData
{
    // BrnMusicEffect.h:720 (DWARF assert site). Bind the block to the owning sound
    // logic module and reset all per-attach streaming state. Returns true (1) on
    // success. @ 0x82697538.
    bool Prepare( BrnSound::Logic::Module::SoundLogicModule* lpLogicModule );

    // -- FLAGGED layout (offsets/widths are X360 facts; element semantics DEFERRED) --

    // @0x00..0x1F: four 8-byte slots, all zeroed on Prepare. UNVERIFIED element
    // type (modelled as opaque 64-bit handles); only the 8-byte width and the
    // zero reset are X360 facts.
    u64 maEaTraxHandles[4];     // @0x00 / 0x08 / 0x10 / 0x18

    u8  maReserved0x20[16];     // @0x20..0x2F: untouched by Prepare (padding/other state)

    // @0x30/0x34/0x38: three s32 reset to -1 ("none"/invalid index sentinels).
    s32 miTrackIndex0;          // @0x30
    s32 miTrackIndex1;          // @0x34
    s32 miTrackIndex2;          // @0x38 (ends at 0x3C)

    BrnSound::Logic::Module::SoundLogicModule* mpLogicModule; // @0x3C

    s32 miState;                // @0x40: reset to 0 on Prepare
    u8  mbReserved0x44;         // @0x44: reset to 0 on Prepare
};

} // namespace MusicEffect
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_MUSIC_EFFECT_EATRAX_DATA_H
