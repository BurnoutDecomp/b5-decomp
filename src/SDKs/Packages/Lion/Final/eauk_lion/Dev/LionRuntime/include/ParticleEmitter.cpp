// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.cpp
//
// cParticleEmitter::Init @0x82913228 -- the one emitter body this build actually RUNS.
//
// ⭐ WHY IT MATTERS: cParticleEmitterManager::AppInit @0x82913470 calls Init(nullptr) on every
// pooled emitter, and AppInit runs inside cLionFX::Init, which ParticleModule::Prepare now
// calls on every boot. So this is not a "for later" body -- it executes 256 times before the
// first frame.
//
// 2026-09-03: three more bodies join it --
//   * InitialiseParticle @0x829116A8 -- fills a particle's fourteen nucleus channels, and the
//     reason sParticleNucleus needed a real layout at all;
//   * ParticleInsert     @0x829133C8 -- its only caller: reserve a bucket slot, initialise it;
//   * IsGenerating       @0x8290D538 -- the pause / life / repeat schedule that decides whether
//     this emitter emits anything in a given time window.
// ⚠ NONE OF THE THREE IS REACHED YET, and that is stated rather than implied: the chain above
// them (Update -> Generate -> Emit) is not reconstructed, so nothing calls ParticleInsert.
//
// The rest of the emitter -- Update (the simulation core), DeInit, Bind, BucketRemove -- is not
// reconstructed and is announced in LionRuntimeLinkStubs.cpp rather than faked here.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleTrigger.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffectManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleScaler.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBucketManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitterManager.h"   // SpawnSubEmitter registers through the manager singleton
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/RenderedParticle.h"   // the per-particle RENDER record the behaviour processors write
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cmath>   // floorf / fabsf -- Blend's de-optimised magic-number floor

#include <cstdio>
#include <cstdlib>   // rand -- InitialiseParticle's random start frame is the C library's

namespace
{
// mBlendLast's initial value: `lfs f0, -0xF30(r10)` at 0x829132B4 resolves to flt_820FF0D0,
// read out of the image (tools/re/x360rd.py 0x820FF0D0) as 0x42C84027 == 100.12529754638672.
// It is a "no blend in progress" sentinel, not a tunable: the blend path compares against it,
// it is never interpolated toward. The exact bits are kept so the comparison behaves.
const f32 KF_BLEND_LAST_NONE = 100.12529754638672f;   // flt_820FF0D0
}  // namespace

// ----------------------------------------------------------------------------
// cParticleEmitter::Init  @ 0x82913228
//
// Store for store, in the console's own order. The descriptor branch at the tail is what makes
// a REGISTERED emitter (apDescriptor != nullptr) different from a POOLED one: a descriptor with
// a single behaviour binds that behaviour directly and precalculates its build data, while one
// with several allocates a blend behaviour from the effect manager and starts a blend.
//
// ⭐ THAT TAIL IS NOW LANDED. It was parked because both of its arms needed bodies this tree did
// not have -- PrecalculateParticleBuildData @0x8290E018 and Blend @0x8290F730 -- and both landed
// in this wave. It is still not REACHED on this build (nothing registers an emitter, because
// cLionFX::EffectCreate is not reconstructed), and the only caller that does run,
// cParticleEmitterManager::AppInit, passes nullptr and never enters it.
// ----------------------------------------------------------------------------
void cParticleEmitter::Init(cParticleDescriptor* apDescriptor)
{
    mForce.x = 0.0f;
    mForce.y = 0.0f;
    mForce.z = 0.0f;
    mForce.w = 0.0f;

    mEmissionCount     = 0;
    mFlags             = 0;
    mNextEmissionTime  = 0;
    mpDescriptor       = apDescriptor;
    mpBindings         = nullptr;
    mpBucket           = nullptr;
    mpNext             = nullptr;
    mPhysicsHandle     = 0;
    mLastTime.BuildZero();
    mParentIndex       = 0;
    mBucketsUsed       = 0;

    mEmitterSeed.Init();

    m_age              = 0.0f;
    mUpdateLastTime.BuildZero();
    mpParentEmitter    = nullptr;
    mpCurrentBehaviour = nullptr;
    mpTempBehaviour    = nullptr;
    mBlendLast         = KF_BLEND_LAST_NONE;

    if (apDescriptor == nullptr)
    {
        return;
    }

    // ⭐ THE DESCRIPTOR TAIL IS LANDED (2026-09-03). It was announced-not-reconstructed because
    // both of its arms reached bodies this tree did not have; both now exist
    // (PrecalculateParticleBuildData @0x8290E018 and Blend @0x8290F730, this wave).
    //
    // asm 0x829132C0..0x82913308. mBehaviourCount decides which emitter this is: a ONE-LAYER
    // descriptor binds its single behaviour outright, a MULTI-LAYER one allocates a scratch
    // behaviour to blend into and hands the choice to Blend.
    if (apDescriptor->mBehaviourCount > 1)
    {
        mpTempBehaviour = cLionParticleEffectManager::Instance().CreateBehaviour();
        if (mpTempBehaviour != nullptr)
        {
            mpTempBehaviour->Init();
            mpTempBehaviour->Build();
        }
        Blend();
    }
    else
    {
        // ⚠ mBlendLast IS RESET TO 0 HERE, not left at the KF_BLEND_LAST_NONE sentinel the head
        // of this function wrote (`stfs f31, 0x214` at 0x82913304, and f31 is the 0.0 this
        // function keeps in a register throughout). A single-layer emitter never blends, so the
        // "always run the first Blend" sentinel would be meaningless on it.
        mBlendLast = 0.0f;
        mpCurrentBehaviour = apDescriptor->GetBehaviours();
        PrecalculateParticleBuildData();
    }
}

// ------------------------------------------------------------------------------------------
// THE ONE TYPE-SPELLING SEAM IN THIS FILE, said out loud rather than papered over.
//
// The console has ONE 16-byte lane register here. This tree carries two spellings of it: the
// Lion records (cParticleBehaviour's base/variance pairs, sParticleNucleus's channels) are
// `cVector` from eauk_common/Maths/Vector.h, while cParticleRandomSeed::Build's DWARF signature
// (ParticleRandomSeed.h:117) is `Vector3Plus Build(Vector4, Vector4)` in the rw::math::vpu
// spelling. Both are `{f32 x,y,z,w}`, 16 bytes, 16-aligned, and on PPC both are one `v` register
// -- which is why the original could hand one to the other with no conversion at all.
//
// These two adapters exist ONLY because our tree has not unified those two spellings. They are
// pure lane copies and compile to nothing. They are NOT a conversion the console performs, and
// they are not a place to put behaviour: if the two homes are ever unified, both disappear and
// the call sites below read exactly as they do now.
// ------------------------------------------------------------------------------------------
namespace
{
    inline rw::math::vpu::Vector4 LaneCast(const cVector& arV)
    {
        return rw::math::vpu::Vector4{ arV.x, arV.y, arV.z, arV.w };
    }

    inline cVector LaneCast(const rw::math::vpu::Vector3Plus& arV)
    {
        return cVector{ arV.x, arV.y, arV.z, arV.w };
    }

    // One `lvx128 v2,<variance>` / `lvx128 v1,<base>` / `bl sub_8290A648` / `stvx128` triple.
    inline cVector DrawChannel(cParticleRandomSeed& arSeed,
                               const cVector& arBase, const cVector& arVariance)
    {
        return LaneCast(arSeed.Build(LaneCast(arBase), LaneCast(arVariance)));
    }
}

