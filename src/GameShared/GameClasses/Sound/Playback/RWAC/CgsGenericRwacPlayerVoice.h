#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_PLAYER_VOICE_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_PLAYER_VOICE_H

#include "types.hpp"

// =============================================================================
// CgsSound::Playback::GenericRwacPlayerVoice
//   GameShared/GameClasses/Sound/Playback/Rwac/CgsGenericRwacPlayerVoice.h (DWARF
//   home) + GameShared/GameClasses/Sound/Playback/Rwac/CgsGenericRwacPlayerVoice.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity; load widths
// authoritative). GenericRwacPlayerVoice is the RenderWare-audio player-voice wrapper
// in the GenericRwacVoice hierarchy.
//
// FLAG (PARTIAL home — TU NOT complete in this group): the ledger recon set is two
// functions; only the scalar profiling getter is reconstructable here:
//   GenericRwacPlayerVoice::GetCpuTicks    @ 0x826C8040   (homed below)
//   GenericRwacPlayerVoice::DoConnectSend  @ 0x826E14C8   (BLOCKED — see below)
//
// DoConnectSend is a bare tail-call thunk (`addi r3,r3,0x2C; b
// CgsSound__Playback__GenericRwacVoice__ConnectSend`, no prologue) into the base
// GenericRwacVoice::ConnectSend(this+0x2C). The GenericRwacVoice keystone is NOT
// homed in this group, so the thunk has no recoverable, linkable body without
// fabricating the un-homed base — it is BLOCKED and DEFERRED to the GenericRwacVoice
// keystone TU. Therefore the player-voice TU is INCOMPLETE; only the genuine getter
// body is committed here.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): GetCpuTicks reads the sub-voice pointer at
// the absolute byte offset +0x30 and the profiling tick count at sub-voice +0x3C
// (the player wrapper places its sub-voice pointer one slot earlier than the master
// wrapper's +0x34). Those offsets assume 4-byte pointers/vptr; on a 64-bit host
// pointer/vptr widths differ, so members are pinned BY NAME only and the absolute
// offsets are NOT static_asserted across pointers.
// =============================================================================

namespace CgsSound
{
namespace Playback
{

// MINIMAL home (full RenderWare voice in the rw::audio::core keystone). GetCpuTicks
// reads one u32 profiling counter from the underlying RW voice (X360 sub-voice +0x3C).
// FLAG: minimal forward shape; byte offset not asserted on the 64-bit host.
struct RwacPlayerVoiceProfilingSource
{
    u32 mu32CpuTicks; // sub-voice +0x3C — lwz r11, 0x3C(r11)
};

// MINIMAL home (full layout in the GenericRwacVoice / GenericRwacPlayerVoice keystone
// TU). Only the sub-voice pointer GetCpuTicks reads is modelled BY NAME here.
struct GenericRwacPlayerVoice
{
    // @ 0x826C8040 — the per-voice profiling tick count. If the underlying RW voice is
    // null the X360 returns flt_82001CC0 (== 0.0f); otherwise it loads the u32 tick
    // counter and converts it to f32 (fcfid/frsp).
    f32 GetCpuTicks() const;

    // The underlying RenderWare voice this player voice profiles (X360 +0x30).
    // FLAG: byte offset not asserted on the 64-bit host; pinned by name only.
    RwacPlayerVoiceProfilingSource* mpVoice;
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_PLAYER_VOICE_H
