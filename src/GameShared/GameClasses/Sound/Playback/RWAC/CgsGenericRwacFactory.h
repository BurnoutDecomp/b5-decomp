#ifndef CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACFACTORY_H
#define CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACFACTORY_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"   // Factory base + Environment
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"  // Registry + RegistrySpec (the in-place carve)
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"    // Handle<GenericRwacFactory>

// ============================================================================
// CgsGenericRwacFactory.h  (home for RwacLock + GenericRwacFactory).
//
// Reconstructed functions homed here (AEMS-cascade wave 2026-08-28; the full
// register-level decode is progress/scratch_dossiers/aems_factory_cascade_codex.md):
//   CgsSound::Playback::RwacLock::RwacLock(System*)              @ 0x826810F8
//   GenericRwacFactory::Create(Environment&, spec)               @ 0x826C7AD0
//   GenericRwacFactory::GenericRwacFactory(Environment&, spec)   @ 0x826C17A0
//
// RwacLock is a stack RAII guard holding the RenderWare audio-core System locked for
// the lifetime of the guard (~RwacLock unlocks).
//
// The audio-core System has an authoritative vendor home (rw/audio/core/PlugIn.h);
// it is NOT redefined here. The System lock is taken through the engine entry
// RwacSystemLock (semantically rw::audio::core::System::Lock), and the process-wide
// default System is fetched through GetDefaultRwacSystem (X360 off_83271928). Both are
// declared BY NAME and resolved in their own TUs.
// ============================================================================

namespace rw
{
namespace audio
{
namespace core
{
    class System;   // vendor home rw/audio/core/PlugIn.h (forward-declared here).

    // Engine entry: take the audio-core System lock (X360 System::Lock). Declared-only.
    void RwacSystemLock(System* apSystem);
}
}
}

namespace CgsSound
{
namespace Playback
{

// The process-wide default RWAC System (X360 off_83271928). Declared-only; bodied in
// the RWAC System singleton TU.
rw::audio::core::System* GetDefaultRwacSystem();

// CgsGenericRwacFactory.h:49 (DWARF). Scoped RAII lock on the audio-core System.
class RwacLock
{
public:
    // @ 0x826810F8. Store the System (falling back to the default when null), assert
    // one is available, and take its lock.
    RwacLock(rw::audio::core::System* apSystem);
    ~RwacLock();

private:
    rw::audio::core::System* mpSystem;   // +0x00  CgsGenericRwacFactory.h:72 (DWARF)
};

// The sizing spec Create/ctor consume (console spec words +0 mpSystem / +4
// entityCount / +8 dataBytes / +0xC stringBytes -- the by-value r5:r6 packing at
// the @0x826E92B8 Prepare build site; null mpSystem resolves to off_83271928).
struct GenericRwacFactorySpec
{
    rw::audio::core::System* mpSystem;            // +0x00
    u32                      mu32EntityCount;     // +0x04 -> RegistrySpec.mu32EntityCount
    u32                      mu32DataSize;        // +0x08 -> RegistrySpec.muDataSize
    u32                      mu32StringTableSize; // +0x0C -> RegistrySpec.muStringTableSize
};

// ---------------------------------------------------------------------------
// GenericRwacFactory -- the RWAC (RenderWare Audio Core) playback factory: the
// bridge between the playback Environment's factory table and the vendor
// rw::audio::core engine. Constructed into a single carve from the environment's
// allocator: [fixed head][in-place Registry header][entity slots][data][strings]
// (console fixed head 0x4020 + registry header 0x1C == the create's 0x100F-word
// term; host uses its own sizeofs, the Environment::operator-new precedent).
//
// LAYOUT (console offsets documentary; host members by NAME + SEQUENCE):
//   Factory base                      @ +0x00 (vtable/refcount/name/environment)
//   mpSystem                          @ +0x10
//   mau8CommandQueueStorage[0x4000]   @ +0x14  (the RwacCommandQueue ring payload;
//                                     the ctor does NOT touch it -- only the two
//                                     control words below are zeroed)
//   mu32CommandQueueWriteCursor       @ +0x4014 (:= 0)
//   mu32CommandQueueReadCursor        @ +0x4018 (:= 0)
//   mpRegistry                        @ +0x401C (-> the in-place Registry @ +0x4020)
// ---------------------------------------------------------------------------
class GenericRwacFactory : public Factory
{
public:
    // @ 0x826C7AD0. Resolve the spec's system (off_83271928 fallback + the
    // "lSpec.mpSystem" assert), size the carve (console 4*(entities+0x100F)+data+
    // strings; host sizeofs), allocate through the ENVIRONMENT's allocator with
    // the five-pair descriptor tagged "GenericRwacFactory", construct, and return
    // the handle with one explicit Acquire (the interim plain-store Handle model:
    // the create's ref becomes the stored handle's owned ref).
    static Handle<GenericRwacFactory> Create(Environment& arEnvironment,
                                             GenericRwacFactorySpec aSpec);

    // @ 0x826C17A0. Factory base (Name = the "~GenericRwacFactory::SK_NAME~"
    // intern == console dword_83008650), the member stores above, the in-place
    // Registry, then -- under the RwacLock guard -- the full plug-in/decoder
    // registration pass (the 25 RegisterPlugInRunTime calls + the standard
    // decoder set; see the .cpp).
    GenericRwacFactory(Environment& arEnvironment, const GenericRwacFactorySpec& akrSpec);

    // The nested registry (console +0x401C), by name -- the GetRwacFactoryRegistry
    // accessor (CgsSoundPlaybackModule.h:99) reads it off the generic Factory*.
    Registry* GetRegistry() { return mpRegistry; }

private:
    rw::audio::core::System* mpSystem;                    // console +0x10
    u8   mau8CommandQueueStorage[0x4000];                 // console +0x14 (ring payload)
    u32  mu32CommandQueueWriteCursor;                     // console +0x4014
    u32  mu32CommandQueueReadCursor;                      // console +0x4018
    Registry* mpRegistry;                                 // console +0x401C
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACFACTORY_H
