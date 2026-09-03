#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h
//
// cParticleEmitter -- a live Lion (eauk_lion) particle emitter instance: the runtime state
// for one playing effect (parent transform, simulation seeds, timing) plus the descriptor /
// bucket / behaviour pointers that drive it.
//
// ⭐⭐ THE LAYOUT IS NOW NAMED, AND IT WAS CONFIRMED OFFSET-FOR-OFFSET BY THE ASM. It used to
// be `u8 maReserved0[0x1F8]; cParticleDescriptor* mpDescriptor;` -- one named member and 504
// opaque bytes. burnout.wiki's "Particle Description" page carries the full cParticleEmitter
// table, and the wiki is name/type authority but NEVER offset authority -- so it was checked
// against cParticleEmitter::Init @0x82913228, which writes NINETEEN distinct fields by offset.
// Every single one lands on a named wiki member:
//
//    Init writes            wiki member                        Init writes         wiki member
//    *this      = 0     ->  mBucketsUsed        @0x000        *(this+416) = 0  -> mUpdateLastTime @0x1A0
//    *(this+96) = 0.0f  ->  mForce.x            @0x060        *(this+420) = 0  -> mNextEmissionTime@0x1A4
//    *(this+100)= 0.0f  ->  mForce.y            @0x064        *(this+424) = 0.0-> m_age           @0x1A8
//    *(this+104)= 0.0f  ->  mForce.z            @0x068        RandomSeed::Init(this+432)
//    *(this+108)= 0.0f  ->  mForce.w            @0x06C                          -> mEmitterSeed   @0x1B0
//    *(this+400)= 0     ->  mParentIndex        @0x190        *(this+496) = 0  -> mFlags          @0x1F0
//    *(this+404)= 0     ->  mpParentEmitter     @0x194        *(this+500) = 0  -> mEmissionCount  @0x1F4
//    *(this+412)= 0     ->  mLastTime           @0x19C        *(this+504) = d  -> mpDescriptor    @0x1F8
//    *(this+508)= 0     ->  mpBindings          @0x1FC        *(this+512) = 0  -> mpBucket        @0x200
//    *(this+516)= 0     ->  mpNext              @0x204        *(this+520) = 0  -> mPhysicsHandle  @0x208
//    *(this+524)= 0     ->  mpCurrentBehaviour  @0x20C        *(this+528) = 0  -> mpTempBehaviour @0x210
//    *(this+532)= 100.1253f -> mBlendLast       @0x214
//
// and the record's total 0x2D0 is exactly the stride cParticleEmitterManager::AppInit
// @0x82913470 allocates and indexes with (`mulli r10, r10, 0x2D0`). A table that agrees with
// the asm on nineteen offsets and on sizeof is not a guess.
//
// ⭐⭐ NOTHING IN THIS RECORD IS A RESERVED SPAN ANY MORE (2026-09-03). Three members used to
// be `u8 maXxx[N]` because their types lived in ParticleBucket.h, whose HONEST-PLACEHOLDER
// `struct cMatrix` collided with ParticleRender.h's `typedef rw::math::vpu::Matrix44 cMatrix`
// -- and this header is reached from LionFX.cpp, which needs both. That collision is retired:
// cMatrix has ONE home (eauk_common/Maths/Matrix.h) and sParticleNucleus has ONE home
// (sParticle.h), so the three are now declared as what they are --
//   cMatrix                  mParentBaseMatrix             (was maParentBaseMatrix[0x40])
//   sParticleNucleus         mParentEmitterNucleus         (was maParentEmitterNucleus[0xE0])
//   ParticleBuildData        mPrecalculatedParticleBuildData (was maPrecalculated...[0xB0])
// and this header includes ParticleBucket.h for the bucket type the simulation walks.
//
// ⭐ ParticleBuildData IS THE DWARF'S OWN NESTED STRUCT (ParticleEmitter.h:277) and its size
// closes the record independently: eleven members, every one a 16-byte VMX lane register
// (Vector4 / Vector2 / Vector3 / Vector3Plus / VecFloat are all one register on PPC), so
// 11 * 16 == 176 == 0xB0 -- exactly the span it replaces, and 0x220 + 0xB0 == 0x2D0, the
// stride cParticleEmitterManager::AppInit @0x82913470 allocates with (`mulli r10,r10,0x2D0`).
//
// X360 pointers are 32-bit; on the host they widen, so the console offsets above are NOT host
// layout facts. Members are pinned BY NAME and SEQUENCE.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h"   // cVector (mParentVel / mForce)
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Matrix.h"   // cMatrix (mParentBaseMatrix)
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRandomSeed.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/sParticle.h"      // sParticleNucleus
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBucket.h"  // cParticleBucket
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/ext-include/GameStructs/cTime.h"

