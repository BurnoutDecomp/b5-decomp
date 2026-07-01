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

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

void ContentSpecFixer::DoFixDown(const Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SK_TYPE_NAME.GetValue(),
               "ContentSpec::SK_TYPE_NAME == lEntity.GetTypeName()");
}

void ContentSpecFixer::DoFixUp(const Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SK_TYPE_NAME.GetValue(),
               "ContentSpec::SK_TYPE_NAME == lEntity.GetTypeName()");
}

void ContentSpecFixer::DoRelocate(Entity& arEntity, u8* apu8Base,
                                  const Registry& arRegistryTo,
                                  const Registry& arRegistryFrom) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SK_TYPE_NAME.GetValue(),
               "ContentSpec::SK_TYPE_NAME == lEntity.GetTypeName()");

    RelocateMemberPointer<ContentType>(GetContentTypeSlot(arEntity),
                                       apu8Base, arRegistryTo, arRegistryFrom);
}

void ContentSpecFixer::DoResolve(Entity& arEntity, const Registry& arRegistry) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SK_TYPE_NAME.GetValue(),
               "ContentSpec::SK_TYPE_NAME == lEntity.GetTypeName()");

    ResolveMemberPointer<ContentType>(GetContentTypeSlot(arEntity), arRegistry);
}

void ContentSpecFixer::DoUnresolve(Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SK_TYPE_NAME.GetValue(),
               "ContentSpec::SK_TYPE_NAME == lEntity.GetTypeName()");

    UnresolveMemberPointer<ContentType>(GetContentTypeSlot(arEntity));
}

} // namespace Playback
} // namespace CgsSound
