#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSAEMSDATASTRUCTURES_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSAEMSDATASTRUCTURES_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"   // CgsSound::Playback::Name
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h" // CgsSound::Playback::Entity

// =============================================================================
// CgsSound::Playback::AemsVoiceCsisClass
//   GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsDataStructures.h (DWARF home)
//   + GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsDataStructures.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. AemsVoiceCsisClass is one of the
// CSIS entity "fixers" in the AEMS data-structure family: a type-handler that
// serialises/relocates the AemsVoiceCsisClass entity payload. Two of its fix
// hooks are bodied by this TU:
//   AemsVoiceCsisClass::DoFixUp   @ 0x8268AAC0  (CgsAemsDataStructures.h:163)
//   AemsVoiceCsisClass::DoFixDown @ 0x8268AB10  (CgsAemsDataStructures.h:172)
//
// Both hooks do the same thing in the X360 build: read the entity's interned
// type-name (Entity.mTypeName, the +4 word the asm loads with `lwz r10,4(r4)`)
// and assert it equals this class's own SK_TYPE_NAME interned hash (the X360
// global dword_82FFBD90, loaded with lwz at @0x8268AB24 / @0x8268AAD4). The body
// IS the assert -- on a matching type-name nothing else happens (the X360 leaves
// r3 untouched on the normal path and returns; on mismatch it runs the
// Begin/Fire/End assert sequence). The fix-up/fix-down field walk that would
// follow is empty for this entity in this build.
//
// HOME RATIONALE: no committed CgsAemsDataStructures home existed; this is its
// canonical home. AemsVoiceCsisClass is the concrete EntityFixer<AemsVoiceCsisClass>
// (the IEntityFixer hierarchy is homed in CgsDataStructures.h). The full fixer
// surface (the EntityFixer<T> template, the spHead self-registration ctor/dtor,
// and the other Do* overrides) is DEFERRED to its own TUs; this minimal home
// models ONLY the two fix hooks this TU bodies plus the SK_TYPE_NAME they compare
// against. The Entity base (mName @ +0 / mTypeName @ +4) is the committed sibling
// home (CgsRegistry.h) -- NOT forked.
//
// Cited by X360 address only -- no leaked-source provenance.
// =============================================================================

namespace CgsSound
{
namespace Playback
{

// CgsAemsDataStructures.h (DWARF). The CSIS "voice class" entity fixer. Modelled
// minimally for the two fix-hook TUs: it carries the interned SK_TYPE_NAME the
// hooks validate the fixed entity against. FLAG: MINIMAL home -- the EntityFixer<T>
// base, the registration ctor/dtor and the remaining Do* overrides are DEFERRED.
struct AemsVoiceCsisClass
{
    // CgsAemsDataStructures.h. This fixer's interned type Name -- the X360 global
    // dword_82FFBD90 the fix hooks compare the entity's mTypeName against. DECLARED
    // here; its DEFINITION (the interned-Name initializer seeded from the type-name
    // string) is bodied in the .cpp.
    static const Name SK_TYPE_NAME;

    // @ 0x8268AAC0. Fix-up hook: assert the entity's type-name matches SK_TYPE_NAME
    // (CgsAemsDataStructures.h:163). No field walk in this build.
    void DoFixUp(const Entity& arEntity) const;

    // @ 0x8268AB10. Fix-down hook: assert the entity's type-name matches SK_TYPE_NAME
    // (CgsAemsDataStructures.h:172). No field walk in this build.
    void DoFixDown(const Entity& arEntity) const;
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_AEMS_CGSAEMSDATASTRUCTURES_H