// ================================================================================================
// cParticleEmitter::InitialiseParticle  @ 0x829116A8      (DWARF ParticleEmitter.h:306 /
//                                                          ParticleEmitter.cpp:2199)
//
// Fill one freshly allocated particle: draw all thirteen nucleus channels from the current
// behaviour's (base, variance) pairs, stamp its life / animation frame / birth time, and hand
// back its spawn transform. This is the function the whole Lion simulation hangs off -- every
// particle in the game starts here.
//
// ⭐ WHY IT READS AS A STRAIGHT RUN OF TWELVE IDENTICAL CALLS. The X360 body is one unrolled
// sequence of `lvx128 v2, r31, <varianceOffset>` / `lvx128 v1, r31, <baseOffset>` /
// `bl sub_8290A648` / `stvx128 v0, r29, <nucleusOffset>`, twelve times over (0x82911770 ..
// 0x82911940). r31 is mpCurrentBehaviour and r29 is the nucleus, so each triple names a
// behaviour member pair and a nucleus member -- and every one of the twelve pairs up BY NAME
// (mPosBase/mPosVariance -> mPos, and so on down the record). That correspondence is what
// pinned sParticleNucleus's layout; see sParticle.h.
//
// ⭐ sub_8290A648 IS cParticleRandomSeed::Build(Vector4, Vector4) -- the PER-LANE draw, not the
// splat. Each of a particle's twelve channels therefore varies independently per axis. Its
// sibling BuildLerp @0x8290A7A8, which splats one random across all four lanes, is NOT what
// this calls, and the difference is visible: a splat would put every particle's spawn offset on
// a single diagonal.
//
// PARAMETER NAMES are the DecFIGS DWARF's own (ParticleEmitter.cpp:1300). Register mapping:
// r3=this, r4=nucleus, r5=apParticleVector (spilled to arg_24 at once), r6=apParticleMatrix,
// r7=&arLocator, r8=&arVelocity, r9=&arSeed, r10=&arTime -- and arCurrentLocatorTime, the ninth
// value and so the first STACK argument, at 0x120+arg_54.
//
// ⭐⭐ THE FOUR PACKED LANES OF THE LIFE VECTOR ARE NAMED TWICE OVER. The asm writes three of
// them with `vrlimi128 vD, vS, <mask>, 0`, whose 4-bit mask selects a word MSB-first, and the
// DWARF's call list for this function names Vector4::SetX, SetZ and SetW -- in that order, for
// masks 8, 2 and 1. Two independent sources, the same three lanes:
//     mask 8 -> word 0 -> SetX -> LifeTime  = arSeed.Build(bhv.mLifeBase, bhv.mLifeVariance)
//     mask 2 -> word 2 -> SetZ -> FPS       = arSeed.Build(mat.mFPS,      mat.mFPSVariance)
//     mask 1 -> word 3 -> SetW -> BirthTime = arTime.GetTimeSeconds()
// Word 1 (FrameTime) is never written -- it keeps the zero the opening clear put there.
//
// ⚠ THE OPENING CLEAR IS ONLY THE LIFE VECTOR, NOT THE WHOLE NUCLEUS. `stvx128 v127, r0, r28`
// at 0x829116FC stores a zeroed register to r28 == nucleus + 0xD0 and to nothing else; the
// other thirteen channels are each fully overwritten by their own draw, so there is nothing to
// clear. The DWARF names this sParticleNucleus::Init, inlined -- but Init has no X360 body of
// its own, so writing one would be inventing a function to hold a single store.
//
// ⚠ WHAT THIS FUNCTION DOES *NOT* DO, said out loud because the parameter list invites the
// assumption: it never derives the spawn matrix's rotation from the particle's own mRot, and it
// never uses arVelocity for anything but the mLocatorVel scale. The spawn transform is either a
// verbatim copy of arLocator or, under DO_IGNORE_ROT, an identity carrying arLocator's
// translation. Orientation is ParticleBuild's job, per frame.
// ================================================================================================
void cParticleEmitter::InitialiseParticle(sParticleNucleus& arParticleNucleus,
                                          cVector* apParticleVector,
                                          cMatrix* apParticleMatrix,
                                          const cMatrix& arLocator,
                                          const cVector& arVelocity,
                                          cParticleRandomSeed& arSeed,
                                          const cTime& arTime,
                                          const cTime& arCurrentLocatorTime)
{
    // DWARF locals ParticleEmitter.cpp:2201 / 2203 / 2205 / 2207.
    const cParticleDescriptor& lrDescriptor = *mpDescriptor;
    const cParticleBehaviour&  lrBhv        = *mpCurrentBehaviour;
    const u32                  luFlags      = lrDescriptor.Flags();
    cParticleMaterial*         lpMat        = lrDescriptor.Material();

    // ---- the packed life / frame / FPS / birth vector (nucleus + 0xD0) ----------------------
    // Cleared first (0x829116FC), then three of its four lanes filled. FrameTime (word 1) is
    // deliberately left at zero -- the console never writes it here.
    arParticleNucleus.mvLifeTimeAndFrameTimeAndFPSAndBirthTime = cVector{ 0.0f, 0.0f, 0.0f, 0.0f };
    arParticleNucleus.BirthTime() = arTime.GetTimeSeconds();                             // SetW
    arParticleNucleus.LifeTime()  = arSeed.Build(lrBhv.mLifeBase, lrBhv.mLifeVariance);  // SetX

    // ---- the twelve vector channels, in the console's own order ----------------------------
    // The behaviour offsets in the trailing comments are the ones ParticleBehaviour.h's
    // static_asserts already pin; they are the `lvx128` displacements, read off the asm.
    arParticleNucleus.mPos          = DrawChannel(arSeed, lrBhv.mPosBase,             lrBhv.mPosVariance);             // 0x100 / 0x110
    arParticleNucleus.mVel          = DrawChannel(arSeed, lrBhv.mVelBase,             lrBhv.mVelVariance);             // 0x190 / 0x1A0
    arParticleNucleus.mAcc          = DrawChannel(arSeed, lrBhv.mAccBase,             lrBhv.mAccVariance);             // 0x000 / 0x010
    arParticleNucleus.mRot          = DrawChannel(arSeed, lrBhv.mRotXYZBase,          lrBhv.mRotXYZVariance);          // 0x090 / 0x0A0
    arParticleNucleus.mRotVel       = DrawChannel(arSeed, lrBhv.mRotXYZVelBase,       lrBhv.mRotXYZVelVariance);       // 0x0B0 / 0x0C0
    arParticleNucleus.mRotAcc       = DrawChannel(arSeed, lrBhv.mRotXYZAccBase,       lrBhv.mRotXYZAccVariance);       // 0x0D0 / 0x0E0
    arParticleNucleus.mOffsetRot    = DrawChannel(arSeed, lrBhv.mOffsetRotXYZBase,    lrBhv.mOffsetRotXYZVariance);    // 0x030 / 0x040
    arParticleNucleus.mOffsetRotVel = DrawChannel(arSeed, lrBhv.mOffsetRotXYZVelBase, lrBhv.mOffsetRotXYZVelVariance); // 0x050 / 0x060
    arParticleNucleus.mOffsetRotAcc = DrawChannel(arSeed, lrBhv.mOffsetRotXYZAccBase, lrBhv.mOffsetRotXYZAccVariance); // 0x070 / 0x080
    arParticleNucleus.mSize         = DrawChannel(arSeed, lrBhv.mSizeXYZBase,         lrBhv.mSizeXYZVariance);         // 0x130 / 0x140
    arParticleNucleus.mSizeVel      = DrawChannel(arSeed, lrBhv.mSizeXYZVelBase,      lrBhv.mSizeXYZVelVariance);      // 0x150 / 0x160
    arParticleNucleus.mSizeAcc      = DrawChannel(arSeed, lrBhv.mSizeXYZAccBase,      lrBhv.mSizeXYZAccVariance);      // 0x170 / 0x180

    // ---- the animation frame rate (word 2 of the life vector) ------------------------------
    arParticleNucleus.FPS() = arSeed.Build(lpMat->mFPS, lpMat->mFPSVariance);            // SetZ

    // ---- DO_WORLD_ACC: acceleration is authored in WORLD space, so rotate it into the -------
    // ---- emitter's locator frame. Descriptor flag 0x80 (Lion token DO_WORLD_ACC). ----------
    // asm 0x8291195C `rlwinm r10, r16, 0,24,24` masks bit 0x80; the DWARF names the block's
    // locals `aMat` / `lAcc` and its calls Transpose3x3 + ApplyAxes, which is exactly what the
    // three interleaved fmadds chains at 0x829119D0..0x829119F0 compute.
    if ((luFlags & cParticleDescriptor::E_FLAG_WORLD_ACC) != 0)
    {
        const cMatrix& lrLocatorFrame = mpBindings->GetpLocator()->GetMat(arCurrentLocatorTime);
        const cMatrix  laMat          = lrLocatorFrame.Transpose3x3();
        const cVector  lAcc           = laMat.ApplyAxes(arParticleNucleus.mAcc);
        arParticleNucleus.mAcc        = lAcc;
    }

    // The acceleration's "plus" lane is cleared unconditionally, on BOTH arms of the branch
    // above (0x82911A00: `vrlimi128 v0, v127(zero), 1, 0` -> word 3). DWARF: SetPlus.
    arParticleNucleus.mAcc.w = 0.0f;

    // ---- the animation start frame ---------------------------------------------------------
    // cParticleMaterial::mAnimTexOptions (+0x41) == 1 selects a RANDOM start frame, and it is
    // drawn with the C library's rand() -- NOT the Lion seed (`bl rand` at 0x82911A18 and
    // 0x82911A70, two independent draws). The frame lands in the "plus" lane of mRotAcc and
    // mOffsetRotAcc. Otherwise the two lanes start at 0.0 and 1.0 (0x82911ACC..0x82911AE8; the
    // 1.0 is `vspltisw`+`vcfsx`, which the DWARF names GetVecFloat_One).
    //
    // ⚠ THE MODULO IS THE CONSOLE'S OWN `divw`/`mullw`/`subf` PAIR, TRAPS INCLUDED. `twllei
    // r10, 0` is the compiler's divide-by-zero trap on mFrameCount; a material with
    // mFrameCount == 0 traps on the console and is undefined here. Not guarded, because a
    // guard would be behaviour this build does not have.
    if (lpMat->mAnimTexOptions == 1)
    {
        arParticleNucleus.mRotAcc.w       = static_cast<f32>(std::rand() % lpMat->mFrameCount);
        arParticleNucleus.mOffsetRotAcc.w = static_cast<f32>(std::rand() % lpMat->mFrameCount);
    }
    else
    {
        arParticleNucleus.mRotAcc.w       = 0.0f;
        arParticleNucleus.mOffsetRotAcc.w = 1.0f;
    }

    // ---- inherited emitter velocity (nucleus + 0xC0) ---------------------------------------
    // `lvlx v13, r31, 0x498` + `vspltw` + `vmulfp128` at 0x82911AEC..0x82911B04: the whole
    // velocity vector scaled by ONE splatted behaviour scalar at +0x498 == 1176 ==
    // mEmitterVelWeight (the offset ParticleBehaviour.h's static_assert already pins). The
    // vrlimi that follows preserves the slot's existing "plus" lane, so only xyz are written.
    arParticleNucleus.mLocatorVel.x = arVelocity.x * lrBhv.mEmitterVelWeight;
    arParticleNucleus.mLocatorVel.y = arVelocity.y * lrBhv.mEmitterVelWeight;
    arParticleNucleus.mLocatorVel.z = arVelocity.z * lrBhv.mEmitterVelWeight;

    // ---- the spawn transform ---------------------------------------------------------------
    // DO_IGNORE_ROT (descriptor flag 0x100, `rlwinm r11, r16, 0,23,23` @0x82911B18): the
    // particle keeps the spawn POSITION but not the spawn ORIENTATION.
    if (apParticleMatrix != nullptr)
    {
        if ((luFlags & cParticleDescriptor::E_FLAG_IGNORE_ROT) != 0)
        {
            apParticleMatrix->BuildIdentity();
            apParticleMatrix->SetTrans(arLocator.wa.x, arLocator.wa.y, arLocator.wa.z);
        }
        else
        {
            *apParticleMatrix = arLocator;   // four `lvx128`/`stvx128` row copies
        }
    }

    // The optional per-particle vector slot takes the spawn POSITION -- arLocator's w row,
    // whole (`lvx128 v0, r15, r17(0x30)` / `stvx128 v0, r0, r11` @0x82911BC4). Note it is
    // copied from arLocator, NOT from apParticleMatrix, so DO_IGNORE_ROT does not change it.
    if (apParticleVector != nullptr)
    {
        *apParticleVector = arLocator.wa;
    }
}

