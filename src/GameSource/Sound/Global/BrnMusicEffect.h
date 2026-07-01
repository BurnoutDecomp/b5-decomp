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

// ADDITIVE GROW (Wave 5: JunkyardInAirEffect::meJunkyardAmbience). DWARF
// BrnMusicEffect.h:24 -- the junkyard-ambience selector enum. Zero-risk additive
// (a new enum in the existing MusicEffect namespace; no layout/member change).
enum EJunkyardAmbience
{
    E_JUNKYARD_AMBIENCE_NONE        = 0,
    E_JUNKYARD_AMBIENCE_NEW_PROFILE = 1,
};

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

// =============================================================================
// BrnSound::Logic::MusicStream  (additively added; shares this BrnMusicEffect.h
//   DWARF/assert home with MusicEffect::EaTraxData. Bodied in BrnMusicStream.cpp.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// MusicStream is the streaming-music playback object MusicEffect drives. It tracks
// whether the next song is queued (mbSongQueued) and exposes a sub-object that
// lives at +0x0C (returned by-address from GetCre*).
//
// This class bodies TWO ledger functions (both touch this struct by name):
//   GetCre         @ 0x82687368   (returns &member @ +0x0C)
//   SetSongQueued  @ 0x82683208   (BrnMusicEffect.h:1080 assert site)
//
// LAYOUT (recovered from the asm; offsets are this struct's field offsets, all
// reached BY NAME in the .cpp):
//   addi r3, r3, 0xC          -> GetCre returns the address of the member @ +0x0C
//   lbz/stb 0x5A              -> mbSongQueued (u8 bool)
//
// FLAG (un-DWARF'd member names/types): no DecFIGS DWARF hint exists, so the
// members are modelled as HONEST named fields at the asm-observed offsets/widths.
// Only the +0x0C address-of and the +0x5A byte read/write are X360 facts; the
// sub-object's element TYPE at +0x0C is UNVERIFIED and modelled as an opaque
// byte-sized-and-aligned reserved region (GetCre only takes its address). The gaps
// are other MusicStream state not touched by these two functions.
// =============================================================================
namespace MusicStream
{

// The sub-object MusicStream::GetCre* returns the address of, at +0x0C. Its real
// type is UNVERIFIED (the X360 only does `addi r3, r3, 0xC` -- take-address); it is
// modelled as an opaque region whose size/alignment are unknown, so the only X360
// fact preserved is the +0x0C offset of its first byte. Declared as a 1-byte
// honest placeholder element.
struct Cre;   // opaque; only its address @ +0x0C is taken (size/layout DEFERRED)

// BrnMusicEffect.h (DWARF home). Streaming-music playback object.
struct Stream
{
    // BrnMusicEffect.h (region). Return the address of the sub-object @ +0x0C.
    // @ 0x82687368.
    Cre* GetCre();

    // BrnMusicEffect.h:1080 (assert site). Set the "next song queued" flag. Asserts
    // the new value actually differs from the current (lbSongQueued != mbSongQueued).
    // @ 0x82683208.
    void SetSongQueued( bool lbSongQueued );

    // -- FLAGGED layout (offsets/widths are X360 facts; field names descriptive) --

    u8  maReserved0x00[0x0C];   // @0x00..0x0B: state untouched by these fns

    u8  maCre[1];               // @0x0C: GetCre returns &maCre[0]; element type DEFERRED

    u8  maReserved0x0D[0x4D];   // @0x0D..0x59: state untouched by these fns

    u8  mbSongQueued;           // @0x5A: "next song queued" flag (lbz/stb)
};

} // namespace MusicStream
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_MUSIC_EFFECT_EATRAX_DATA_H
