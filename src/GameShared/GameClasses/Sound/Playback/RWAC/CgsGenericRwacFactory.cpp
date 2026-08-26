// ============================================================================
// CgsGenericRwacFactory.cpp
//
// CgsSound::Playback::RwacLock::RwacLock(System*) -- the RWAC scoped lock guard's
// constructor. Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826810F8.
//
// RwacLock is a stack RAII guard holding the RenderWare audio-core System locked for
// the lifetime of the guard. The constructor:
//   mpSystem = apSystem;                     // stw r4,0(r31)
//   if (!apSystem)                           // fall back to the default System
//   {
//       mpSystem = GetDefaultRwacSystem();   // lwz off_83271928
//       CGS_ASSERT(mpSystem != 0, "mpSystem"); // CgsGenericRwacFactory.h:59
//   }
//   rw::audio::core::System::Lock(mpSystem); // engine entry RwacSystemLock
//
// Called by GenericRwacFactory::GenericRwacFactory and ::AddRegistry.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/audio/core/PlugIn.h"   // the complete System (Lock/Unlock members; phase B5)

// The process-wide System singleton the vendor System.cpp publishes from
// CreateInstance (extern "C" System *off_83271928).
extern "C" rw::audio::core::System* off_83271928;

namespace rw
{
namespace audio
{
namespace core
{
    // The two host-side engine-entry shims the sound Playback callers dispatch
    // through BY NAME (bodied 2026-08-25, faithful-audio-engine phase B5): each
    // is the corresponding System method, exactly what the console `bl
    // rw::audio::core::System::Lock/Unlock` sites do.
    void RwacSystemLock(System* apSystem)
    {
        System::Lock(apSystem);
    }
    void RwacSystemUnlock(System* apSystem)
    {
        System::Unlock(apSystem);
    }
} // namespace core
} // namespace audio
} // namespace rw

namespace CgsSound
{
namespace Playback
{
    // The process-wide default RWAC System accessor (bodied phase B5): the console
    // inlines the off_83271928 read + the "mpSystem" assert (CgsGenericRwacFactory.h:59)
    // at every consumer.
    rw::audio::core::System* GetDefaultRwacSystem()
    {
        CGS_ASSERT(off_83271928 != 0, "mpSystem");
        return off_83271928;
    }
} // namespace Playback
} // namespace CgsSound

namespace CgsSound
{
namespace Playback
{

RwacLock::RwacLock(rw::audio::core::System* apSystem)
    : mpSystem(apSystem)
{
    if (mpSystem == 0)
    {
        // Fall back to the process-wide default RWAC System (X360 off_83271928).
        mpSystem = GetDefaultRwacSystem();
        CGS_ASSERT(mpSystem != 0, "mpSystem");
    }

    // Hold the audio-core System locked for the guard's lifetime.
    rw::audio::core::RwacSystemLock(mpSystem);
}

} // namespace Playback
} // namespace CgsSound
