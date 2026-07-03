#ifndef BRN_SIMPLE_PARTICLE_ARRAY_H
#define BRN_SIMPLE_PARTICLE_ARRAY_H

// ============================================================================
// GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h
//
// BrnParticle::Native::BrnSimpleParticleArray -- the platform ("Native") simple-particle
// array. During FX-bundle load (caller: BrnParticle::ParticleModule::LoadFXBundle) the
// loader walks each native particle-array slot and calls AcquireTexture to resolve the
// per-array texture descriptor: it matches the array's referenced Lion texture name
// (a BrnParticle::TextureNameMap::Entry) against the requested name-hash and, on a hit
// (or when the slot has no texture name), publishes the resolved 64-bit texture
// descriptor into the renderer's per-array-type descriptor table and marks the array
// dirty.
//
// LAYOUT AUTHORITY (X360 ARTIST asm, AcquireTexture @0x8227E468):
//   this[0]  @ +0x00 -- mbDirty (byte flag, set to 1 once a descriptor is published)
//   this[9]  @ +0x24 -- mpTextureNameRef : pointer to the slot's texture-name reference;
//                       its first word (ref[0]) is the owning GDB/Lion name string
//                       (a NUL-terminated char*), null when the slot has no texture name.
//
// The leArrayID argument indexes the renderer's static per-array-type descriptor table
// (AttribSys::Enums::NativeParticleType::eParticleArray_Max == 13 entries). The
// descriptor is a 64-bit value; an unresolved slot publishes the module's default
// descriptor.
//
// X360 pointers are 32-bit; on the 64-bit host the absolute byte offsets above do NOT
// hold. Members are pinned BY NAME and order, not by an absolute-offset assert.
// GROW additively.
//
// HONEST PLACEHOLDER: only the fields AcquireTexture touches are modelled. The full
// BrnSimpleParticleArray layout (the simple-particle pool itself) is not reconstructed
// in this pass.
// ============================================================================

#include "types.hpp"

namespace BrnParticle
{
namespace Native
{
    // The per-array texture-name reference object. Only its leading name pointer is
    // load-bearing for AcquireTexture.
    struct SimpleParticleTextureNameRef
    {
        char* mpTextureName;   // ref[0] @ +0x00 -- owning GDB/Lion texture-name string (may be null)
    };

    class BrnSimpleParticleArray
    {
    public:
        // BrnParticle::Native::BrnSimpleParticleArray::CB4ParticleBank -- one CB4 particle
        // bank of this array (the regular bank @ +0x04, the crash bank @ +0x14). Render walks
        // the bank's live CB4 particles and builds one shaded/rotated screen-space quad per
        // surviving particle into the locked vertex buffer via the iterator.
        //
        // HONEST STUB -- VMX KEYSTONE (NOT reconstructed in this pass). Render @0x8291E600 is
        // a ~1000-instruction hand-vectorised VMX/AltiVec pipeline (Hex-Rays: "local variable
        // allocation has failed") with no faithful scalar lowering (mirrors the committed
        // ShadedRotatingRenderMethod::BuildQuad stub). See BrnSimpleParticleRenderer.cpp home.
        struct CB4ParticleBank
        {
            // 3-arg register contract proven by the caller @0x8291F948-58/0x8291F978-88:
            //   this = the CB4 bank (r3); lpIterator = r4; lpArray = r5 (owning
            //   BrnSimpleParticleArray); lpCamera = r6 (the local CgsGraphics::Camera copy).
            // HONEST STUB: not implemented -- VMX keystone.
            void Render(void* lpIterator, void* lpArray, void* lpCamera);
        };

        // BrnParticle::Native::BrnSimpleParticleArray::AcquireTexture @0x8227E468.
        // Resolve and publish the texture descriptor for native-particle-array type
        // leArrayID. luRequestedHash is matched against the hash of this array's
        // texture-name reference; lTextureDescriptor is the 64-bit descriptor to
        // publish on a hit. Returns true once a descriptor was published.
        //   - leArrayID must be in [0, eParticleArray_Max).
        //   - If the slot has a texture name: publish lTextureDescriptor only when its
        //     hash equals the requested hash.
        //   - If the slot has no texture name: a non-zero leArrayID is unexpected
        //     (asserts); the module default descriptor is published unconditionally.
        // The X360 returns a leftover register the caller (LoadFXBundle) discards, so
        // the contract is the side effects (publish + mark dirty); modelled as void.
        void AcquireTexture(u32 luRequestedHash, u64 lTextureDescriptor, u32 leArrayID);

    private:
        u8                            mbDirty;             // this[0]  @ +0x00 (byte flag)
        // this[1..8] -- opaque simple-particle-array state, not touched by AcquireTexture.
        void*                         mpReserved1;
        void*                         mpReserved2;
        void*                         mpReserved3;
        void*                         mpReserved4;
        void*                         mpReserved5;
        void*                         mpReserved6;
        void*                         mpReserved7;
        void*                         mpReserved8;
        SimpleParticleTextureNameRef* mpTextureNameRef;    // this[9]  @ +0x24
    };
}
}

#endif
