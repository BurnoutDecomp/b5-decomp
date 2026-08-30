// ============================================================================
// CgsContentSpecFixer.cpp -- CgsSound::Playback::ContentSpecFixer hooks.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   DoFixDown  @ 0x82692A20   DoFixUp   @ 0x826929D0
//   DoRelocate @ 0x826AC900   DoResolve @ 0x826AC820   DoUnresolve @ 0x826AC898
//
// Each hook asserts the fixed entity's mTypeName == ContentSpec::SK_TYPE_NAME; the
// relocate/resolve/unresolve hooks then fix the contained ContentType* member pointer
// at entity+8 via the free member-pointer helpers.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsContentSpecFixer.h"
#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

template <>
Name EntityFixer<ContentSpec>::DoGetTypeName() const
{
    return ContentSpec::SK_TYPE_NAME;
}

template <>
void EntityFixer<ContentSpec>::DoFixDown(Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == ContentSpec::SK_TYPE_NAME.GetValue(),
               "ContentSpec::SK_TYPE_NAME == lEntity.GetTypeName()");
}

template <>
void EntityFixer<ContentSpec>::DoFixUp(Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == ContentSpec::SK_TYPE_NAME.GetValue(),
               "ContentSpec::SK_TYPE_NAME == lEntity.GetTypeName()");
}

template <>
void EntityFixer<ContentSpec>::DoRelocate(Entity& arEntity, u8* apu8Base,
                                          const Registry& arRegistryTo,
                                          const Registry& arRegistryFrom) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == ContentSpec::SK_TYPE_NAME.GetValue(),
               "ContentSpec::SK_TYPE_NAME == lEntity.GetTypeName()");

    ContentSpec& lrContentSpec = static_cast<ContentSpec&>(arEntity);
    RelocateMemberPointer<ContentType>(&lrContentSpec.mpContentType,
                                       apu8Base, arRegistryTo, arRegistryFrom);
}

template <>
void EntityFixer<ContentSpec>::DoResolve(Entity& arEntity,
                                         const Registry& arRegistry) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == ContentSpec::SK_TYPE_NAME.GetValue(),
               "ContentSpec::SK_TYPE_NAME == lEntity.GetTypeName()");

    ContentSpec& lrContentSpec = static_cast<ContentSpec&>(arEntity);
    ResolveMemberPointer<ContentType>(&lrContentSpec.mpContentType, arRegistry);
}

template <>
void EntityFixer<ContentSpec>::DoUnresolve(Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == ContentSpec::SK_TYPE_NAME.GetValue(),
               "ContentSpec::SK_TYPE_NAME == lEntity.GetTypeName()");

    ContentSpec& lrContentSpec = static_cast<ContentSpec&>(arEntity);
    UnresolveMemberPointer<ContentType>(&lrContentSpec.mpContentType);
}

} // namespace Playback
} // namespace CgsSound