// ================================================================================================
// cParticleEmitter::ParticleInsert  @ 0x829133C8      (DWARF ParticleEmitter.h:303)
//
// Reserve the next free slot in apBucket and initialise the particle that lands in it. Returns
// false when the bucket is full or the allocator refuses, true when a particle was born.
//
// ⭐ THIS IS THE FUNCTION THAT MAKES InitialiseParticle REACHABLE -- it is its only caller. The
// three out-parameters cParticleBucket::AllocateParticle fills (the nucleus and the two optional
// per-particle side-array slots) are handed straight on as InitialiseParticle's first three
// arguments, and the caller's own apMatrix -- the SPAWN transform -- goes in as its `locator`.
// Reading the register shuffle at 0x82913424..0x82913448 is what proves those are two different
// matrices and not one passed twice:
//     r6 <- var_50  (AllocateParticle's cMatrix** out: the bucket's per-particle matrix slot)
//     r7 <- r28     (this function's apMatrix parameter: the spawn transform)
//
// ⚠ THE FULL TEST IS AN EQUALITY, NOT A `>=`. `cmplwi cr6, r11, 0x10 ; beq` at 0x829133F8 --
// the console asks "is the fill position exactly 16", which is what cParticleBucket::IsFull()
// spells. Written that way rather than "tightened" to >=; a bucket whose count ever exceeded 16
// would run past its array on the console too, and hiding that would hide a real defect.
//
// ⚠ auSlot IS PASSED BY VALUE AND ITS ADDRESS IS HANDED TO THE ALLOCATOR. `stw r9, arg_44` at
// 0x829133D8 spills the U32 parameter to its home slot and 0x8291340C passes &arg_44 as
// AllocateParticle's `U32&` -- so the allocator writes the chosen slot index into this
// function's own copy, and the caller never sees it. That is the console's shape, kept.
// ================================================================================================
bool cParticleEmitter::ParticleInsert(cParticleBucket* apBucket,
                                      cMatrix* apMatrix,
                                      const cVector& arVector,
                                      const cTime& arTime,
                                      cParticleRandomSeed& arSeed,
                                      u32 auSlot,
                                      const cTime& arCurrentLocatorTime)
{
    if (apBucket->IsFull())
    {
        return false;
    }

    sParticleNucleus* lpNucleus = nullptr;
    cVector*          lpVector  = nullptr;
    cMatrix*          lpMatrix  = nullptr;
    if (!apBucket->AllocateParticle(auSlot, &lpNucleus, &lpVector, &lpMatrix))
    {
        return false;
    }

    InitialiseParticle(*lpNucleus, lpVector, lpMatrix, *apMatrix, arVector, arSeed,
                       arTime, arCurrentLocatorTime);

    // The bucket remembers when its youngest particle was born; the bucket manager's eviction
    // pass signed-compares against it (`stw r11, 0xC(r31)` at 0x82913454).
    apBucket->SetLatestBirthTime(arTime);
    return true;
}

// ================================================================================================
// cParticleEmitter::IsGenerating  @ 0x8290D538      (DWARF ParticleEmitter.h:186)
//
// Should this emitter emit anything in the window [arStartTime, arEndTime]? Four cheap rejects,
// then the pause / life / repeat schedule.
//
// THE SCHEDULE, store for store:
//   * lPause = arSeed.Build(descriptor.mPauseTime, mPauseTimeVariance)      -- seconds of delay
//     before the emitter starts at all, drawn per call.
//   * lLife  = arSeed.Build(descriptor.mEmitterLifeBase, mEmitterLifeVariance) -- how long it
//     then runs.
//   * Both times are measured from the TRIGGER's start stamp, not from the emitter's:
//     `lwz r11, 4(r28)` at 0x8290D5BC reads cParticleTrigger::mTimeStart.
//
// ⚠ THE TWO TIME PARAMETERS ARE A WINDOW, AND THE ORDER MATTERS. r5 (arStartTime) and r6
// (arEndTime) are subtracted from the same trigger stamp into two separate elapsed values
// (f13 from r5, f12 from r6) and the tail tests BOTH -- an emitter whose whole active window
// falls between two frames still fires, because the "already over" test is on f13 while the
// "not yet begun" rescue is on f12.
//
// ⭐ THE REPEAT PATH SEEDS ITS OWN CLOCK, and it does it through mLastTime. On the first pass
// with a repeating descriptor (DO_REPEAT, flag 0x4) mLastTime is 0, and the console fills it
// with `(S32)(lPause * 3000.0f)` -- flt_820FEC3C, read out of the image as exactly 3000.0,
// which is msfTicksPerSecond. So the repeat clock starts at the END of the pause, expressed in
// ticks. That constant landing on the tick rate the DWARF and cTime's own reciprocal already
// agree on is the third independent confirmation of 3000 Hz.
//
// ⚠ `if (lfElapsedStart > 0.0f) return false;` LOOKS DEAD AND IS NOT WRITTEN AS IF IT WERE. The
// function has already returned false for lfElapsedStart < 0, so the only value that survives
// to the last two tests is exactly 0.0 -- but that is the console's control flow (fcmpu/bgt at
// 0x8290D6FC), not a simplification opportunity, and a future behaviour change to the guard
// above would make it live again.
// ================================================================================================
bool cParticleEmitter::IsGenerating(cParticleRandomSeed& arSeed,
                                    const cTime& arStartTime,
                                    const cTime& arEndTime)
{
    const cParticleTrigger* lpTrigger = mpBindings->GetpTrigger();

    // Four rejects, in the console's own order (0x8290D560..0x8290D590).
    if ((mFlags & KU_FLAG_ACTIVE) == 0)   return false;
    if (mpDescriptor == nullptr)          return false;
    if (lpTrigger == nullptr)             return false;
    if (!lpTrigger->IsRunning())          return false;

    const cParticleDescriptor& lrDescriptor = *mpDescriptor;

    const f32 lfPause = arSeed.Build(lrDescriptor.mPauseTime, lrDescriptor.mPauseTimeVariance);
    const f32 lfLife  = arSeed.Build(lrDescriptor.mEmitterLifeBase,
                                     lrDescriptor.mEmitterLifeVariance);

    // Seconds since the trigger started, minus the pause -- i.e. seconds INTO the emitter's own
    // active life, negative while it is still waiting.
    const s32 liStartTicks = arStartTime.GetTimeDiff(lpTrigger->GetTimeStart());
    const s32 liEndTicks   = arEndTime.GetTimeDiff(lpTrigger->GetTimeStart());
    const f32 lfElapsedStart = static_cast<f32>(liStartTicks) * msfOneOverTicksPerSecond - lfPause;
    const f32 lfElapsedEnd   = static_cast<f32>(liEndTicks)   * msfOneOverTicksPerSecond - lfPause;

    if (lfElapsedStart < 0.0f)
    {
        return false;   // still inside the pause
    }

    if ((lrDescriptor.mFlags & cParticleDescriptor::E_FLAG_REPEAT) != 0)
    {
        const f32 lfRepeat = arSeed.Build(lrDescriptor.mRepeatTime,
                                          lrDescriptor.mRepeatTimeVariance);

        if (mLastTime.GetTicks() == 0)
        {
            // First pass: start the repeat clock at the end of the pause, in ticks.
            mLastTime = cTime(static_cast<u32>(lfPause * msfTicksPerSecond));
        }

        const f32 lfSinceRepeatEnd =
            static_cast<f32>(arEndTime.GetTimeDiff(mLastTime)) * msfOneOverTicksPerSecond;
        if (lfSinceRepeatEnd > lfRepeat)
        {
            mLastTime = arEndTime;     // a new repetition begins here
            return true;
        }

        const f32 lfSinceRepeatStart =
            static_cast<f32>(arStartTime.GetTimeDiff(mLastTime)) * msfOneOverTicksPerSecond;
        return lfSinceRepeatStart < lfLife;
    }

    // Non-repeating.
    if (lrDescriptor.mEmitterLifeInfiniteFlag != 0)  return true;
    if (lfElapsedStart <= lfLife)                    return true;
    if (lfElapsedStart > 0.0f)                       return false;   // see the banner
    return lfElapsedEnd >= 0.0f;
}

