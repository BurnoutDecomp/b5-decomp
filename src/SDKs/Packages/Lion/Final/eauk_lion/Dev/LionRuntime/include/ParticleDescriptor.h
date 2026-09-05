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
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialisedPtr.h"

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
        // ⭐ THE GAME'S OWN TOKEN NAME (LionParticleParser.cpp:57, offset 32 == mFlags):
        // CELL_RENDER_FLAG. cParticleRender::Render @0x82914894 tests it FIRST, before the
        // active bit, and routes the emitter to EmitterCubeRender instead of EmitterRender --
        // the cell/volume renderer that wraps its particles inside a camera-anchored box.
        E_FLAG_CELL_RENDER       = 0x8,     // token CELL_RENDER_FLAG
        // ⚠ THE TOKEN TABLE CALLS THIS ONE DYNAMIC_PLACEMENT_FLAG (LionParticleParser.cpp:61),
        // which is what the three simulation kernels use it for: set -> the particle is placed
        // in WORLD space, so the locator transform they simulate against is the IDENTITY
        // (@0x82912114). The "needs a bucket" spelling here is an older inferred name kept
        // because several bodies already reference it.
        E_FLAG_NEEDS_BUCKET      = 0x10,    // token DYNAMIC_PLACEMENT_FLAG
        E_FLAG_FORCE_HEAVY       = 0x20,    // force the heavyweight bucket type
        // Token ORIENT_TO_CAMERA_FLAG / DWARF eDO_FACECAMERA (LionParticleParser.cpp:63 --
        // the token table and the DecFIGS enum agree). LionBlendRenderer::RenderTilts
        // @0x82282FC8 masks it with `rlwinm r14, r28, 0,25,25` (0x40) at 0x822830B0 and
        // 0x822835C8: when set, BOTH of its draw loops derive the segment transform from
        // BuildCameraOrientatedLocator instead of the caller's per-particle matrix.
        E_FLAG_ORIENT_TO_CAMERA  = 0x40,    // token ORIENT_TO_CAMERA_FLAG / eDO_FACECAMERA
        // Asm-attested bit (cLionParticleEffectManager::BindingsAttach masks 0x8000 to skip
        // a descriptor); name inferred -- sub/child descriptors are spawned by their parent
        // emitter, so they are excluded from top-level binding attach.
        E_FLAG_SKIP_AUTO_EMITTER = 0x8000,
        // The two bits cParticleEmitter::InitialiseParticle @0x829116A8 tests
        // (`rlwinm r10,r16,0,24,24` -> 0x80 at 0x8291195C, `rlwinm r11,r16,0,23,23` -> 0x100
        // at 0x82911B18). These names are NOT inferred: they are the Lion authoring token
        // table's own (X360 cLionTokenTable @0x82F36A34, transcribed in LionParticleParser.cpp),
        // which binds DO_WORLD_ACC to 0x80 and DO_IGNORE_ROT to 0x100 on the +32 flags word.
        // Token DO_REPEAT. cParticleEmitter::IsGenerating @0x8290D624 masks it with
        // `rlwinm r10, r10, 0,29,29` (bit 2 == 0x4) to pick the repeating schedule.
        E_FLAG_REPEAT            = 0x4,      // token DO_REPEAT
        E_FLAG_WORLD_ACC         = 0x80,     // token DO_WORLD_ACC
        E_FLAG_IGNORE_ROT        = 0x100,    // token DO_IGNORE_ROT
        // Token DO_PREFORM (LionParticleParser.cpp:59, the same authoring table).
        // cParticleEmitter::Generate @0x82915290 masks it with `rlwinm r11, r11, 0,17,17`
        // (bit 14 == 0x4000): when it is set and the emitter has not emitted yet, the emission
        // clock is back-dated by one whole particle lifetime, so the effect starts already
        // running instead of building up from its first particle.
        E_FLAG_PREFORM           = 0x4000,   // token DO_PREFORM
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
    cParticleDescriptor* GetNextDescriptor() const { return mpNext.Get(); }
    cParticleBehaviour*  GetBehaviours()    const { return mpBehaviours.Get(); }

    // DWARF ParticleDescriptor.h:166 / :172 -- the two accessors
    // cParticleEmitter::InitialiseParticle reaches (its DWARF call list names
    // cParticleDescriptor::Flags and cParticleDescriptor::Material). The X360 build reads
    // the fields directly (`lwz r16, 0x20(r11)` / `lwz r26, 0x4C(r11)` @0x829116F4), so
    // they are inline by construction; they are re-outlined here to their DWARF names
    // rather than left as raw member reads at the call site.
    u32                Flags()    const { return mFlags; }
    cParticleMaterial* Material() const { return mpMaterial.Get(); }

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
    // ⭐ 2026-09-03 (second half of the same wave): the nine links below are
    // tLionSerialisedPtr, not host pointers, so the console offsets in these comments are
    // ALSO the host offsets and the static_asserts at the bottom of this file check them.
    // A .lef payload is read verbatim -- widening the links moved every member after +0x38
    // and made cParticleDescriptor::Relocate fault at +0xE on the first descriptor.
    // Members are still reached BY NAME.
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
    tLionSerialisedPtr<char> mpName;   // console +0x38 (56)  token NAME (LionChar*)

    // DWARF :282-293 -- private in the original; kept public here for the same reason the
    // sibling cParticleBehaviour / cParticleMaterial records are, so cLionParticleEffect's
    // serialise path can relink the chain by name.
    s32  mBehaviourCount;              // console +0x3C (60)  twiddled, never delocated
    tLionSerialisedPtr<cParticleBehaviour>  mpBehaviours;     // +0x40 (64) chain head
    tLionSerialisedPtr<cParticleBehaviour>  mpBehaviourTemp;  // +0x44 (68) scratch/blend
    tLionSerialisedPtr<cParticleBehaviour>  mpBehaviour;      // +0x48 (72) current layer;
                                           //   Relocate and Serialise both end by setting
                                           //   it to mpBehaviours, Delocate never touches it
    tLionSerialisedPtr<cParticleMaterial>   mpMaterial;   // +0x4C (76) the material
    tLionSerialisedPtr<cLionEffectDefinition> mpDef;      // +0x50 (80) owning definition
    tLionSerialisedPtr<cParticleDescriptor> mpNext;       // +0x54 (84) next in the chain
    tLionSerialisedPtr<cParticleDescriptor> mpParent;     // +0x58 (88) parent
                                       //   (RegisterSubEmitter reads it to locate the
                                       //   emitter to attach under)
    tLionSerialisedPtr<cParticleDescriptor> mpChild;      // +0x5C (92) first child
};

