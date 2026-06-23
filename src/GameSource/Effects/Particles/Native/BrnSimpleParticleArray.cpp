#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
#include "SharedClasses/Graphics/TextureNameMapResourceType.h"   // BrnParticle::TextureNameMap::Entry::HashString
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnParticle::Native::BrnSimpleParticleArray::AcquireTexture  @ 0x8227E468
//
// AcquireTexture resolves the texture descriptor for one native-particle-array type
// during FX-bundle load. The renderer keeps two module-level statics (X360 data
// symbols, no own export -- modelled here as file-scope statics):
//   - gaTextureDescriptors[eParticleArray_Max] (@0x82FAC150): the per-array-type 64-bit
//     texture descriptor table, indexed by leArrayID.
//   - gDefaultTextureDescriptor (@0x82FACBA0/0x82FACBA4): the 64-bit "no texture"
//     default descriptor published when a slot carries no texture name.
//
// The two 32-bit halves of the default descriptor are unrecoverable rodata in this
// pass (they are initialised elsewhere at module bring-up); modelled as a zero-init
// placeholder. FLAGGED: the default-descriptor initial value is a placeholder, not an
// X360-observed constant.

namespace BrnParticle
{
namespace Native
{
    // AttribSys::Enums::NativeParticleType::eParticleArray_Max (the asserted upper bound
    // on leArrayID; the X360 checks 0 <= leArrayID < 0xD).
    static const u32 KU_PARTICLE_ARRAY_MAX = 13;

    // Per-array-type resolved texture descriptor table (@0x82FAC150, 13 x 64-bit).
    static u64 gaTextureDescriptors[KU_PARTICLE_ARRAY_MAX] = { 0 };

    // The module default "no texture" descriptor (@0x82FACBA0 low / 0x82FACBA4 high).
    // PLACEHOLDER: the X360 initial value is not recovered in this pass.
    static u64 gDefaultTextureDescriptor = 0;

    void BrnSimpleParticleArray::AcquireTexture(u32 luRequestedHash, u64 lTextureDescriptor, u32 leArrayID)
    {
        CGS_ASSERT(leArrayID < KU_PARTICLE_ARRAY_MAX,
                   "( leArrayID >= 0 ) && ( leArrayID < AttribSys::Enums::NativeParticleType::eParticleArray_Max )");

        const char* lpcTextureName = mpTextureNameRef->mpTextureName;

        if (lpcTextureName == nullptr)
        {
            // No texture name on this slot. A non-zero array id is unexpected here.
            CGS_ASSERT(leArrayID == 0u, "Missing Texture name for Native Particle Params");

            gaTextureDescriptors[leArrayID] = gDefaultTextureDescriptor;
            mbDirty = 1;
            return;
        }

        // The slot references a texture name: publish the candidate descriptor only when
        // its name-hash matches the requested hash.
        TextureNameMap::Entry lHasher;
        if (luRequestedHash == lHasher.HashString(lpcTextureName))
        {
            gaTextureDescriptors[leArrayID] = lTextureDescriptor;
            mbDirty = 1;
        }
    }
}
}