// ================================================================================================
// cParticleEmitter::PrecalculateParticleBuildData  @ 0x8290E018   (DWARF ParticleEmitter.h:299)
//
// Work out, ONCE per behaviour change, the constants ParticleBuild would otherwise redo per
// particle per frame, and park them in mPrecalculatedParticleBuildData. Called from
// cParticleEmitter::Init's single-behaviour arm and from Blend @0x8290F730.
//
// ⭐⭐⭐ THIS FUNCTION IS WHY unk_82FAC100 HAD TO BE RECOVERED BEFORE ANYTHING DOWNSTREAM OF IT
// COULD LAND. It is the vector every Lion colour is multiplied by, it has THREE readers in the
// export set and NO writer, and it lives in dynamically-initialised .bss -- so it reads
// 0x00000000 straight out of the image BY DEFINITION. Zero is not a multiply's identity: land
// this with a flagged zero and every particle in the game comes out BLACK, and it reads as a
// shader bug. The previous wave decoded this body and deliberately did not land it for exactly
// that reason, calling "almost certainly 1/255 per lane" a guess. It is no longer a guess:
//
//     tools/re/findinit.py 82FAC100  ->  6 sites; five are 0x829xxxxx game code (this
//                                        function, cParticleBehaviour::Build @0x8290B044 and
//                                        ColourStepsBehaviour::Process x3), the outlier
//                                        0x82C4A128 is the CRT init bank.
//     tools/re/ppcdis.py 0x82C4A110  ->  lfs f0, flt_82010C1C ; stfs to the stack ; lvlx ;
//                                        vspltw v0,v0,0 ; stvx128 v0 -> 0x82FAC100
//     tools/re/x360rd.py 82010C1C 4  ->  0x3B808081 == 0.003921568859368563 == 1/255
//
// So unk_82FAC100 is splat4(1/255): the u8 -> normalised-float colour scale. Its sibling
// unk_82FAC220, recovered the same way through 0x82C4A0B0, is splat4(255.0f) -- the re-pack
// scale cParticleBehaviour::Build uses on the way back.
//
// ⚠ AND THE INIT-ORDER HAZARD WAS CHECKED, NOT ASSUMED. A dynamically-initialised constant is
// only correct if its thunk runs before its readers, and retail does ship init-order bugs whose
// genuinely-0.0 value is the faithful thing to reproduce. Not here: all five readers are at
// 0x829xxxxx, i.e. ordinary game code called long after CRT startup, and none is itself in the
// 0x82C4xxxx init bank. There is no init-order window for a reader to fall into, so 1/255 is
// what every one of them sees, and a named constant is the faithful reconstruction.
//
// ⭐ THE MEMBER NAMES CONFIRM THE ARITHMETIC INDEPENDENTLY, which is the corroboration this
// reconstruction rests on beyond the asm. The DWARF names slot 4 mvDragFactorsVelRotScale and
// the asm fills it from behaviour +0x484/+0x488/+0x48C, which the committed cParticleBehaviour
// record already names mDragFactorVel / mDragFactorRot / mDragFactorScale -- in that order. It
// names slot 2 mvScaleAndProportionalScaleYXAndZX and the asm fills y with
// mSizeXYZBase.y / mSizeXYZBase.x and z with mSizeXYZBase.z / mSizeXYZBase.x -- "proportional
// scale Y-over-X and Z-over-X", exactly. Two names, two offset sets, no assumptions.
//
// ⚠ ONE HOST DIVERGENCE, STATED: mvfOneOverFrameCount is a `vrefp` + two Newton-Raphson
// refinements + a multiply by 1.0f on the console (asm 0x8290E240..0x8290E254), i.e. the VMX
// expansion of `1.0f / x`, correctly rounded to about a ulp rather than exactly. The host
// writes the true divide. That is the PC leaf diverging by construction; the RESULT is the same
// reciprocal to within a ulp and nothing downstream compares it for equality.
//
// ⚠ WHAT THIS DOES NOT WRITE: slot 0, mvDeltaTimeAndCurrentTime. It is per-frame state, filled
// by the simulation step, not a per-behaviour constant.
// ================================================================================================
namespace
{
// unk_82FAC100 -- splat4(1/255). See the banner: CRT thunk @0x82C4A110 <- flt_82010C1C.
// The exact bits matter, so it is written as the image's float, not as `1.0f / 255.0f`.
const f32 KF_COLOUR_U8_TO_UNIT = 0.003921568859368563f;   // 0x3B808081

// flt_820FEC48, loaded twice at 0x8290E038 into both live lanes of slot 3. The DWARF names the
// slot mvOrientStepAndDragFrameRateConstants, so these are the orient step and the drag
// frame-rate reference; the console stores the SAME literal in both.
const f32 KF_ORIENT_STEP           = 0.05f;   // flt_820FEC48 == 0x3D4CCCCD
const f32 KF_DRAG_FRAME_RATE_CONST = 0.05f;   // flt_820FEC48, the second lane

// One `lwz` + four byte extracts + `vcfux` + `vmulfp128 <1/255>` group. The console unpacks the
// packed word LOW BYTE FIRST into lane x -- and E_LION_MEMBER_COLOUR is byte-swapped as a
// 32-BIT VALUE by sLionMemberToken::EndianTwiddle @0x82908B48, so the word's numeric value is
// endian-invariant and its low byte is the FIRST channel on both platforms. On this
// little-endian host that low byte is cColour8::r at +0, which is why this reads by name with
// no shifting at all.
inline void NormaliseColour(rw::math::vpu::Vector3Plus& arOut, const cColour8& arColour)
{
    arOut.x = static_cast<f32>(arColour.r) * KF_COLOUR_U8_TO_UNIT;
    arOut.y = static_cast<f32>(arColour.g) * KF_COLOUR_U8_TO_UNIT;
    arOut.z = static_cast<f32>(arColour.b) * KF_COLOUR_U8_TO_UNIT;
    arOut.w = static_cast<f32>(arColour.a) * KF_COLOUR_U8_TO_UNIT;
}
}  // namespace

void cParticleEmitter::PrecalculateParticleBuildData()
{
    const cParticleBehaviour& lrBehaviour = *mpCurrentBehaviour;   // lwz r11, 0x20C(r3)
    ParticleBuildData& lrData = mPrecalculatedParticleBuildData;

    // --- slot 1: mvAlphaFadeInAndFadeOut (+0x230) -- asm 0x8290E058 / 0x8290E084.
    // Two lane inserts, so the z/w lanes keep whatever the previous behaviour left; that is
    // what a Vector2 member assignment compiles to and it is reproduced, not tidied.
    lrData.mvAlphaFadeInAndFadeOut.x = lrBehaviour.mAlphaFadeIn;
    lrData.mvAlphaFadeInAndFadeOut.y = lrBehaviour.mAlphaFadeOut;

    // --- slot 2: mvScaleAndProportionalScaleYXAndZX (+0x240) -- asm 0x8290E0A4..0x8290E0EC.
    // Plain `fdivs`, no zero guard: a behaviour authored with mSizeXYZBase.x == 0 divides by
    // zero on the console too.
    lrData.mvScaleAndProportionalScaleYXAndZX.x = lrBehaviour.mScale;
    lrData.mvScaleAndProportionalScaleYXAndZX.y =
        lrBehaviour.mSizeXYZBase.y / lrBehaviour.mSizeXYZBase.x;
    lrData.mvScaleAndProportionalScaleYXAndZX.z =
        lrBehaviour.mSizeXYZBase.z / lrBehaviour.mSizeXYZBase.x;

    // --- slot 3: mvOrientStepAndDragFrameRateConstants (+0x250) -- asm 0x8290E108, a full
    // 16-byte store of the vector built at the top of the function: (K, K, 0, 0).
    lrData.mvOrientStepAndDragFrameRateConstants.x = KF_ORIENT_STEP;
    lrData.mvOrientStepAndDragFrameRateConstants.y = KF_DRAG_FRAME_RATE_CONST;
    lrData.mvOrientStepAndDragFrameRateConstants.z = 0.0f;
    lrData.mvOrientStepAndDragFrameRateConstants.w = 0.0f;

    // --- slot 5: mvRGBADiff (+0x270) -- asm 0x8290E110..0x8290E11C. The behaviour's mRGBADiff
    // is already a float vector in 0..255 units (cParticleBehaviour::Build @0x8290B15C writes
    // it as (c1/255 - c0/255) * 255); this normalises it to 0..1.
    lrData.mvRGBADiff.x = lrBehaviour.mRGBADiff.x * KF_COLOUR_U8_TO_UNIT;
    lrData.mvRGBADiff.y = lrBehaviour.mRGBADiff.y * KF_COLOUR_U8_TO_UNIT;
    lrData.mvRGBADiff.z = lrBehaviour.mRGBADiff.z * KF_COLOUR_U8_TO_UNIT;
    lrData.mvRGBADiff.w = lrBehaviour.mRGBADiff.w * KF_COLOUR_U8_TO_UNIT;

    // --- slots 6..8: the three packed colours (+0x280 / +0x290 / +0x2A0) -- asm 0x8290E120,
    // 0x8290E15C and 0x8290E1C0. Note the ORDER: the console reads mRGBA0, then mRGBAVar, then
    // mRGBABase, into the DWARF's mvRGBA0 / mvRGBAVar / mvRGBABase slots.
    NormaliseColour(lrData.mvRGBA0,    lrBehaviour.mRGBA0);
    NormaliseColour(lrData.mvRGBAVar,  lrBehaviour.mRGBAVar);
    NormaliseColour(lrData.mvRGBABase, lrBehaviour.mRGBABase);

    // --- slots 9/10: the animation frame count and its reciprocal (+0x2B0 / +0x2C0) --
    // asm 0x8290E1FC..0x8290E258. `lwz 0x4C(descriptor)` is mpMaterial, `lwz 0x34(material)`
    // is mFrameCount, and the `std`/`lfd`/`fcfid`/`frsp` chain is the SIGNED s32 -> f32
    // convert (mFrameCount is S32). Both slots are splatted across all four lanes.
    const cParticleMaterial& lrMaterial = *mpDescriptor->Material();
    const f32 lfFrameCount = static_cast<f32>(lrMaterial.mFrameCount);

    lrData.mvfFrameCount.x = lfFrameCount;
    lrData.mvfFrameCount.y = lfFrameCount;
    lrData.mvfFrameCount.z = lfFrameCount;
    lrData.mvfFrameCount.w = lfFrameCount;

    const f32 lfOneOverFrameCount = 1.0f / lfFrameCount;
    lrData.mvfOneOverFrameCount.x = lfOneOverFrameCount;
    lrData.mvfOneOverFrameCount.y = lfOneOverFrameCount;
    lrData.mvfOneOverFrameCount.z = lfOneOverFrameCount;
    lrData.mvfOneOverFrameCount.w = lfOneOverFrameCount;

    // --- slot 4: mvDragFactorsVelRotScale (+0x260) -- asm 0x8290E25C..0x8290E280, and LAST in
    // the console's own store order, which is why it is last here. A full 16-byte store, so the
    // w lane is written zero rather than left alone.
    lrData.mvDragFactorsVelRotScale.x = lrBehaviour.mDragFactorVel;
    lrData.mvDragFactorsVelRotScale.y = lrBehaviour.mDragFactorRot;
    lrData.mvDragFactorsVelRotScale.z = lrBehaviour.mDragFactorScale;
    lrData.mvDragFactorsVelRotScale.w = 0.0f;
}