#include "rw/math/vpu/types.h"   // the lane-register spellings ParticleBuildData's DWARF uses

class  cParticleDescriptor;
struct cParticleBehaviour;
struct cLionBindings;   // LionBindings.h (sibling home) -- Bind() attaches one to this emitter

class cParticleEmitter
{
public:
    // ParticleEmitter.h:266 (DWARF) -- what one call to ParticleBuild concluded about a
    // particle. cParticleEmitter::ParticleBuild returns it and the Simulate* family switches
    // on it to keep, skip or retire a slot.
    enum EParticleBuildResult
    {
        eParticleBuildResultNotBornYet = 0,
        eParticleBuildResultAlive      = 1,
        eParticleBuildResultDead       = 2,
    };

    // ParticleEmitter.h:277 (DWARF) -- the per-emitter constants PrecalculateParticleBuildData
    // @0x8290E018 works out ONCE per behaviour change, so that ParticleBuild does not redo
    // them per particle per frame. Every member is one 16-byte VMX lane register; the DWARF's
    // Vector2 / Vector3 / Vector3Plus / Vector4 / VecFloat spellings say how many of the four
    // lanes carry meaning, not how wide the slot is (see the banner's size proof).
    struct ParticleBuildData
    {
        rw::math::vpu::Vector4     mvDeltaTimeAndCurrentTime;              // :278
        rw::math::vpu::Vector2     mvAlphaFadeInAndFadeOut;                // :281
        rw::math::vpu::Vector3     mvScaleAndProportionalScaleYXAndZX;     // :282
        rw::math::vpu::Vector2     mvOrientStepAndDragFrameRateConstants;  // :283
        rw::math::vpu::Vector3     mvDragFactorsVelRotScale;               // :284
        rw::math::vpu::Vector3Plus mvRGBADiff;                             // :287
        rw::math::vpu::Vector3Plus mvRGBA0;                                // :288
        rw::math::vpu::Vector3Plus mvRGBAVar;                              // :289
        rw::math::vpu::Vector3Plus mvRGBABase;                             // :290
        rw::math::vpu::Vector4     mvfFrameCount;                          // :293 (VecFloat)
        rw::math::vpu::Vector4     mvfOneOverFrameCount;                   // :294 (VecFloat)
    };

    // The descriptor this emitter is playing (console +0x1F8). LionParticleRender::Render
    // switches on its render mode to pick the draw shape.
    const cParticleDescriptor* GetDescriptor() const { return mpDescriptor; }

    // Bind arBindings onto this emitter: store the binding pointer and back-link the
    // binding's emitter (cLionBindings::SetEmitter). Called (inlined) by
    // cLionParticleEffectManager::BindingsAttach @ 0x82914530. NOT RECONSTRUCTED -- see
    // LionRuntimeLinkStubs.cpp.
    void Bind(cLionBindings& arBindings);

    // Detach apBucket from this emitter's bucket list. Called by
    // cParticleBucketManager::Free @ 0x8290F378 as it recycles a bucket back to the pool.
    // X360 @0x82909790 is an EXPORT-SET HOLE; NOT RECONSTRUCTED -- see LionRuntimeLinkStubs.cpp.
    void BucketRemove(cParticleBucket* apBucket);

    // Bring the emitter to its initial state for apDescriptor (nullptr while pooling).
    // X360 @0x82913228. RECONSTRUCTED (ParticleEmitter.cpp) -- it runs during
    // cParticleEmitterManager::AppInit, i.e. during cLionFX::Init, on every boot.
    void Init(cParticleDescriptor* apDescriptor);

    // Release the emitter's buckets/behaviours back to their pools. X360 @0x82913330.
    // NOT RECONSTRUCTED -- see LionRuntimeLinkStubs.cpp.
    void DeInit();

    // Advance one frame; returns non-zero while still alive (0 -> manager unregisters it).
    // X360 @0x829153D8 -- the head of the Lion SIMULATION core (Generate / Emit /
    // ParticleBuild / InitialiseParticle / Blend). NOT RECONSTRUCTED -- see
    // LionRuntimeLinkStubs.cpp.
    u32 Update(const cTime& arTime);