// Console sizeof == 96, pinned by cParticleDescriptor::Serialise @0x8290F640
// (`cLionSerialiser::DataStore(a2, a1, 96)`) and GetSerialiseSize @0x8290D100
// (`*(a2 + 20) += 96`). With 32-bit links the host record is the same 96 bytes, and the
// eight offsets Relocate @0x8290F488 rebases (asm word indices 14, 16, 17, 19, 20, 21, 22,
// 23) are checked one by one so a future member insertion fails the gate, not the game.
static_assert(sizeof(cParticleDescriptor) == 96,
              "cParticleDescriptor is the 96-byte serialised record "
              "(cParticleDescriptor::Serialise @0x8290F640 DataStore(this, 96))");
static_assert(offsetof(cParticleDescriptor, mFlags)          == 0x20, "Relocate/asm word 8");
static_assert(offsetof(cParticleDescriptor, mShape)          == 0x2C, "GetRequiredBucketType");
static_assert(offsetof(cParticleDescriptor, mpName)          == 0x38, "Relocate asm word 14");
static_assert(offsetof(cParticleDescriptor, mpBehaviours)    == 0x40, "Relocate asm word 16");
static_assert(offsetof(cParticleDescriptor, mpBehaviourTemp) == 0x44, "Relocate asm word 17");
static_assert(offsetof(cParticleDescriptor, mpBehaviour)     == 0x48, "Relocate asm word 18");
static_assert(offsetof(cParticleDescriptor, mpMaterial)      == 0x4C, "Relocate asm word 19");
static_assert(offsetof(cParticleDescriptor, mpDef)           == 0x50, "Relocate asm word 20");
static_assert(offsetof(cParticleDescriptor, mpNext)          == 0x54, "Relocate asm word 21");
static_assert(offsetof(cParticleDescriptor, mpParent)        == 0x58, "Relocate asm word 22");
static_assert(offsetof(cParticleDescriptor, mpChild)         == 0x5C, "Relocate asm word 23");