// ================================================================================================
// cParticleEmitter::Blend  @ 0x8290F730        (DWARF ParticleEmitter.h -- Blend)
//
// Pick the behaviour layer this emitter is currently playing. A descriptor with more than one
// behaviour is a STACK of layers, and the effect's SCALER binding chooses a position along that
// stack: an integral position selects one layer outright, a fractional one interpolates the two
// it falls between into mpTempBehaviour.
//
// ⭐ THE SCALER IS THE BLEND AXIS, and that is the one thing the asm makes you work for. At
// 0x8290F774 the chain is mpBindings -> +0x10 -> the float at +0x00, and LionBindings.h already
// names +0x10 mpScaler while ParticleScaler.h (added this wave) says the scaler IS one float.
// So this is `mpBindings->GetpScaler()->GetScale()` -- no offsets, three names.
//
// ⭐ mBlendLast IS A POSITION, WHICH IS WHY ITS SENTINEL IS 100.1253f. cParticleEmitter::Init
// seeds it with flt_820FF0D0 == 100.12529754638672, and the first thing this function does with
// it is `|position - mBlendLast| < 0.01 -> return`. A sentinel that far outside any real stack
// position simply guarantees the first Blend after an Init always runs. The value is not a
// tunable and it is never interpolated toward.
//
// ⭐ THE FOUR fsel PAIRS ARE CLAMPS AND A floorf, de-optimised back to what they mean:
//   0x8290F7BC  fsel f0,  -pos, 0.0, pos          ->  pos = max(pos, 0)
//   0x8290F7D8  fsel f31, (n-1)-pos, pos, n-1     ->  pos = min(pos, behaviourCount - 1)
//   0x8290F818..0x8290F83C and again 0x8290F890..0x8290F8AC -- the classic magic-number floorf:
//       m    = (x >= 0) ? +2^52 : -2^52          (dbl_82001CB0 / dbl_82001CB8)
//       r    = (x - m) + m                        -- round to nearest even
//       frac = (x - r >= 0) ? 0.0 : 1.0           (dbl_82001CA8 / dbl_82001CA0)
//       out  = r - frac                           -- floor
//   All six constants were read out of the image; the two doubles are exactly +/-2^52 and the
//   other two exactly 0.0 and 1.0, which is what makes it floor rather than round or trunc.
//
// ⚠ THE EPSILON IS 0.01, TWICE OVER: flt_82002138 == 0.009999999776482582 and flt_8201FDB8 is
// its exact negation. It is used three times -- "position unchanged", "fraction is 0" and
// "fraction is 1" -- so a blend within 1% of a layer boundary snaps to that layer instead of
// running the interpolator.
//
// ⚠ THE PERFMON BRACKET IS REPRODUCED, in the style ParticleRandomSeed.cpp already uses:
// dword_82FAB63C is LionPerfMon + 4, and LionPerfMon's member order (LionPerfMon.cpp) puts
// miEmitterBlend exactly there.
//
// ⛔ cParticleBehaviour::Lerp @0x8290B1F8 IS NOT RECONSTRUCTED -- 1,530 instructions, a wave of
// its own. It is a LOUD, LOG-ONCE stub in LionRuntimeLinkStubs.cpp rather than an assert,
// deliberately: Blend runs per frame for any multi-layer effect, and an assert here is the
// 840,000-line storm that has starved a harness before. What the miss costs is stated there.
// ================================================================================================
namespace
{
// dword_82FAB63C -- the CPU perfmon handle this body brackets itself with. It is LionPerfMon's
// miEmitterBlend (base 0x82FAB638, second member by declaration order). LionPerfMon has no
// global instance in this tree yet, so the handle is carried here the same way
// cParticleRandomSeed::Update carries guUpdateMonitor.
s32 giEmitterBlendMonitor = -1;   // X360 dword_82FAB63C

// flt_82002138 / flt_8201FDB8 -- the blend snap epsilon and its negation.
const f32 KF_BLEND_EPSILON = 0.009999999776482582f;

// The console walks the behaviour chain link by link FIVE times in this one function
// (0x8290F874, 0x8290F8FC, 0x8290F950, 0x8290F984, 0x8290F9A8), each time with the same
// null-tolerant "advance auCount links" shape. That is one helper inlined five times, so it is
// re-outlined once here. It yields null when the chain is shorter than auCount, which the
// console relies on: mpCurrentBehaviour is then set to null rather than to a wrong layer.
cParticleBehaviour* AdvanceBehaviours(cParticleBehaviour* apHead, u32 auCount)
{
    cParticleBehaviour* lpBehaviour = apHead;
    for (u32 luIndex = 0; luIndex < auCount && lpBehaviour != nullptr; ++luIndex)
    {
        lpBehaviour = lpBehaviour->mpNext.Get();
    }
    return lpBehaviour;
}
}  // namespace

void cParticleEmitter::Blend()
{
    CgsDev::PerfMonCpu::StartMonitor(giEmitterBlendMonitor);

    const cParticleDescriptor& lrDescriptor = *mpDescriptor;
    const s32 liBehaviourCount = lrDescriptor.mBehaviourCount;

    // asm 0x8290F768 -- a single-layer descriptor has nothing to blend.
    if (liBehaviourCount > 1)
    {
        // asm 0x8290F774..0x8290F7A0.
        f32 lfPosition = 0.0f;
        if (mpBindings != nullptr)
        {
            const cParticleScaler* lpScaler = mpBindings->GetpScaler();
            if (lpScaler != nullptr && mpTempBehaviour != nullptr)
            {
                lfPosition = lpScaler->GetScale();
            }
        }

        // asm 0x8290F7BC / 0x8290F7D8 -- clamp into [0, behaviourCount - 1].
        const f32 lfLastLayer = static_cast<f32>(liBehaviourCount - 1);
        if (lfPosition <= 0.0f)
        {
            lfPosition = 0.0f;
        }
        if (lfPosition > lfLastLayer)
        {
            lfPosition = lfLastLayer;
        }

        // asm 0x8290F7DC..0x8290F7F8 -- the position has not moved since the last blend.
        if (fabsf(lfPosition - mBlendLast) >= KF_BLEND_EPSILON)
        {
            cParticleBehaviour* lpHead = lrDescriptor.GetBehaviours();

            const f32 lfFloor  = floorf(lfPosition);
            const s32 liLayer  = static_cast<s32>(lfFloor);
            const s32 liLayerN = liLayer + 1;

            cParticleBehaviour* lpChosen = nullptr;
            f32 lfNewBlendLast = lfPosition;

            if (liLayer >= liBehaviourCount - 1)
            {
                // asm 0x8290F858..0x8290F884 -- at or past the top of the stack: the last layer.
                lpChosen = AdvanceBehaviours(lpHead, static_cast<u32>(liBehaviourCount - 1));
            }
            else
            {
                const f32 lfFraction = lfPosition - lfFloor;

                if (lfFraction <= KF_BLEND_EPSILON && lfFraction >= -KF_BLEND_EPSILON)
                {
                    // asm 0x8290F8E4..0x8290F910 -- within 1% of the lower layer: snap to it.
                    lpChosen = AdvanceBehaviours(lpHead, static_cast<u32>(liLayer));
                    lfNewBlendLast = lfFloor;
                }
                else if (fabsf(1.0f - lfFraction) < KF_BLEND_EPSILON)
                {
                    // asm 0x8290F938..0x8290F964 -- within 1% of the upper layer: snap to it.
                    lpChosen = AdvanceBehaviours(lpHead, static_cast<u32>(liLayerN));
                    lfNewBlendLast = lfFloor + 1.0f;
                }
                else
                {
                    // asm 0x8290F968..0x8290F9C0 -- a genuine interpolation into the temp layer.
                    // No null check on mpTempBehaviour: the console does not make one either.
                    const cParticleBehaviour* lpLo =
                        AdvanceBehaviours(lpHead, static_cast<u32>(liLayer));
                    const cParticleBehaviour* lpHi =
                        AdvanceBehaviours(lpHead, static_cast<u32>(liLayerN));
                    mpTempBehaviour->Lerp(lpLo, lpHi, lfFraction);
                    lpChosen = mpTempBehaviour;
                }
            }

            // asm 0x8290F9C4..0x8290F9D0 -- both the snap arms and the interpolating arm land
            // here: publish the layer, remember where the blend stopped, and re-derive the
            // per-behaviour constants for it.
            mBlendLast = lfNewBlendLast;
            mpCurrentBehaviour = lpChosen;
            PrecalculateParticleBuildData();
        }
    }

    CgsDev::PerfMonCpu::StopMonitor(giEmitterBlendMonitor);
}

