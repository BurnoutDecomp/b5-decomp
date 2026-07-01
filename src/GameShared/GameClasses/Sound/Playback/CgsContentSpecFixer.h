#ifndef CGS_SOUND_PLAYBACK_CGSCONTENTSPECFIXER_H
#define CGS_SOUND_PLAYBACK_CGSCONTENTSPECFIXER_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"

// ============================================================================
// CgsSound::Playback::ContentSpecFixer  (DWARF home CgsDataStructures.h family).
//
// The type-handler ("fixer") for the serialised ContentSpec entity. Its fix hooks
// assert the fixed entity's mTypeName == ContentSpec::SK_TYPE_NAME, then the
// relocate/resolve/unresolve hooks fix the contained ContentType* member pointer at
// entity+8 (the ContentSpec's mpContentType slot) between registries.
//
// FLAG: MINIMAL home for the Wave-6 ContentSpec fixer TUs. The fixer is modelled as
// a plain named handler struct (mirroring the committed AemsVoiceCsisClass), carrying
// only its interned SK_TYPE_NAME. The registration ctor/dtor and the other Do*
// overrides are DEFERRED. The ContentType* slot lives inside the serialised entity's
// payload (the X360 addi r3,r31,8): recovered BY NAME via GetContentTypeSlot rather
// than by fabricating a full ContentSpec entity layout.
// ============================================================================

namespace CgsSound
{
namespace Playback
{

struct ContentSpecFixer
{
    // The interned type-name the hooks validate the fixed entity against. DECLARED
    // here; the interned-Name DEFINITION lives with the registration TU (DEFERRED).
    static const Name SK_TYPE_NAME;

    // @ 0x826929D0 / 0x82692A20. Pre/post-relocation fix hooks -- assert type-name.
    void DoFixUp(const Entity& arEntity) const;
    void DoFixDown(const Entity& arEntity) const;

    // @ 0x826AC820 / 0x826AC898 / 0x826AC900. Resolve/unresolve/relocate the
    // contained ContentType* member pointer at entity+8.
    void DoResolve(Entity& arEntity, const Registry& arRegistry) const;
    void DoUnresolve(Entity& arEntity) const;
    void DoRelocate(Entity& arEntity, u8* apu8Base,
                    const Registry& arRegistryTo,
                    const Registry& arRegistryFrom) const;

    // The ContentType* slot inside the serialised ContentSpec entity payload lives at
    // entity+8 (the X360 `addi r3,r31,8`). Recovered BY NAME -- reinterpret the
    // Entity base + 8 as the ContentType* slot (parameters/resolvers see it as a
    // (const ContentType**) so the member-pointer helpers can rewrite it in place).
    static const ContentType** GetContentTypeSlot(Entity& arEntity)
    {
        return reinterpret_cast<const ContentType**>(
            reinterpret_cast<u8*>(&arEntity) + 8);
    }
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_CGSCONTENTSPECFIXER_H
