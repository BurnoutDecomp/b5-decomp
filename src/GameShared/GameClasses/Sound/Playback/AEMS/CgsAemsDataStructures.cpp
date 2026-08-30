// ============================================================================
// CgsAemsDataStructures.cpp -- CgsSound::Playback::AemsVoiceCsisClass fix hooks.
//
// Definition home for the two AemsVoiceCsisClass fixer hooks. Bodied from
// BURNOUT_X360_ARTIST.XEX:
//   AemsVoiceCsisClass::DoFixUp   @ 0x8268AAC0
//   AemsVoiceCsisClass::DoFixDown @ 0x8268AB10
//
// Each hook reads the entity's interned type-name (Entity.mTypeName -- the X360
// `lwz r10,4(r4)`) and asserts it equals this fixer's SK_TYPE_NAME interned hash
// (the X360 global dword_82FFBD90 -- `lwz r11,dword_82FFBD90`). The compare is
// `cmplw cr6,r10,r11; beq <return>`; on inequality the Begin/Fire/End assert
// sequence runs with the message
//   "AemsVoiceCsisClass::SK_TYPE_NAME == lEntity.GetTypeName()"
// (CgsAemsDataStructures.h:163 for DoFixUp, :172 for DoFixDown). The body IS the
// assert: on a matching type-name nothing else happens (the entity's fix-up /
// fix-down field walk is empty for this entity in this build). Member access is
// BY NAME: the entity type-name via Entity::mTypeName / Name::GetValue, the class
// name via the static SK_TYPE_NAME -- no raw-offset cast.
//
// The asserts are vacuous in the house CGS_ASSERT substitution but are reproduced
// so the validated invariant (the fixer only ever fixes its own entity type)
// stays visible.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsDataStructures.h"
#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

// CgsAemsDataStructures.h. The fixer's interned type Name (the X360 global
// dword_82FFBD90). Constructing a Name from the type-name string interns it
// (MakeHash -> Store), so GetValue() yields the hash the X360 holds in that word.
const Name AemsVoiceCsisClass::SK_TYPE_NAME = Name("~AemsVoiceCsisClass~");

namespace
{
    const EntityFixer<AemsVoiceCsisClass> sAemsVoiceCsisClassFixer;
}

// ---------------------------------------------------------------------------
// AemsVoiceCsisClass::DoFixUp(arEntity)  @ 0x8268AAC0
//   assert arEntity.GetTypeName() == SK_TYPE_NAME; no field walk.
// (asm: lwz r10,4(r4) [entity.mTypeName]; lwz r11,dword_82FFBD90 [SK_TYPE_NAME];
//  cmplw; beq return; else Begin/Fire/End assert @ CgsAemsDataStructures.h:163.)
// ---------------------------------------------------------------------------
void AemsVoiceCsisClass::DoFixUp(const Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SK_TYPE_NAME.GetValue(),
               "AemsVoiceCsisClass::SK_TYPE_NAME == lEntity.GetTypeName()");
}

// ---------------------------------------------------------------------------
// AemsVoiceCsisClass::DoFixDown(arEntity)  @ 0x8268AB10
//   assert arEntity.GetTypeName() == SK_TYPE_NAME; no field walk.
// (asm: lwz r10,4(r4) [entity.mTypeName]; lwz r11,dword_82FFBD90 [SK_TYPE_NAME];
//  cmplw; beq return; else Begin/Fire/End assert @ CgsAemsDataStructures.h:172.)
// ---------------------------------------------------------------------------
void AemsVoiceCsisClass::DoFixDown(const Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SK_TYPE_NAME.GetValue(),
               "AemsVoiceCsisClass::SK_TYPE_NAME == lEntity.GetTypeName()");
}

} // namespace Playback
} // namespace CgsSound