// ================================================================================================
// cParticleEmitter::DeInit  @ 0x82913330
//
// Give the emitter's buckets back to the bucket pool, give its blend behaviour back to the
// effect manager's allocator, and re-run Init(nullptr) so the record is pool-clean again.
//
// ⭐ THE BUCKET WALK CAPTURES THE LINK BEFORE FREEING (asm 0x82913360 reads +0x08 into r5, THEN
// calls Free with r4) -- the ordinary use-after-free-safe list drain, and it has to be written
// that way here too because cParticleBucketManager::Free really does recycle the node.
//
// ⚠ THE BEHAVIOUR FREE IS AN INLINED cLionParticleEffectManager::Free(cParticleBehaviour*)
// (DWARF LionParticleEffectManager.h:74). The console reaches the manager's mpAllocator and
// calls the ITaggedAllocator::Free(ptr, 0) virtual through it (`lwz r11,0(r3)` then slot +0xC);
// that is de-inlined back onto the owning method rather than written as a vtable poke.
// ================================================================================================
void cParticleEmitter::DeInit()
{
    cParticleBucket* lpBucket = mpBucket;
    while (lpBucket != nullptr)
    {
        cParticleBucket* lpNext = lpBucket->GetEmitterNext();
        cParticleBucketManager::Instance().Free(lpBucket);
        lpBucket = lpNext;
    }

    if (mpTempBehaviour != nullptr)
    {
        cLionParticleEffectManager::Instance().Free(mpTempBehaviour);
        mpTempBehaviour = nullptr;
    }

    Init(nullptr);
}

// ================================================================================================
// cParticleEmitter::Bind  (DWARF ParticleEmitter.h:158)
//
// NO STANDALONE X360 BODY -- fully inlined at its only call site, cLionParticleEffectManager::
// BindingsAttach @0x82914530, where it is the two stores that follow a successful Register:
//
//     82914580  stw  r29, 0x1FC(r3)      ; emitter->mpBindings = &bindings
//     82914584  stw  r3,  0x64(r29)      ; bindings.m_p_emitter = emitter
//
// Re-outlined onto the owning class rather than left as two offset pokes in the manager (the
// DWARF names the method, and the same pair appears again inside cParticleEmitterManager::
// UnRegister(descriptor,...) @0x82914764 on its re-bind path).
//
// ⭐ IT LANDS NOW BECAUSE ITS CALLER CAN NOW RUN. This was a trap in LionRuntimeLinkStubs.cpp
// under the reasoning that "nothing registers an emitter"; cLionEffectManager::EffectCreate
// @0x829149E8 landing is exactly what changes that, and BindingsAttach reaches here on the
// first effect the game starts.
// ================================================================================================
void cParticleEmitter::Bind(cLionBindings& arBindings)
{
    mpBindings = &arBindings;
    arBindings.SetEmitter(this);
}

// ================================================================================================
// cParticleEmitter::BucketRemove  @0x82909790      (DWARF ParticleEmitter.h:149)
//
// AN EXPORT-SET HOLE: IDA names it in cParticleBucketManager::Free's xrefs_to but emits no
// 0x82909790.json, so it had no ledger row and no pseudocode. Disassembled out of the image
// (tools/re/ppcdis.py); the whole function is 31 instructions and reads:
//
//     r11 = this->mpBucket (0x200)     ; if (!r11) return
//     if (!apBucket) return
//     if (apBucket == r11) {           ; head case
//         this->mpBucket = r11->mpEmitterNext (+8)
//         apBucket->mpEmitterNext = 0 ; apBucket->mpEmitter = 0
//         return
//     }
//     r10 = r11
//     loop: if (r11 == apBucket) goto found
//           r10 = r11 ; r11 = r11->mpEmitterNext ; if (r11) goto loop
//           apBucket->mpEmitterNext = 0 ; apBucket->mpEmitter = 0 ; return   <-- NOT FOUND
//     found: if (r11) r10->mpEmitterNext = r11->mpEmitterNext
//            apBucket->mpEmitterNext = 0 ; apBucket->mpEmitter = 0
//
// ⭐ THE CLEAR HAPPENS ON THE NOT-FOUND PATH TOO. 0x829097E0/E4 stores zero into both fields
// after a walk that fell off the end, so a bucket handed in that this emitter never owned is
// still detached from whatever it thinks it belongs to. That is the console's behaviour and it
// is transcribed; it is also why cParticleBucketManager::Free can call this unconditionally.
//
// ⚠ THE TWO STORES ARE +8 THEN +4, IN THAT ORDER, on every one of the three exits. Field order
// in a clear is not usually load-bearing, but this one is written out as the asm has it because
// the pair is what tells a reader the second field is mpEmitter and not more of the link.
// ================================================================================================
void cParticleEmitter::BucketRemove(cParticleBucket* apBucket)
{
    cParticleBucket* lpNode = mpBucket;
    if (lpNode == 0)
        return;
    if (apBucket == 0)
        return;

    if (apBucket == lpNode)
    {
        // Head of this emitter's bucket list.
        mpBucket = lpNode->GetEmitterNext();
    }
    else
    {
        cParticleBucket* lpPrev = lpNode;
        while (lpNode != apBucket)
        {
            lpPrev = lpNode;
            lpNode = lpNode->GetEmitterNext();
            if (lpNode == 0)
                break;
        }
        if (lpNode != 0)
        {
            lpPrev->SetEmitterNext(lpNode->GetEmitterNext());
        }
    }

    apBucket->SetEmitterNext(0);
    apBucket->ClearEmitter();
}

// ================================================================================================
// cParticleEmitter::SubEmitterInit  @0x829112F0      (DWARF ParticleEmitter.h:245)
//
// AN EXPORT-SET HOLE -- IDA names it in cParticleEmitter::SpawnSubEmitter's xrefs and emits no
// 0x829112F0.json, so it had no ledger row and no pseudocode. Disassembled out of the packed
// .i64 (tools/re/ppcdis.py); 60 instructions, and it is what makes a spawned child emitter
// follow the PARENT PARTICLE that spawned it instead of the effect's locator:
//
//     82911328  mFlags |= 8                            -- the sub-emitter bit
//     82911330  mParentRandomSeed = bucket[+0x10]      -- 8x ld/std == 64 bytes (see below)
//     82911358  memcpy(&mParentEmitterNucleus,         -- 0xE0 == one sParticleNucleus
//                      &bucket->mParticles[auSlot], 0xE0)
//     82911384  f13 = mParentEmitterNucleus.BirthTime()  (nucleus + 0xDC == this + 0x14C)
//     8291138C  f0  = 3000.0f  (flt_820FEC3C)
//     82911394  r4  = (s32)(BirthTime * 3000.0)        -- fmuls / fctidz / stfiwx
//     829113A4  mParentRandomSeed.Offset(r4)
//     829113D8  cParticleBucket::GetpMatrix(bucket, auSlot, &mParentBaseMatrix, arTime)
//
// ⭐ THE OFFSET IS THE PARENT PARTICLE'S BIRTH TIME IN TICKS. 3000.0f is cTime's
// msfTicksPerSecond, and BirthTime is stored in seconds -- so two children spawned off the same
// bucket at different times get DIFFERENT random streams, and a replay of the same particle
// gets the same one. That is the whole point of seeding from the parent rather than the clock.
//
// ⭐ THE SEED COPY IS 64 BYTES FROM bucket + 0x10, AND THAT IS WHAT PINS cParticleBucket'S SEED
// OFFSET. See the note on cParticleBucket::mRandomSeed -- this loop is the attestation that
// corrected it from 0x1C.
//
// ⚠ THE CONSOLE ROUND-TRIPS THE SEED THROUGH A STACK TEMPORARY (this+0x150 -> sp+0x60, Offset
// on the temporary, sp+0x60 -> this+0x150, asm 0x8291135C / 0x829113A8). That is the compiler
// materialising a by-value copy for a by-pointer call, not an observable step: the net effect
// is mParentRandomSeed.Offset(ticks), which is what is written.
//
// ⚠ mParentIndex / mpParentEmitter ARE NOT WRITTEN HERE. The asm touches neither +0x190 nor
// +0x194. Whatever sets the child's parent link, it is not this function; recorded so nobody
// "completes" it by inventing the two stores.
// ================================================================================================
void cParticleEmitter::SubEmitterInit(cParticleBucket* apBucket,
                                      u32 auSlot,
                                      const cTime& arTime)
{
    // mFlags bit 3 -- "this emitter follows a parent particle". cParticleEmitter::Update
    // @0x829153D8 branches on it (`*(this + 496) & 8`) to build its spawn transform from
    // mParentEmitterNucleus instead of from the bindings' locator.
    mFlags |= KU_FLAG_SUB_EMITTER;

    // Inherit the parent bucket's random stream and the parent particle's whole nucleus.
    mParentRandomSeed     = apBucket->GetRandomSeed();
    mParentEmitterNucleus = apBucket->GetNucleus(auSlot);

    // Decorrelate that stream by the parent particle's birth time, in ticks.
    const s32 liBirthTicks =
        static_cast<s32>(mParentEmitterNucleus.BirthTime() * msfTicksPerSecond);
    mParentRandomSeed.Offset(static_cast<u32>(liBirthTicks));

    // And take the parent particle's world transform as this emitter's base.
    apBucket->GetpMatrix(auSlot, &mParentBaseMatrix, arTime);
}

