#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_MASTER_VOICE_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_MASTER_VOICE_H

#include "types.hpp"

// =============================================================================
// CgsSound::Playback::GenericRwacMasterVoice
//   GameShared/GameClasses/Sound/Playback/Rwac/CgsGenericRwacMasterVoice.h (DWARF
//   home) + GameShared/GameClasses/Sound/Playback/Rwac/CgsGenericRwacMasterVoice.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity; load widths
// authoritative). GenericRwacMasterVoice is the RenderWare-audio master-voice wrapper
// in the GenericRwacVoice hierarchy.
//
// FLAG (PARTIAL home — TU NOT complete in this group): the ledger recon set is two
// functions; only the scalar profiling getter is reconstructable here:
//   GenericRwacMasterVoice::GetCpuTicks  @ 0x826D82D8   (homed below)
//   GenericRwacMasterVoice::DoUpdate     @ 0x826D8738   (BLOCKED — see below)
//
// DoUpdate is a bare tail-call thunk (`b ___Update_VGenericRwacMasterVoice ...
// GenericRwacVoice::Update`, no prologue) into the templated base
// GenericRwacVoice::Update<GenericRwacMasterVoice>(this+0x30). That base template and
// the whole GenericRwacVoice keystone are NOT homed in this group, so the thunk has
// no recoverable, linkable body without fabricating the un-homed base — it is BLOCKED
// and DEFERRED to the GenericRwacVoice keystone TU. Therefore the master-voice TU is
// INCOMPLETE; only the genuine getter body is committed here.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): GetCpuTicks reads the sub-voice pointer at
// the absolute byte offset +0x34 and the profiling tick count at sub-voice +0x3C.
// Those offsets assume 4-byte pointers/vptr; on a 64-bit host pointer/vptr widths
// differ, so the members are pinned BY NAME only and the absolute offsets are NOT
// static_asserted across pointers.
// =============================================================================

namespace CgsSound
{
namespace Playback
{

// MINIMAL home (full RenderWare voice in the rw::audio::core keystone). GetCpuTicks
// reads one u32 profiling counter from the underlying RW voice (X360 sub-voice +0x3C).
// FLAG: minimal forward shape; byte offset not asserted on the 64-bit host.
struct RwacMasterVoiceProfilingSource
{
    u32 mu32CpuTicks; // sub-voice +0x3C — lwz r11, 0x3C(r11)
};

// MINIMAL home (full layout in the GenericRwacVoice / GenericRwacMasterVoice keystone
// TU). Only the sub-voice pointer GetCpuTicks reads is modelled BY NAME here.
struct GenericRwacMasterVoice
{
    // @ 0x826D82D8 — the per-voice profiling tick count. If the underlying RW voice is
    // null the X360 returns flt_82001CC0 (== 0.0f); otherwise it loads the u32 tick
    // counter and converts it to f32 (fcfid/frsp).
    f32 GetCpuTicks() const;

    // The underlying RenderWare voice this master voice profiles (X360 +0x34).
    // FLAG: byte offset not asserted on the 64-bit host; pinned by name only.
    RwacMasterVoiceProfilingSource* mpVoice;
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_MASTER_VOICE_H
