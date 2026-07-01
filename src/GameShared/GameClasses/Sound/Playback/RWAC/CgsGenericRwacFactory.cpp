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