// ================================================================================================
// cParticleEmitter::SpawnSubEmitter  @0x82914640      (DWARF ParticleEmitter.h:230)
//
// Walk this emitter's descriptor's CHILD chain and, for each child, register a sub-emitter and
// point it at the particle in apBucket's slot auSlot:
//
//     8291465C  for (d = mpDescriptor->mpChild (+0x5C); d; d = d->mpNext (+0x54))
//     82914674      if (!apBucket) continue;                -- the guard is INSIDE the loop
//     8291468C      e = cParticleEmitterManager::RegisterSubEmitter(&mSingleton, d)
//     829146A8      if (e) { e->SubEmitterInit(apBucket, auSlot, arTime);
//     829146AC              e->mpBindings = mpBindings;      -- lwz 0x1FC / stw 0x1FC
//     829146B4              mpBindings->m_p_emitter = e; }   -- stw r31, 0x64(r11)
//
// ⭐ THE LAST TWO STORES ARE cParticleEmitter::Bind, INLINED -- the same pair
// cLionParticleEffectManager::BindingsAttach @0x82914580/84 emits. The child is bound to the
// PARENT'S bindings object, so a sub-emitter shares its parent effect's locator/scaler/trigger
// and is torn down by the same BindingsRemove walk.
//
// ⚠ THE NULL-BUCKET TEST IS RE-EVALUATED EVERY ITERATION (0x82914674, inside the loop, with the
// loop's own back-edge at 0x829146C0 jumping to it). apBucket cannot change inside the loop, so
// this is the compiler keeping a loop-invariant test rather than a hoisting opportunity taken --
// transcribed as a `continue` so the control flow reads as the asm does.
// ================================================================================================
void cParticleEmitter::SpawnSubEmitter(cParticleBucket* apBucket,
                                       u32 auSlot,
                                       const cTime& arTime)
{
    for (cParticleDescriptor* lpChild = mpDescriptor->mpChild.Get();
         lpChild != 0;
         lpChild = lpChild->mpNext.Get())
    {
        if (apBucket == 0)
            continue;

        cParticleEmitter* lpChildEmitter =
            cParticleEmitterManager::Instance().RegisterSubEmitter(lpChild);
        if (lpChildEmitter != 0)
        {
            lpChildEmitter->SubEmitterInit(apBucket, auSlot, arTime);

            // cParticleEmitter::Bind, inlined by the console at 0x829146AC/B0/B4.
            lpChildEmitter->Bind(*mpBindings);
        }
    }
}

// ================================================================================================
// THE PER-PARTICLE BEHAVIOUR PROCESSORS  (DWARF ParticleEmitter.cpp:820 / :1023 / :1052)
//
// cParticleEmitter::ParticleBuild @0x82910118 -- the 1,142-instruction simulation kernel -- is the
// ONLY caller of all seven of these; each is `xrefs_to == [ParticleBuild]`. They are file-local
// helper structs in the console's own ParticleEmitter.cpp, which is why they are declared here
// rather than in a header: nothing outside this TU can reach them.
//
// ⭐ `Process` IS STATIC, AND THE REGISTER FRAME IS WHY. The DecFIGS dump prints them as ordinary
// public members, but every one of these structs has NO DATA MEMBERS and the call frames carry no
// `this`: ParticleBuild's call site for BaseColourWithVariance @0x82911238..0x82911258 loads
// r3 = the ParticleBuildData, r4 = the cParticleBehaviour, r5 = the RenderedParticle,
// r6 = the cParticleRandomSeed -- four registers for the DWARF's four parameters, with nothing
// left for an implicit first argument. Same for the other two below.
//
// ⛔ THREE OF SEVEN. SizeBehaviour (108 instructions), DragBehaviour (214), ColourStepsBehaviour
// (147) and MultiFrameBehaviour (308) are NOT written -- 777 instructions still open. They are
// listed here so the next wave counts the family, not the leftovers.
//
// The perf-monitor handles are LionPerfMon members reached off the same file-scope base
// (0x82FAB638) that cParticleEmitter::Blend's giEmitterBlendMonitor uses; LionPerfMon has no
// global instance in this tree yet, so each is carried as its own handle exactly as
// ParticleRandomSeed.cpp already does.
// ================================================================================================
namespace
{
s32 giBaseColourWithVarianceMonitor = -1;   // X360 dword_82FAB660 (LionPerfMon + 0x28)
s32 giAlphaFadeMonitor             = -1;   // X360 dword_82FAB670 (LionPerfMon + 0x38)
s32 giRotationMonitor              = -1;   // X360 dword_82FAB674 (LionPerfMon + 0x3C)
}  // namespace

// ------------------------------------------------------------------------------------------------
// BaseColourWithVarianceBehaviour::Process  @ 0x8290D720   (DWARF ParticleEmitter.cpp:820)
//
// Seed the particle's colour from the behaviour's RGBA base, optionally perturbed by the variance.
// Three-way switch on cParticleBehaviour::mRGBAVarianceMode (+0x264, `lwz r11, 0x264(r30)`):
//     mode 1  -> cParticleRandomSeed::Build(base, diff)      -- a PER-LANE draw (X360
//                sub_8290A648, named by the DWARF at ParticleRandomSeed.h:117)
//     mode 2  -> cParticleRandomSeed::BuildLerp(base, diff)  -- ONE scalar draw lerping the whole
//                colour (X360 @0x8290A7A8)
//     else    -> the base, unperturbed
// The two vector arguments are the same pair in every arm: `lvx128 v1, r31, 0x60` == the build
// data's mvRGBA0 and `lvx128 v2, r31, 0x50` == its mvRGBADiff, so the base/variance roles are
// fixed by the load offsets, not chosen. The result lands whole at particle +0x50 (mvColour) --
// `stvx128 v0, r29, r10` with r10 == 0x50, in both the default arm (0x8290D774) and the shared
// random arm (0x8290D7C4).
// ------------------------------------------------------------------------------------------------
struct BaseColourWithVarianceBehaviour
{
    static void Process(const cParticleEmitter::ParticleBuildData& arData,
                        const cParticleBehaviour& arBehaviour,
                        RenderedParticle& arParticle,
                        cParticleRandomSeed& arSeed);
};

void BaseColourWithVarianceBehaviour::Process(const cParticleEmitter::ParticleBuildData& arData,
                                              const cParticleBehaviour& arBehaviour,
                                              RenderedParticle& arParticle,
                                              cParticleRandomSeed& arSeed)
{
    const s32 liMonitor = giBaseColourWithVarianceMonitor;
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    // The console hands both draws the same two registers it just loaded (`lvx128 v1, r31, 0x60`
    // and `lvx128 v2, r31, 0x50`); Vector3Plus and Vector4 are one and the same 16-byte lane
    // register there, and only the host's type vocabulary distinguishes them, so the four lanes
    // are carried across by name.
    const rw::math::vpu::Vector4 lvBase = { arData.mvRGBA0.x, arData.mvRGBA0.y,
                                            arData.mvRGBA0.z, arData.mvRGBA0.w };
    const rw::math::vpu::Vector4 lvDiff = { arData.mvRGBADiff.x, arData.mvRGBADiff.y,
                                            arData.mvRGBADiff.z, arData.mvRGBADiff.w };

    rw::math::vpu::Vector3Plus lvColour;
    if (arBehaviour.mRGBAVarianceMode == 1)
    {
        lvColour = arSeed.Build(lvBase, lvDiff);
    }
    else if (arBehaviour.mRGBAVarianceMode == 2)
    {
        lvColour = arSeed.BuildLerp(lvBase, lvDiff);
    }
    else
    {
        // The default arm does not draw at all: it stores mvRGBA0 straight through
        // (0x8290D76C `lvx128 v0, r31, r11` with r11 == 0x60, then the same store).
        lvColour = arData.mvRGBA0;
    }

    arParticle.mvColour.x = lvColour.x;
    arParticle.mvColour.y = lvColour.y;
    arParticle.mvColour.z = lvColour.z;
    arParticle.mvColour.w = lvColour.w;

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
}