    // Set the "active" flag word (bit 0 == active). The X360 folds SetActiveFlag(1) into
    // `mFlags |= 1` at its call sites; re-outlined here.
    void SetActiveFlag(u32 auFlag)                { mFlags |= auFlag; }

    // Manager free/used list link (console +0x204).
    void SetNext(cParticleEmitter* apNext)        { mpNext = apNext; }
    cParticleEmitter*& GetNextEmitter()           { return mpNext; }

private:
    // ParticleEmitter.h:306 -- fill one freshly allocated particle from the current
    // behaviour's base/variance pairs, the emitter's material and the spawn transform.
    // X360 @0x829116A8. RECONSTRUCTED (ParticleEmitter.cpp).
    //
    // The parameter NAMES are the DecFIGS DWARF's own (ParticleEmitter.cpp:1300):
    // lParticleNucleus, lpParticleVector, lpParticleMatrix, locator, velocity, aSeed, aTime,
    // lCurrentLocatorTime. The const-ness is the HEADER dump's (ParticleEmitter.h:306) --
    // the .cpp dump prints an extra `const` on every reference, including the two this
    // function writes through.
    void InitialiseParticle(sParticleNucleus& arParticleNucleus,
                            cVector* apParticleVector,
                            cMatrix* apParticleMatrix,
                            const cMatrix& arLocator,
                            const cVector& arVelocity,
                            cParticleRandomSeed& arSeed,
                            const cTime& arTime,
                            const cTime& arCurrentLocatorTime);

    // ---- members (burnout.wiki cParticleEmitter, every offset below re-checked against
    //      cParticleEmitter::Init @0x82913228 / AppInit @0x82913470 -- see the banner) ----
    u32                  mBucketsUsed;              // 0x000
    u8                   maPad004[0x0C];            // 0x004  (wiki: explicit padding)
    cMatrix              mParentBaseMatrix;         // 0x010  DWARF ParticleEmitter.h:334
    cVector              mParentVel;                // 0x050
    cVector              mForce;                    // 0x060
    sParticleNucleus     mParentEmitterNucleus;     // 0x070  DWARF ParticleEmitter.h:337
    cParticleRandomSeed  mParentRandomSeed;         // 0x150  (console span 0x40)
    u8                   maPadParentSeed[0x10];     // 0x180  the seed's tail padding to 0x190
    u32                  mParentIndex;              // 0x190
    cParticleEmitter*    mpParentEmitter;           // 0x194
    cTime                mParentTime;               // 0x198  DWARF ParticleEmitter.h:341
    cTime                mLastTime;                 // 0x19C  DWARF ParticleEmitter.h:342
    cTime                mUpdateLastTime;           // 0x1A0  DWARF ParticleEmitter.h:343
    s32                  mNextEmissionTime;         // 0x1A4
    f32                  m_age;                     // 0x1A8
    f32                  mDt;                       // 0x1AC
    cParticleRandomSeed  mEmitterSeed;              // 0x1B0  (console span 0x40)
    u8                   maPadEmitterSeed[0x10];    // 0x1E0  the seed's tail padding to 0x1F0
    u32                  mFlags;                    // 0x1F0
    u32                  mEmissionCount;            // 0x1F4
    cParticleDescriptor* mpDescriptor;              // 0x1F8
    cLionBindings*       mpBindings;                // 0x1FC
    cParticleBucket*     mpBucket;                  // 0x200
    cParticleEmitter*    mpNext;                    // 0x204
    u32                  mPhysicsHandle;            // 0x208
    cParticleBehaviour*  mpCurrentBehaviour;        // 0x20C
    cParticleBehaviour*  mpTempBehaviour;           // 0x210
    f32                  mBlendLast;                // 0x214
    u8                   maPad218[0x08];            // 0x218  (wiki: explicit padding)
    ParticleBuildData    mPrecalculatedParticleBuildData;         // 0x220 DWARF :363

    // ⚠ THE TWO SEED PADS ABOVE ARE THE CONSOLE'S, NOT OURS. The wiki sizes cParticleRandomSeed
    // 0x40 (0x30 of Random + mSeed + 0xC of padding) and the emitter's two seed slots span
    // 0x150..0x190 and 0x1B0..0x1F0, i.e. 0x40 each. This project's ParticleRandomSeed.h models
    // only the 0x34 the bucket's DWARF gap attests, so the remainder is carried here explicitly
    // rather than silently changing that type's size for every other user of it.
};
