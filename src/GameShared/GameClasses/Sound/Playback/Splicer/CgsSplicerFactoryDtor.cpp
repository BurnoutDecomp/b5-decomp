// ============================================================================
// CgsSplicerFactoryDtor.cpp -- CgsSound::Playback::SplicerFactory destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826DB0E0
//   (CgsSound::Playback::SplicerFactory::`scalar deleting destructor')
//
// NOTE (updated 2026-08-25, audio-faithfulness wave 1): CgsSplicerFactory.cpp's
// old TU-local ad-hoc `struct SplicerFactory` is RETIRED -- SplicerAssertFunc
// @ 0x8268ABA0 is now a member declared in the coherent CgsSplicerFactory.h, which
// both TUs include. The split into two TUs is retained (harmless).
//
// The compiler synthesis runs:
//   SplicerFactory::~SplicerFactory(this);  // base dtor (Factory)
//   if (a2 & 1) operator delete(this);      // scalar-deleting tail
//
// SplicerFactory's own members (mpRegistry, mhRwacFactory, mpManager) are trivially
// destructible, so ~SplicerFactory contributes no member teardown beyond running the
// Factory base destructor; the body is empty.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerFactory.h"

namespace CgsSound
{
namespace Playback
{
    SplicerFactory::~SplicerFactory()
    {
        // The Factory base destructor is run implicitly here; SplicerFactory's own
        // members are trivially destructible.
    }
}
}
