#ifndef CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACFACTORY_H
#define CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACFACTORY_H

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// ============================================================================
// CgsGenericRwacFactory.h  (MINIMAL home for the RwacLock scoped guard).
//
// The one reconstructed function homed here so far:
//   CgsSound::Playback::RwacLock::RwacLock(System*)  @ 0x826810F8
//
// RwacLock is a stack RAII guard holding the RenderWare audio-core System locked for
// the lifetime of the guard (~RwacLock unlocks). The full GenericRwacFactory surface
// (SetupConfig, the feature-name statics, GetFeatureImplementation, ...) is a deep
// keystone TU DEFERRED to when its collaborator homes land.
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

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_RWAC_CGSGENERICRWACFACTORY_H
