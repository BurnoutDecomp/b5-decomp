#ifndef CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERFACTORY_H
#define CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERFACTORY_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"   // Factory (base)
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"  // Registry
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"    // Handle<GenericRwacFactory>

struct SpliceManager;

// ============================================================================
// CgsSplicerFactory.h  (MINIMAL home for the SplicerFactory destructor TU).
//
//   CgsSound::Playback::SplicerFactory::`scalar deleting destructor'  @ 0x826DB0E0
//
// DWARF (CgsSplicerFactory.h:58): SplicerFactory : public Factory, with three
// trivially-destructible members. The class destructor body is empty -- the Factory
// base dtor does the teardown; MSVC re-synthesises the scalar-deleting thunk from the
// out-of-line dtor. Members pinned BY NAME (host-width FLAG).
// ============================================================================

namespace CgsSound
{
namespace Playback
{

class GenericRwacFactory;

struct SplicerFactory : public Factory
{
    // The ctor (registers with the environment) is its own TU; declared for shape so
    // the base construction is well-formed. FLAG (DEFER): declared-only.
    SplicerFactory(Name aName, Environment& arEnvironment);

    // @ 0x826DB0E0. Empty out-of-line dtor (Factory base dtor runs implicitly).
    virtual ~SplicerFactory();

    // @ 0x8268ABA0. The splice factory's assertion sink (always fires; returns the
    // assert front-end's leave result). Bodied in CgsSplicerFactory.cpp.
    void* SplicerAssertFunc(const char* lpcExpression);

private:
    Registry*                  mpRegistry;     // CgsSplicerFactory.h:128
    Handle<GenericRwacFactory> mhRwacFactory;  // CgsSplicerFactory.h:129
    SpliceManager*             mpManager;      // CgsSplicerFactory.h:130
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICERFACTORY_H
