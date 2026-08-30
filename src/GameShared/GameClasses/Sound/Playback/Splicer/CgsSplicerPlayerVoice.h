#ifndef CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERPLAYERVOICE_H
#define CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERPLAYERVOICE_H

#include "types.hpp"

#include <cstddef> // size_t

#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h" // VoiceSpec (real home, :301)
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"    // Environment (real home, :89)
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"        // Factory (real home, :55)
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"          // PlayerVoice (the primary base)
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoice.h"

struct Splice;

// ============================================================================
// CgsSplicerPlayerVoice.h  (MINIMAL home for the splicer player-voice TUs).
//
// The two reconstructed functions:
//   SplicerPlayerVoice::operator new(size_t, Factory&, const VoiceSpec&) @ 0x826AFC48
//   SplicerPlayerVoice::~SplicerPlayerVoice()  (scalar-deleting dtor)    @ 0x826E1838
//
// SplicerPlayerVoice derives (like every playback voice) from Playback::Voice + a
// GenericRwacVoice subobject; those bases are un-homed keystones. This MINIMAL home
// models only the allocation surface and the two own-member nulls the dtor writes.
// (2026-08-25, audio-faithfulness wave 6: the local Factory / Environment /
// VoiceSpec DEFER slices are FOLDED onto their real homes, included above -- the
// real surfaces carry exactly the methods these TUs call BY NAME.) Host-width
// FLAG: pointer members widen; pinned BY NAME only.
// ============================================================================

namespace CgsSound
{
namespace Playback
{

// ---------------------------------------------------------------------------
// CgsSplicerPlayerVoice.h. The splicer player voice (allocation + dtor surface).
//
// BASE CHAIN (DWARF CgsSplicerPlayerVoice.h:56 prints `: public GenericRwacVoice`,
// but the dumper renders only ONE base -- zero multi-base renderings exist in the
// whole DecFIGS dump corpus -- and the dtor asm @0x826E1838 destroys BOTH
// Voice::~Voice(this+0) and GenericRwacVoice::~GenericRwacVoice(this+0x2C), with
// the own members at +0x84/+0x88 exactly past Voice(0x2C) + GenericRwacVoice(0x58):
// the real shape is MI `: public PlayerVoice, public GenericRwacVoice`). The
// PlayerVoice primary base is real (CgsVoice.h); the GenericRwacVoice base is the
// ledgered un-homed keystone, modelled as an opaque span at its console extent.
// ---------------------------------------------------------------------------
struct SplicerPlayerVoice : public PlayerVoice, public GenericRwacVoice
{
    // @ 0x826AFC48 (placement new). ARTIST size = 20*(slots + inputParams) +
    // 12*(sends + outputParams) + 140; the host uses the corresponding native
    // sizeof values and compiler-supplied client size.
    void* operator new(size_t auSize, Factory& arFactory, const VoiceSpec& arVoiceSpec);

    SplicerPlayerVoice(Factory& arFactory, const VoiceSpec& arVoiceSpec, u32 au32Ident);

    // @ 0x826E1838. Null the two own members, then run the base dtor chain: the
    // implicit ~PlayerVoice/~Voice here matches the console; the console's
    // ~GenericRwacVoice(this+0x2C) is part of the un-homed keystone (see the span
    // below) and is NOT emitted on the host yet.
    virtual ~SplicerPlayerVoice();

    virtual f32 GetCpuTicks();
    virtual EProfileVoiceType GetProfileVoiceType();
    virtual void DoUpdate(System* apSystem, f32 af32DeltaTime);
    virtual bool DoConnectSend(u32 au32Index, SubmixVoice* apSubmix);

    rw::audio::core::PlugIn* mpInternalSubmix; // X360 +0x84, DWARF :177
    ::Splice* mpSplice;       // X360 +0x88 (a1[34]) -- DWARF :178; nulled by the dtor
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERPLAYERVOICE_H
