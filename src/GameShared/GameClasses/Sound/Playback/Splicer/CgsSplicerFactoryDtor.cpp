// ============================================================================
// CgsSplicerFactoryDtor.cpp -- CgsSound::Playback::SplicerFactory destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826DB0E0
//   (CgsSound::Playback::SplicerFactory::`scalar deleting destructor')
//
// NOTE (consolidator): the committed CgsSplicerFactory.cpp homes SplicerFactory::
// SplicerAssertFunc @ 0x8268ABA0 using a self-contained LOCAL ad-hoc `struct
// SplicerFactory` (NOT the coherent CgsSplicerFactory.h). To avoid two conflicting
// definitions of SplicerFactory in one TU, this destructor is emitted in a SEPARATE
// sibling TU that includes the coherent header.
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
