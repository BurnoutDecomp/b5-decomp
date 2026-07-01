// ============================================================================
// CgsParameterSchemaFixer.cpp -- CgsSound::Playback::EntityFixer<ParameterSchema>.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   DoFixDown @ 0x82689B80   DoFixUp @ 0x82689B30
//
// Both hooks are a single type-name tripwire asserting the fixed entity's mTypeName
// equals ParameterSchema::SK_TYPE_NAME; the fix-up/down field walk is inlined away
// (empty) in this build.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

template <>
void EntityFixer<ParameterSchema>::DoFixDown(Entity& arEntity) const
{
    CGS_ASSERT(ParameterSchema::SK_TYPE_NAME == arEntity.mTypeName,
               "ParameterSchema::SK_TYPE_NAME == lEntity.GetTypeName()");
}

template <>
void EntityFixer<ParameterSchema>::DoFixUp(Entity& arEntity) const
{
    CGS_ASSERT(ParameterSchema::SK_TYPE_NAME == arEntity.mTypeName,
               "ParameterSchema::SK_TYPE_NAME == lEntity.GetTypeName()");
}

} // namespace Playback
} // namespace CgsSound
