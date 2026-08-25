#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSAEMSPLAYERVOICE_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSAEMSPLAYERVOICE_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"          // Name (real home)
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"        // Registry / Entity (real homes)
#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"  // VoiceSpec / VoiceSchema / FeatureSchema (real homes)
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"     // Environment (real home)
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"         // Factory (real home)
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsDataStructures.h" // AemsVoiceCsisClass (real home)

#include <cstddef> // size_t

namespace rw
{
    struct ResourceDescriptor;
    struct Resource;
    class  IResourceAllocator;
}

// ============================================================================
// CgsAemsPlayerVoice.h  (MINIMAL home for the AEMS player-voice allocation TUs).
//
// The reconstructed functions:
//   AemsPlayerVoice::GetClientAllocationSize(Factory&, const VoiceSpec&) @ 0x826A2B58
//   AemsPlayerVoice::operator new(size_t, Factory&, const VoiceSpec&)    @ 0x826C2270
//   AemsPlayerVoice::~AemsPlayerVoice()  (scalar-deleting dtor)          @ 0x826DAA50
//   AemsPlayerVoice::Stop                                                @ 0x826DAF10
//
// (2026-08-25, audio-faithfulness wave 6): the 7 local collaborator DEFER slices
// (Name / AemsVoiceCsisClass / Registry / FeatureSchema / VoiceSchema / VoiceSpec /
// Environment) are FOLDED onto their real homes, included above -- the asm calls
// the real symbols (`bl VoiceSpec::GetVoiceSchema`, `bl VoiceSchema::
// GetFeatureSchema`, `bl Registry::GetEntity<AemsVoiceCsisClass>`), the +0xC spec
// byte is the real mu8SendCount (GetTailUnitCount -- the rival's separate
// mu8TailUnitCount member was an invention), and the CSIS `lhz +0xC` is the real
// AemsVoiceCsisClass::mu16UserParameterStart (NOT a parameter count).
//
// STILL keystone-blocked (FLAG): the concrete AemsFactory does not derive Factory
// yet and its registry member is untyped, so the AEMS-registry recovery stays the
// free-accessor shim below (the X360 walk: Factory* -4 == the AemsFactory MI base
// adjust, then +0x60 == mpRegistry); the Voice/GenericRwacVoice base chain of
// AemsPlayerVoice itself stays un-modeled.
// ============================================================================

namespace CgsSound
{
namespace Playback
{

// FLAG (DEFER, the AEMS keystone): the concrete AemsFactory's registry, recovered
// off the generic Factory pointer (X360: factory - 4 -> +0x60). SAME entity as the
// declaration in Module/CgsSoundPlaybackModule.h; bodied in the AemsFactory TU
// when the base chain lands.
Registry* GetAemsFactoryRegistry(Factory* lpAemsFactory);

// The CSIS command ring the Stop slice posts into (real struct home:
// CgsCommandQueue.h:107; fwd-declared so this header stays out of the
// CsisCommandQueue dual-home tangle its banners document).
struct CsisCommandQueue;

// ---------------------------------------------------------------------------
// CgsAemsPlayerVoice.h. The AEMS player voice (allocation surface + the Stop
// slice). 2026-08-25 wave 5: this is now the SINGLE AemsPlayerVoice definition --
// CgsCommandQueue.cpp's rival TU-local class (2 data members, no virtual; a hard
// ODR conflict with this one) is retired and its Stop @0x826DAF10 merged here.
// The Voice/GenericRwacVoice base chain stays the DEFERRED keystone; the two
// Stop-slice members below sit at the head of the un-modeled span per the
// by-name rule (console offsets in comments only).
// ---------------------------------------------------------------------------
struct AemsPlayerVoice
{
    // @ 0x826A2B58 (static). Client block size = 4*(featureParamSchemaCount +
    // csisParamCount + 42).
    static size_t GetClientAllocationSize(Factory& arFactory, const VoiceSpec& arVoiceSpec);

    // @ 0x826C2270 (placement new). Allocate the client block (fixed part +
    // per-parameter/-slot tail) through the Factory's Environment RenderWare allocator.
    void* operator new(size_t auSize, Factory& arFactory, const VoiceSpec& arVoiceSpec);

    // @ 0x826DAA50. Empty out-of-line class dtor (base dtors run implicitly).
    virtual ~AemsPlayerVoice();

    // @ 0x826DAF10. If no CSIS request is outstanding report false; else post a
    // release command for it, clear the handle, report true. Bodied in
    // CgsCommandQueue.cpp (where the queue/command types are complete).
    bool Stop();

    // Console +0x98: the live CSIS request handle (0 == none). The intervening
    // Voice/GenericRwacVoice base span is un-modeled (keystone-DEFERRED).
    uintptr_t mhRequestHandle;

    // FLAG (by-name ROUTE stand-in): the X360 Stop reaches the queue via
    // `*(this+8)` (Voice::mFactory) -4 (the MI base adjust) +0x68 == the owning
    // AemsFactory's embedded CsisCommandQueue. That walk is un-wirable until the
    // Voice base chain + the AemsFactory queue member land, so the queue is held
    // as a direct pointer; the real route replaces it with mFactory-derived
    // access when the keystone lands.
    CsisCommandQueue* mpCommandQueue;
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_AEMS_CGSAEMSPLAYERVOICE_H
