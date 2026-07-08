#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h
//
// cParticleDescriptor -- the Lion (eauk_lion) particle runtime descriptor that owns
// a chain of cParticleBehaviour layers and the top-level effect parameters. Only the
// slice exercised by cParticleDescriptor::GetRequiredBucketType is modelled here.
//
// LAYOUT AUTHORITY (X360 ARTIST asm, GetRequiredBucketType @ 0x82908660):
//   mFlags     @ console +0x20 (32) -- a 32-bit flags word; bit 0x10 ("needs a bucket")
//              and bit 0x20 ("needs the heavyweight bucket type") gate the result
//              (`rlwinm r9,r10,0,27,27` masks bit 4 == 0x10; `rlwinm r11,r10,0,26,26`
//              masks bit 5 == 0x20).
//   mRenderMode@ console +0x2C (44) -- a small enum (0..9) selecting the draw/render
//              mode; a 10-case switch maps it to bucket-type 1 for cases {0,3,4,9} and
//              bucket-type 2 (the default) otherwise.
//
// X360 pointers are 32-bit; on the 64-bit host the absolute byte offsets widen, so the
// leading reserved span places mFlags/mRenderMode at the observed console offsets but is
// NOT asserted as a host layout fact. Members are accessed BY NAME. GROW this descriptor
// additively (real member set, the behaviour chain head, etc.) as further Lion descriptor
// TUs land -- never reorder/retype the modelled members.
//
// HONEST PLACEHOLDER: the full descriptor is large and mostly un-reconstructed in this
// pass; everything except mFlags and mRenderMode is an opaque reserved span.
// ============================================================================

#include "types.hpp"

struct cParticleBehaviour;   // ParticleBehaviour.h (sibling home) -- behaviour chain node
class  cParticleMaterial;    // ParticleMaterial.h (sibling home)
class  cLionSerialiser;      // LionSerialiser.h (sibling home)

class cParticleDescriptor
{
public:
    // Bucket-type categories returned by GetRequiredBucketType. 0 == no bucket needed,
    // 1 == the lightweight bucket pool, 2 == the heavyweight bucket pool.
    enum BucketType
    {
        E_BUCKET_NONE  = 0,
        E_BUCKET_LIGHT = 1,
        E_BUCKET_HEAVY = 2,
    };

    // mFlags bits the runtime tests here.
    enum Flags
    {
        E_FLAG_NEEDS_BUCKET = 0x10,  // descriptor draws particles -> needs a bucket
        E_FLAG_FORCE_HEAVY  = 0x20,  // force the heavyweight bucket type
    };

    // cParticleDescriptor::GetRequiredBucketType @ 0x82908660. Decides which bucket
    // pool an emitter using this descriptor must allocate from. Caller (X360 xref):
    // cParticleEmitter::Emit.
    BucketType GetRequiredBucketType() const;

    // The draw/render-mode selector (console +0x2C). LionParticleRender::Render switches on
    // it to pick the geometry shape (0 sprites, 1 quads, 3/4 tilts; 2/other = none). Inline
    // accessor -- the X360 build reads the field directly (no out-of-line call).
    u32 GetRenderMode() const { return mRenderMode; }

    // Chain / child accessors (DecFIGS DWARF attests GetNextDescriptor / GetBehaviours;
    // the X360 build reads the fields directly). Members are public (as with the sibling
    // cParticleMaterial / cParticleBehaviour records) so the owning cLionParticleEffect
    // serialisation path can relink the descriptor chain by name.
    cParticleDescriptor* GetNextDescriptor() const { return mpNext; }
    cParticleBehaviour*  GetBehaviours()    const { return mpBehaviours; }

    // ---- serialise / relocate path (out-of-line in ARTIST; bodies in ParticleDescriptor's
    // own TUs). cLionParticleEffect walks these over its descriptor chain. ----
    void Delocate(u32 aEndianTwiddleFlag);
    void Relocate();
    void GetSerialiseSize(cLionSerialiser& aSer) const;
    cParticleDescriptor* Serialise(cLionSerialiser& aSer) const;
    f32  GetDurationMax() const;

    // True when arOther is this descriptor or one of its ancestors up the mpParent chain.
    // Body lives in a cParticleDescriptor TU; declared here for cParticleEmitterManager::
    // RegisterSubEmitter (X360 @0x82913668 calls it to find a sub-emitter's parent emitter).
    // DWARF: U32 IsChildOf(const cParticleDescriptor&) const (ParticleDescriptor.h:219).
    u32 IsChildOf(const cParticleDescriptor& arOther) const;

    // ----- serialised record members (console offsets; host pointer widths differ, so the
    // absolute offsets are NOT host-asserted -- members are accessed BY NAME). -----
    // Opaque leading record preceding mFlags at console +0x20 (32).
    u8  maReserved0[0x20];

    u32 mFlags;       // console +0x20 (32)

    // Opaque span between mFlags (+0x20, 4 bytes) and mRenderMode (+0x2C, 44).
    u8  maReserved1[0x2C - 0x24];

    u32 mRenderMode;  // console +0x2C (44) -- 0..9 draw/render mode selector

    // Opaque span between mRenderMode (+0x2C) and mpBehaviours (+0x40, 64).
    u8  maReserved2[0x40 - 0x30];

    cParticleBehaviour* mpBehaviours;  // console +0x40 (64) -- behaviour chain head

    // Opaque span between mpBehaviours (+0x40) and mpMaterial (+0x4C, 76).
    u8  maReserved3[0x4C - 0x44];

    cParticleMaterial*  mpMaterial;    // console +0x4C (76) -- the descriptor's material

    // Opaque span between mpMaterial (+0x4C) and mpNext (+0x54, 84).
    u8  maReserved4[0x54 - 0x50];

    cParticleDescriptor* mpNext;       // console +0x54 (84) -- next descriptor in the chain

    cParticleDescriptor* mpParent;     // console +0x58 (88) -- parent descriptor (DWARF
                                       // ParticleDescriptor.h:292 mpParent). RegisterSubEmitter
                                       // reads it to locate the used emitter to attach under.
};