// ------------------------------------------------------------------------------------------------
// AlphaFadeBehaviour::Process  @ 0x8290D7D8   (DWARF ParticleEmitter.cpp:1023)
//
// Scale the particle's alpha lane by the fade-in / fade-out ramp for its current normalised life.
//
//   life   = particle.mvTimeScaleAndLifeScale.y   (`addi r11, r30, 0x60` then `vspltw v12, v13, 1`)
//   fadeIn = data.mvAlphaFadeInAndFadeOut.x       (`addi r10, r31, 0x10` then `vspltw v13, v13, 0`)
//   fadeOut= data.mvAlphaFadeInAndFadeOut.y       (the same load splatted at lane 1)
//
//        if (fadeIn > life)   alpha = life / fadeIn;
//   else if (life > fadeOut)  alpha = 1 - (life - fadeOut) / (1 - fadeOut);
//   else                      alpha = 1;
//   particle.mvColour.w *= alpha;
//
// ⭐ THE TWO DIVISIONS ARE DE-OPTIMISED BACK. The console has no vector divide: each `/` is a
// `vrefp` reciprocal estimate followed by TWO Newton-Raphson refinements
// (`vnmsubfp`/`vmaddfp` pairs at 0x8290D848..0x8290D860 and 0x8290D888..0x8290D8A0). Per the
// project's strength-reduction rule those are written as the divisions they compute, not
// transcribed as the estimate-and-refine sequence. The `1.0` they refine against is
// `vcfsx(vspltisw(1), 0)` -- an immediate 1 converted to float, not a loaded constant.
//
// ⚠ `vnmsubfp vD, vA, vB, vC` computes vB - vA*vC (raw field order), which is what makes the pair
// an NR step: residual = 1 - r*x, then r = r*residual + r. Reading it in assembler order would
// give a different expression and a different function.
//
// The alpha lane is read and written in place: `vspltw v13, v13, 3` takes the OLD alpha out of
// mvColour, multiplies, and `vrlimi128 v13, v0, 1, 0` (mask 1 == word 3) puts only that lane back
// -- the rgb lanes are untouched.
// ------------------------------------------------------------------------------------------------
struct AlphaFadeBehaviour
{
    static void Process(const cParticleEmitter::ParticleBuildData& arData,
                        RenderedParticle& arParticle);
};

void AlphaFadeBehaviour::Process(const cParticleEmitter::ParticleBuildData& arData,
                                 RenderedParticle& arParticle)
{
    const s32 liMonitor = giAlphaFadeMonitor;
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    const f32 lfLife    = arParticle.mvTimeScaleAndLifeScale.y;
    const f32 lfFadeIn  = arData.mvAlphaFadeInAndFadeOut.x;
    const f32 lfFadeOut = arData.mvAlphaFadeInAndFadeOut.y;

    f32 lfAlpha = 1.0f;
    if (lfFadeIn > lfLife)
    {
        lfAlpha = lfLife / lfFadeIn;
    }
    else if (lfLife > lfFadeOut)
    {
        lfAlpha = 1.0f - (lfLife - lfFadeOut) / (1.0f - lfFadeOut);
    }

    arParticle.mvColour.w = arParticle.mvColour.w * lfAlpha;

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
}

// ------------------------------------------------------------------------------------------------
// RotationBehaviour::Process  @ 0x8290D8F0   (DWARF ParticleEmitter.cpp:1052)
//
// Integrate the particle's rotation for this frame and publish it into the render record. Three
// arms, selected by two bits of cParticleBehaviour::mFlags (+0x2C4):
//
//   mFlags & E_BV_ROT (0x1)        `clrlwi r10, r11, 31` -- the SINGLE-ANGLE arm. Everything is
//                                  splatted from lane 2 (`vspltw ..., 2`) and only word 2 is
//                                  written back, so just the Z angle advances; the render record
//                                  receives (0, 0, angle, frame). This is the billboard-sprite
//                                  spin RenderSprites feeds to FastMatrix33FromEulerXYZ.
//   mFlags & E_BV_ROTVELACC (0x40) `rlwinm r11, r11, 0,25,25` -- the FULL XYZ arm: the whole
//                                  vectors integrate and the whole xyz reaches the record.
//   neither                        the record's rotation lanes are zeroed.
//
// In all three arms the record's W lane is preserved (`vrlimi128 v9/v12/v0, <old>, 1, 0`): it is
// mvRotPlusFrame's frame number, which MultiFrameBehaviour owns, not this one.
//
// ⚠ THE STORE AT THE TAIL IS SHARED BUT ITS TARGET IS NOT. `loc_8290DA00`'s single
// `stvx128 v0, r0, r11` writes the NUCLEUS's mRotVel in the first two arms (r11 == nucleus+0x40)
// and the PARTICLE's mvRotPlusFrame in the third (r11 == particle+0x30). It is one instruction
// doing two different jobs, so it is written out as three explicit stores rather than hoisted.
//
// ⭐ `vperm v9, v0, v0, v7` (0x8290D97C) IS A NO-OP AND ITS SELECTOR NEED NOT BE READ: both
// source registers are the zero vector `vspltisw v0, 0` from 0x8290D93C, so every permutation of
// them is zero whatever unk_82CDA350 holds. The compiler kept a general shuffle whose inputs it
// had already proved constant; the result is simply the zero vector the next two vrlimi128s
// insert into.
//
// ⚠ `vmaddfp128 vD, vA, vB, vD` is the VMX128 THREE-REGISTER accumulate form -- vD = vA*vB + vD
// -- not the classic four-operand raw-field-order shape. IDA prints the accumulator twice (as the
// destination and again as the last source), which is the tell.
// ------------------------------------------------------------------------------------------------
struct RotationBehaviour
{
    static void Process(const cParticleBehaviour& arBehaviour,
                        sParticleNucleus& arNucleus,
                        RenderedParticle& arParticle,
                        const cVector& arDeltaTime);
};

void RotationBehaviour::Process(const cParticleBehaviour& arBehaviour,
                                sParticleNucleus& arNucleus,
                                RenderedParticle& arParticle,
                                const cVector& arDeltaTime)
{
    const s32 liMonitor = giRotationMonitor;
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    if ((arBehaviour.mFlags & cParticleBehaviour::E_BV_ROT) != 0)
    {
        // asm 0x8290D938..0x8290D9A4 -- the single-angle arm; every operand is lane 2.
        const f32 lfNewRot    = arNucleus.mRotVel.z * arDeltaTime.z + arNucleus.mRot.z;
        const f32 lfNewRotVel = arNucleus.mRotAcc.z * arDeltaTime.z + arNucleus.mRotVel.z;

        arParticle.mvRotPlusFrame.x = 0.0f;
        arParticle.mvRotPlusFrame.y = 0.0f;
        arParticle.mvRotPlusFrame.z = lfNewRot;
        // .w (the frame) is carried through untouched.

        arNucleus.mRot.z    = lfNewRot;
        arNucleus.mRotVel.z = lfNewRotVel;
    }
    else if ((arBehaviour.mFlags & cParticleBehaviour::E_BV_ROTVELACC) != 0)
    {
        // asm 0x8290D9B4..0x8290D9EC -- the full XYZ arm; whole-vector integrate.
        const f32 lfNewRotX = arNucleus.mRotVel.x * arDeltaTime.x + arNucleus.mRot.x;
        const f32 lfNewRotY = arNucleus.mRotVel.y * arDeltaTime.y + arNucleus.mRot.y;
        const f32 lfNewRotZ = arNucleus.mRotVel.z * arDeltaTime.z + arNucleus.mRot.z;
        const f32 lfNewRotW = arNucleus.mRotVel.w * arDeltaTime.w + arNucleus.mRot.w;

        const f32 lfNewVelX = arNucleus.mRotAcc.x * arDeltaTime.x + arNucleus.mRotVel.x;
        const f32 lfNewVelY = arNucleus.mRotAcc.y * arDeltaTime.y + arNucleus.mRotVel.y;
        const f32 lfNewVelZ = arNucleus.mRotAcc.z * arDeltaTime.z + arNucleus.mRotVel.z;
        const f32 lfNewVelW = arNucleus.mRotAcc.w * arDeltaTime.w + arNucleus.mRotVel.w;

        arParticle.mvRotPlusFrame.x = lfNewRotX;
        arParticle.mvRotPlusFrame.y = lfNewRotY;
        arParticle.mvRotPlusFrame.z = lfNewRotZ;
        // .w (the frame) is carried through untouched.

        arNucleus.mRot.x = lfNewRotX;
        arNucleus.mRot.y = lfNewRotY;
        arNucleus.mRot.z = lfNewRotZ;
        arNucleus.mRot.w = lfNewRotW;

        arNucleus.mRotVel.x = lfNewVelX;
        arNucleus.mRotVel.y = lfNewVelY;
        arNucleus.mRotVel.z = lfNewVelZ;
        arNucleus.mRotVel.w = lfNewVelW;
    }
    else
    {
        // asm 0x8290D9F0..0x8290DA04 -- no rotation block compiled: zero the record's xyz and
        // keep its frame lane.
        arParticle.mvRotPlusFrame.x = 0.0f;
        arParticle.mvRotPlusFrame.y = 0.0f;
        arParticle.mvRotPlusFrame.z = 0.0f;
    }

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
}
