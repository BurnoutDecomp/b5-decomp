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

struct cParticleBehaviour;      // ParticleBehaviour.h (sibling home) -- behaviour chain node
class  cParticleMaterial;       // ParticleMaterial.h (sibling home)
class  cLionSerialiser;         // LionSerialiser.h (sibling home)
// Pointer-only back-reference to the owning definition (DWARF :289); LionEffect.h includes
// this header for its own descriptor walk, so a forward declaration breaks that cycle.
struct cLionEffectDefinition;   // LionEffect.h (sibling home)

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
        E_FLAG_NEEDS_BUCKET      = 0x10,    // descriptor draws particles -> needs a bucket
        E_FLAG_FORCE_HEAVY       = 0x20,    // force the heavyweight bucket type
        // Asm-attested bit (cLionParticleEffectManager::BindingsAttach masks 0x8000 to skip
        // a descriptor); name inferred -- sub/child descriptors are spawned by their parent
        // emitter, so they are excluded from top-level binding attach.
        E_FLAG_SKIP_AUTO_EMITTER = 0x8000,
    };

    // cParticleDescriptor::GetRequiredBucketType @ 0x82908660. Decides which bucket
    // pool an emitter using this descriptor must allocate from. Caller (X360 xref):
    // cParticleEmitter::Emit.
    BucketType GetRequiredBucketType() const;

    // The draw/render-mode selector (console +0x2C; the DWARF and the Lion token table both
    // call the member SHAPE / mShape). LionParticleRender::Render switches on it to pick the
    // geometry shape (0 sprites, 1 quads, 3/4 tilts; 2/other = none). Inline accessor -- the
    // X360 build reads the field directly (no out-of-line call).
    u32 GetRenderMode() const { return mShape; }

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

    // ----- the record, in full -------------------------------------------------------
    // ⭐ COMPLETED 2026-09-03 (boost-exhaust wave). Everything except mFlags/mShape used to
    // be `maReservedN[]` opaque spans. It does not have to be: the DecFIGS DWARF
    // (ParticleDescriptor.h:262-293) declares the WHOLE record, member for member, and every
    // one of its offsets is confirmed twice over --
    //   (a) by the X360 asm that reaches them: Relocate @0x8290F488 rebases words 14/16/17/
    //       19/20/21/22/23 and ends `word18 = word16`; Delocate @0x8290CE50 twiddles word 15
    //       but never delocates it (so it is a value, not a pointer -- DWARF: mBehaviourCount);
    //       GetDurationMax @0x82909698 reads +4/+8/+20/+24 and returns -1 on +28;
    //       Serialise @0x8290F640 copies exactly 96 bytes, which is sizeof this record;
    //   (b) by the Lion authoring token table (X360 cLionTokenTable @0x82F36A34, transcribed
    //       in LionParticleParser.cpp), which NAMES the first thirteen fields: PAUSE_TIME +4,
    //       PAUSE_TIME_VARIANCE +8, REPEAT_TIME +12, REPEAT_TIME_VARIANCE +16,
    //       EMITTER_LIFE_BASE +20, EMITTER_LIFE_VARIANCE +24, EMITTER_LIFE_INFINITE +28,
    //       the DO_* flag bits +32, LODGROUP +36, RENDERGROUP +40, SHAPE +44,
    //       COLLISION_TYPE +48, NAME +56.
    // Console offsets are the 4-byte-pointer ABI; on the x64 host the pointer half widens, so
    // the absolute offsets are NOT host-asserted -- members are reached BY NAME.
    u32  mID;                          // console +0x00 (0)   DWARF :262
    f32  mPauseTime;                   // console +0x04 (4)   token PAUSE_TIME
    f32  mPauseTimeVariance;           // console +0x08 (8)   token PAUSE_TIME_VARIANCE
    f32  mRepeatTime;                  // console +0x0C (12)  token REPEAT_TIME
    f32  mRepeatTimeVariance;          // console +0x10 (16)  token REPEAT_TIME_VARIANCE
    f32  mEmitterLifeBase;             // console +0x14 (20)  token EMITTER_LIFE_BASE
    f32  mEmitterLifeVariance;         // console +0x18 (24)  token EMITTER_LIFE_VARIANCE
    u32  mEmitterLifeInfiniteFlag;     // console +0x1C (28)  token EMITTER_LIFE_INFINITE
    u32  mFlags;                       // console +0x20 (32)  the DO_* bits
    u32  mLodGroup;                    // console +0x24 (36)  token LODGROUP
    u32  mRenderGroup;                 // console +0x28 (40)  token RENDERGROUP
    // token SHAPE. The DWARF spells this member mShape; GetRenderMode() below keeps the
    // name the committed readers already use.
    u32  mShape;                       // console +0x2C (44)
    u32  mCollisionType;               // console +0x30 (48)  token COLLISION_TYPE
    f32  mBlendLast;                   // console +0x34 (52)  DWARF :278
    char* mpName;                      // console +0x38 (56)  token NAME (LionChar*)

    // DWARF :282-293 -- private in the original; kept public here for the same reason the
    // sibling cParticleBehaviour / cParticleMaterial records are, so cLionParticleEffect's
    // serialise path can relink the chain by name.
    s32  mBehaviourCount;              // console +0x3C (60)  twiddled, never delocated
    cParticleBehaviour* mpBehaviours;  // console +0x40 (64)  behaviour chain head
    cParticleBehaviour* mpBehaviourTemp;   // console +0x44 (68)  the scratch/blend behaviour
    const cParticleBehaviour* mpBehaviour; // console +0x48 (72)  the current layer; Relocate
                                           //   and Serialise both end by setting it to
                                           //   mpBehaviours, and Delocate never touches it
    cParticleMaterial*  mpMaterial;    // console +0x4C (76)  the descriptor's material
    cLionEffectDefinition* mpDef;      // console +0x50 (80)  owning definition (DWARF :289)
    cParticleDescriptor* mpNext;       // console +0x54 (84)  next descriptor in the chain
    cParticleDescriptor* mpParent;     // console +0x58 (88)  parent (RegisterSubEmitter reads
                                       //   it to locate the emitter to attach under)
    cParticleDescriptor* mpChild;      // console +0x5C (92)  first child descriptor
};
// Console sizeof == 96, pinned by cParticleDescriptor::Serialise @0x8290F640
// (`cLionSerialiser::DataStore(a2, a1, 96)`) and GetSerialiseSize @0x8290D100
// (`*(a2 + 20) += 96`). The host record is larger only because its nine pointers widen.
