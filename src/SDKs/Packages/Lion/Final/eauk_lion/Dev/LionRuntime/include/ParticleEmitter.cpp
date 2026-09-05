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
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleWaveForm.h"    // ParticleBuild samples the three position wave forms
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

// X360 dword_82FAB640 (LionPerfMon + 0x8) -- the emitter UPDATE monitor. Same arrangement as
// the behaviour processors' handles below: LionPerfMon has no global instance in this tree yet.
s32 giEmitterUpdateMonitor = -1;

// flt_82F369A8 == 0x39AEC33E == 0.00033333332976326346 == 1/3000, the Lion tick rate. Written
// as the image's float rather than 1.0f/3000.0f because the console multiplies by this exact
// value in cParticleEmitter::Update, ::Emit and ::ParentMatrixCurrentBuild.
const f32 KF_TICKS_TO_SECONDS = 0.00033333332976326346f;

// X360 dword_82FAB644 (LionPerfMon + 0xC) -- the emitter GENERATE monitor.
s32 giEmitterGenerateMonitor = -1;

// flt_820FEC40 == 0.10000000149011612 and flt_82009E10 == 1000.0, both .rdata literals
// cParticleEmitter::Generate loads directly (@0x829151FC and @0x8291520C). The first is a FLOOR
// on the emission rate, not an epsilon; the second is the milliseconds-per-second the emission
// clock is kept in (mNextEmissionTime is ticks/3, and 3000/3 == 1000).
const f32 KF_MIN_EMISSION_RATE       = 0.10000000149011612f;   // flt_820FEC40
const f32 KF_MILLISECONDS_PER_SECOND = 1000.0f;                // flt_82009E10
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
// ✅ SEVEN OF SEVEN (2026-09-05). DragBehaviour::Process @0x8290DBD0 (214 instructions),
// ColourStepsBehaviour::Process @0x8290F9F8 (147) and MultiFrameBehaviour::Process @0x8290FC48
// (308) close the family -- 967 instructions, nothing left open. Each was the last thing
// standing between cParticleEmitter::ParticleBuild and a link.
//
// The perf-monitor handles are LionPerfMon members reached off the same file-scope base
// (0x82FAB638) that cParticleEmitter::Blend's giEmitterBlendMonitor uses; LionPerfMon has no
// global instance in this tree yet, so each is carried as its own handle exactly as
// ParticleRandomSeed.cpp already does.
// ================================================================================================
namespace
{
s32 giBaseColourWithVarianceMonitor = -1;   // X360 dword_82FAB660 (LionPerfMon + 0x28)
s32 giColourStepsMonitor           = -1;   // X360 dword_82FAB664 (LionPerfMon + 0x2C)
s32 giAlphaFadeMonitor             = -1;   // X360 dword_82FAB670 (LionPerfMon + 0x38)
s32 giRotationMonitor              = -1;   // X360 dword_82FAB674 (LionPerfMon + 0x3C)
s32 giSizeMonitor                  = -1;   // X360 dword_82FAB678 (LionPerfMon + 0x40)
s32 giDragMonitor                  = -1;   // X360 dword_82FAB67C (LionPerfMon + 0x44)
s32 giMultiFrameMonitor            = -1;   // X360 dword_82FAB680 (LionPerfMon + 0x48)

// flt_820FEC38, read out of the image (tools/re/x360rd.py 0x820FEC38 -> 0x34000000). It is
// FLT_EPSILON, and DragBehaviour::Process loads it with `lvlx` (the symbol is 8 mod 16, so the
// left-load puts it in lane 0) and splats it. Every "is this channel worth dragging" test in
// that function compares against this.
const f32 KF_DRAG_EPSILON = 1.1920928955078125e-07f;   // flt_820FEC38

// The two multi-frame blend-weight clamps, both real .rdata floats read out of the image
// (`lis r10, flt_820132C8@ha` @0x8290FDE4 / `lis r10, flt_820FEC60@ha` @0x8290FE34, and again
// at 0x8290FF98 / 0x8290FFF0 in the play-over-lifetime arm):
//     flt_820132C8 = 0x3F7FF972 = 0.9998999834060669
//     flt_820FEC60 = 0x3F7FFFEF = 0.9999989867210388
// They keep the fractional part strictly inside one atlas cell: once the weight reaches 0.9999
// it is PINNED at 0.999999 rather than allowed to reach 1.0, which would make BuildUVs' floor
// step to the next cell a frame early.
const f32 KF_FRAME_WEIGHT_LIMIT = 0.9998999834060669f;     // flt_820132C8
const f32 KF_FRAME_WEIGHT_PIN   = 0.9999989867210388f;     // flt_820FEC60
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
                        f32 afDeltaTime);
};

void RotationBehaviour::Process(const cParticleBehaviour& arBehaviour,
                                sParticleNucleus& arNucleus,
                                RenderedParticle& arParticle,
                                f32 afDeltaTime)
{
    const s32 liMonitor = giRotationMonitor;
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    if ((arBehaviour.mFlags & cParticleBehaviour::E_BV_ROT) != 0)
    {
        // asm 0x8290D938..0x8290D9A4 -- the single-angle arm; every operand is lane 2.
        const f32 lfNewRot    = arNucleus.mRotVel.z * afDeltaTime + arNucleus.mRot.z;
        const f32 lfNewRotVel = arNucleus.mRotAcc.z * afDeltaTime + arNucleus.mRotVel.z;

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
        const f32 lfNewRotX = arNucleus.mRotVel.x * afDeltaTime + arNucleus.mRot.x;
        const f32 lfNewRotY = arNucleus.mRotVel.y * afDeltaTime + arNucleus.mRot.y;
        const f32 lfNewRotZ = arNucleus.mRotVel.z * afDeltaTime + arNucleus.mRot.z;
        const f32 lfNewRotW = arNucleus.mRotVel.w * afDeltaTime + arNucleus.mRot.w;

        const f32 lfNewVelX = arNucleus.mRotAcc.x * afDeltaTime + arNucleus.mRotVel.x;
        const f32 lfNewVelY = arNucleus.mRotAcc.y * afDeltaTime + arNucleus.mRotVel.y;
        const f32 lfNewVelZ = arNucleus.mRotAcc.z * afDeltaTime + arNucleus.mRotVel.z;
        const f32 lfNewVelW = arNucleus.mRotAcc.w * afDeltaTime + arNucleus.mRotVel.w;

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

// ------------------------------------------------------------------------------------------------
// SizeBehaviour::Process  @ 0x8290DA20   (DWARF ParticleEmitter.cpp:1104)
//
// Integrate the particle's size for this frame, apply the emitter's scale, and publish it into the
// render record. Three arms, selected by two bits of cParticleBehaviour::mFlags (+0x2C4):
//
//   mFlags & E_BV_SIZE_FULL (0x80)          `rlwinm r10, r11, 0,24,24` -- the FULL XYZ arm: whole
//                                           vectors integrate and the whole xyz reaches the record.
//   mFlags & E_BV_SIZE_PROPORTIONAL (1<<24) `rlwinm r11, r11, 0,7,7` -- only X integrates; Y and Z
//                                           are derived from it through the build data's
//                                           proportional lanes.
//   neither                                 only X integrates and the record gets it uniformly.
//
// The nucleus lanes are mSize (+0x90), mSizeVel (+0xA0) and mSizeAcc (+0xB0); the scale and the two
// proportional ratios are the x/y/z lanes of the build data's mvScaleAndProportionalScaleYXAndZX
// (+0x20), which is what its name says they are. The record's w lane -- mvSizePlusNextFrame's NEXT
// FRAME number -- is preserved in every arm (`vrlimi128 ..., v8/v10, 1, 0`); MultiFrameBehaviour
// owns it.
//
// ⚠ THE SHARED TAIL AGAIN. `loc_8290DBB4`'s single `stvx128 v0, r0, r11` writes the nucleus's
// mSizeVel in all three arms, but r11 is set separately in each (0x8290DA78 / 0x8290DAC0); written
// out per arm rather than hoisted.
//
// ⭐ THE PROPORTIONAL ARM IS THE PROOF THAT THE DELTA TIME IS A BROADCAST. Its `vperm v0, v0, v12,
// v7` takes word 0 from the scaled-size register and word 1 from the register holding
// scaledSize * proportionalYX. The selector unk_82CDA350 reads out of the image as
// { 00 01 02 03 | 14 15 16 17 | 00 01 02 03 | 00 01 02 03 } -- word 0 <- source A word 0, word 1 <-
// source B word 1, words 2 and 3 <- source A word 0 (then both overwritten by the two vrlimi128s).
// Word 1 is therefore lane Y of a register whose only MEANINGFUL lane is X: the integrate above it
// splatted mSize/mSizeVel to lane 0 and multiplied by the UNSPLATTED delta-time register, so lane Y
// carries mSizeVel.x * dt.y + mSize.x. That is the right answer only if dt.y == dt.x. The DWARF
// types the parameter `VecFloat`, which in this tree means one broadcast float lane -- and this
// vperm is what proves the console relies on it. Modelled as the scalar it is.
// ------------------------------------------------------------------------------------------------
struct SizeBehaviour
{
    static void Process(const cParticleBehaviour& arBehaviour,
                        const cParticleEmitter::ParticleBuildData& arData,
                        sParticleNucleus& arNucleus,
                        RenderedParticle& arParticle,
                        f32 afDeltaTime);
};

void SizeBehaviour::Process(const cParticleBehaviour& arBehaviour,
                            const cParticleEmitter::ParticleBuildData& arData,
                            sParticleNucleus& arNucleus,
                            RenderedParticle& arParticle,
                            f32 afDeltaTime)
{
    const s32 liMonitor = giSizeMonitor;
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    // data +0x20 lane x -- the emitter's overall scale, splatted in every arm
    // (`lvx128 v0, r0, <data+0x20>` then `vspltw v11, v0, 0`).
    const f32 lfScale = arData.mvScaleAndProportionalScaleYXAndZX.x;

    if ((arBehaviour.mFlags & cParticleBehaviour::E_BV_SIZE_FULL) != 0)
    {
        // asm 0x8290DA74..0x8290DAB0 -- whole-vector integrate, all four lanes.
        const f32 lfSizeX = arNucleus.mSizeVel.x * afDeltaTime + arNucleus.mSize.x;
        const f32 lfSizeY = arNucleus.mSizeVel.y * afDeltaTime + arNucleus.mSize.y;
        const f32 lfSizeZ = arNucleus.mSizeVel.z * afDeltaTime + arNucleus.mSize.z;
        const f32 lfSizeW = arNucleus.mSizeVel.w * afDeltaTime + arNucleus.mSize.w;

        const f32 lfVelX = arNucleus.mSizeAcc.x * afDeltaTime + arNucleus.mSizeVel.x;
        const f32 lfVelY = arNucleus.mSizeAcc.y * afDeltaTime + arNucleus.mSizeVel.y;
        const f32 lfVelZ = arNucleus.mSizeAcc.z * afDeltaTime + arNucleus.mSizeVel.z;
        const f32 lfVelW = arNucleus.mSizeAcc.w * afDeltaTime + arNucleus.mSizeVel.w;

        arParticle.mvSizePlusNextFrame.x = lfSizeX * lfScale;
        arParticle.mvSizePlusNextFrame.y = lfSizeY * lfScale;
        arParticle.mvSizePlusNextFrame.z = lfSizeZ * lfScale;
        // .w (the next frame) is carried through untouched.

        arNucleus.mSize.x = lfSizeX;
        arNucleus.mSize.y = lfSizeY;
        arNucleus.mSize.z = lfSizeZ;
        arNucleus.mSize.w = lfSizeW;

        arNucleus.mSizeVel.x = lfVelX;
        arNucleus.mSizeVel.y = lfVelY;
        arNucleus.mSizeVel.z = lfVelZ;
        arNucleus.mSizeVel.w = lfVelW;
    }
    else if ((arBehaviour.mFlags & cParticleBehaviour::E_BV_SIZE_PROPORTIONAL) != 0)
    {
        // asm 0x8290DAC8..0x8290DB48 -- X integrates, Y and Z follow it by ratio. Only word 0 of
        // the two nucleus lanes is written back (`vrlimi128 ..., 8, 0` -- mask 8 == word 0).
        const f32 lfSizeX = arNucleus.mSizeVel.x * afDeltaTime + arNucleus.mSize.x;
        const f32 lfVelX   = arNucleus.mSizeAcc.x * afDeltaTime + arNucleus.mSizeVel.x;

        const f32 lfScaledX = lfSizeX * lfScale;
        arParticle.mvSizePlusNextFrame.x = lfScaledX;
        arParticle.mvSizePlusNextFrame.y =
            lfScaledX * arData.mvScaleAndProportionalScaleYXAndZX.y;   // proportional Y:X
        arParticle.mvSizePlusNextFrame.z =
            lfScaledX * arData.mvScaleAndProportionalScaleYXAndZX.z;   // proportional Z:X
        // .w (the next frame) is carried through untouched.

        arNucleus.mSize.x    = lfSizeX;
        arNucleus.mSizeVel.x = lfVelX;
    }
    else
    {
        // asm 0x8290DB4C..0x8290DBB0 -- X integrates and the record gets it in all three lanes
        // (the vperm's sources are the same register here, so every lane is the scaled X).
        const f32 lfSizeX = arNucleus.mSizeVel.x * afDeltaTime + arNucleus.mSize.x;
        const f32 lfVelX  = arNucleus.mSizeAcc.x * afDeltaTime + arNucleus.mSizeVel.x;

        const f32 lfScaledX = lfSizeX * lfScale;
        arParticle.mvSizePlusNextFrame.x = lfScaledX;
        arParticle.mvSizePlusNextFrame.y = lfScaledX;
        arParticle.mvSizePlusNextFrame.z = lfScaledX;
        // .w (the next frame) is carried through untouched.

        arNucleus.mSize.x    = lfSizeX;
        arNucleus.mSizeVel.x = lfVelX;
    }

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
}

// ------------------------------------------------------------------------------------------------
// DragBehaviour::Process  @ 0x8290DBD0   (214 instructions)
//
// Bleed velocity, rotation velocity and size velocity, at a FIXED SIMULATION RATE that is
// independent of the frame rate. That fixed rate is the whole shape of the function and it is
// what the two loop-carried registers are for:
//
//   accumulator = arNucleus.mAcc.w                    (`addi r29, r30, 0x20`, `vspltw v0, v0, 3`)
//   step        = arData.mvOrientStepAndDragFrameRateConstants.y
//                                                     (`addi r4, r31, 0x30`, `vspltw v13, v13, 1`)
//   accumulator += dt;  while (accumulator >= step) { one drag step; accumulator -= step; }
//   arNucleus.mAcc.w = accumulator;                   (`vrlimi128 v0, v8, 1, 0` @0x8290DF0C)
//
// ⭐ THE ACCUMULATOR LIVES IN mAcc's W LANE, and that is not a guess: r29 is fixed at
// nucleus+0x20 for the whole body, lane 3 is the only lane read (`vspltw v0, v0, 3`), and the
// single store at the tail is a `vrlimi128 ..., 1, 0` -- mask 1 == word 3 -- so mAcc.xyz is
// untouched. mAcc is a DWARF `Vector3Plus`; the "Plus" lane is exactly this kind of slot, the
// same way MultiFrameBehaviour below parks its two frame numbers in mRotAcc.w / mOffsetRotAcc.w.
//
// THE THREE CHANNELS, and the flag that picks each arm (cParticleBehaviour::mFlags @+0x2C4, the
// same bits RotationBehaviour and SizeBehaviour above test):
//
//   velocity          always      mVel    (+0x10)   factor mvDragFactorsVelRotScale.x
//   rotation velocity E_BV_ROT    mRotVel (+0x40)   factor .y   -- the SCALAR z arm
//                     E_BV_ROTVELACC        "       factor .y   -- the VECTOR arm
//                     neither     nothing happens
//   size velocity     E_BV_SIZE_FULL mSizeVel(+0xA0) factor .z  -- the VECTOR arm
//                     otherwise            "        factor .z   -- the SCALAR x arm
//
// ⚠ THE VECTOR ARMS AND THE SCALAR ARMS DO DIFFERENT ARITHMETIC, and smoothing that away would
// be a behaviour change, so both are written out:
//     vector:  v -= normalise(v) * min(1, k * |v|^2)     -- removes an ABSOLUTE amount
//     scalar:  s -= s            * min(1, k * |s|)       -- removes a FRACTION
// The vector form's `|v|^2` is `vmsum3fp128 v, v` (xyz only) and its normalise is a `vrsqrtefp`
// plus TWO Newton-Raphson refinements, de-optimised back to the division it computes per the
// project's strength-reduction rule. The scalar form has neither a square nor a reciprocal.
//
// ⚠ AND THE EPSILON GUARD IS NOT UNIFORM EITHER. Three of the four working arms first test
// `any(|channel.xyz| > FLT_EPSILON)` (`vandc` against the 0x80000000 sign mask built by
// `vslw v9, v9` on `vspltisw v9, -1`, then `vcmpgtfp.` read on CR6 bit 2 == "no lane true").
// The size SCALAR arm at 0x8290DE9C has NO such guard -- it drags mSizeVel.x unconditionally.
// That asymmetry is in the binary; a uniform guard here would be an invented arm.
//
// ⚠ THE `vrlimi128 v11, v13, 1, 1` inside each guard is a rotate-by-one insert, so the tested
// register is (|x|, |y|, |z|, |x|) -- the w lane is a duplicate of x, not the channel's own w.
// It changes nothing (an OR over four lanes with one repeated), but reading it as |w| would
// invent a dependency on a lane the DWARF calls spare.
// ------------------------------------------------------------------------------------------------
struct DragBehaviour
{
    static void Process(const cParticleEmitter::ParticleBuildData& arData,
                        const cParticleBehaviour& arBehaviour,
                        sParticleNucleus& arNucleus,
                        f32 afDeltaTime);
};

namespace
{
// `vandc` against the sign mask, then `vcmpgtfp.` read on CR6 bit 2 ("no lane greater"): the
// drag arm runs when ANY of the three channel lanes is above FLT_EPSILON.
inline bool AnyChannelAboveDragEpsilon(const cVector& arV)
{
    return (arV.x < 0.0f ? -arV.x : arV.x) > KF_DRAG_EPSILON
        || (arV.y < 0.0f ? -arV.y : arV.y) > KF_DRAG_EPSILON
        || (arV.z < 0.0f ? -arV.z : arV.z) > KF_DRAG_EPSILON;
}

// v -= normalise(v) * min(1, afFactor * |v|^2), xyz only; the w lane is restored by the
// console's `vrlimi128 v0, v7, 1, 0` and is therefore simply not written here.
inline void DragVectorChannel(cVector& arV, f32 afFactor)
{
    const f32 lfLen2 = arV.x * arV.x + arV.y * arV.y + arV.z * arV.z;   // vmsum3fp128

    f32 lfAmount = afFactor * lfLen2;                                   // vmulfp128
    if (lfAmount > 1.0f)                                                // vminfp against vcfsx 1
    {
        lfAmount = 1.0f;
    }

    // vrsqrtefp + two Newton-Raphson steps == 1/sqrt(len2); written as the division it computes.
    const f32 lfScale = lfAmount / std::sqrt(lfLen2);

    arV.x = arV.x - arV.x * lfScale;
    arV.y = arV.y - arV.y * lfScale;
    arV.z = arV.z - arV.z * lfScale;
}

// s -= s * min(1, afFactor * |s|). No square, no reciprocal -- see the banner.
inline f32 DragScalarChannel(f32 afValue, f32 afFactor)
{
    f32 lfAmount = afFactor * (afValue < 0.0f ? -afValue : afValue);
    if (lfAmount > 1.0f)
    {
        lfAmount = 1.0f;
    }
    return afValue - afValue * lfAmount;
}
}  // namespace

void DragBehaviour::Process(const cParticleEmitter::ParticleBuildData& arData,
                            const cParticleBehaviour& arBehaviour,
                            sParticleNucleus& arNucleus,
                            f32 afDeltaTime)
{
    const s32 liMonitor = giDragMonitor;
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    // data +0x30 lane y -- the fixed drag time step (the member's name says so).
    const f32 lfStep = arData.mvOrientStepAndDragFrameRateConstants.y;

    // asm 0x8290DC14..0x8290DC2C -- the accumulator is read out of mAcc's w lane and advanced
    // BEFORE the loop test, so a step can be taken on the very first frame.
    f32 lfAccumulator = arNucleus.mAcc.w + afDeltaTime;

    while (lfAccumulator >= lfStep)
    {
        // ---- velocity (asm 0x8290DC58..0x8290DCF0) --------------------------------------
        if (AnyChannelAboveDragEpsilon(arNucleus.mVel))
        {
            DragVectorChannel(arNucleus.mVel, arData.mvDragFactorsVelRotScale.x);
        }

        // ---- rotation velocity (asm 0x8290DCF4..0x8290DDEC) ----------------------------
        if ((arBehaviour.mFlags & cParticleBehaviour::E_BV_ROT) != 0)
        {
            // The single-angle arm: only lane z, and the SCALAR form of the drag.
            arNucleus.mRotVel.z =
                DragScalarChannel(arNucleus.mRotVel.z, arData.mvDragFactorsVelRotScale.y);
        }
        else if ((arBehaviour.mFlags & cParticleBehaviour::E_BV_ROTVELACC) != 0)
        {
            if (AnyChannelAboveDragEpsilon(arNucleus.mRotVel))
            {
                DragVectorChannel(arNucleus.mRotVel, arData.mvDragFactorsVelRotScale.y);
            }
        }

        // ---- size velocity (asm 0x8290DDF0..0x8290DED4) ---------------------------------
        if ((arBehaviour.mFlags & cParticleBehaviour::E_BV_SIZE_FULL) != 0)
        {
            if (AnyChannelAboveDragEpsilon(arNucleus.mSizeVel))
            {
                DragVectorChannel(arNucleus.mSizeVel, arData.mvDragFactorsVelRotScale.z);
            }
        }
        else
        {
            // ⚠ NO epsilon guard on this arm -- see the banner.
            arNucleus.mSizeVel.x =
                DragScalarChannel(arNucleus.mSizeVel.x, arData.mvDragFactorsVelRotScale.z);
        }

        lfAccumulator = lfAccumulator - lfStep;   // asm 0x8290DEEC
    }

    arNucleus.mAcc.w = lfAccumulator;   // asm 0x8290DF0C -- w lane only

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
}

// ------------------------------------------------------------------------------------------------
// ColourStepsBehaviour::Process  @ 0x8290F9F8   (147 instructions)
//
// Multiply the particle's colour by the behaviour's colour-STEP ramp, sampled at the particle's
// normalised life. cParticleBehaviour::BuildColourSteps @0x82909100 packed the ramp into the
// parallel arrays mColourStepRGBA[4] (+0x240) / mRGBATime[4] (+0x250) with mColourSteps (+0x260)
// live entries; this reads them back.
//
//   life = arParticle.mvTimeScaleAndLifeScale.y        (`lfs f0, 0x64(r30)`)
//   i    = the first index with mRGBATime[i] >= life, else mColourSteps
//
//   i == 0             -> mColourStepRGBA[0]                     (before the first key)
//   i >= mColourSteps  -> mColourStepRGBA[mColourSteps - 1]      (after the last key)
//   otherwise          -> lerp(mColourStepRGBA[i-1], mColourStepRGBA[i],
//                              (life - mRGBATime[i-1]) / (mRGBATime[i] - mRGBATime[i-1]))
//
//   arParticle.mvColour *= that                                  (`lvx/vmulfp128/stvx` @+0x50)
//
// ⭐ THE SEARCH IS FOUR-WAY UNROLLED IN THE BINARY AND RE-ROLLED HERE. asm 0x8290FA34..0x8290FA78
// is the four-at-a-time body (four `lfs`/`fcmpu`/`bge` pairs into four distinct
// `addi r11, r11, 1|2|3` landing pads) and 0x8290FA84..0x8290FAA8 is the one-at-a-time remainder
// the `blt cr6, loc_8290FA7C` head jumps straight to when mColourSteps < 4. Both compute the same
// index; per the project's de-optimisation rule they are one loop here.
//
// ⭐ THE 1/255 IS READ, NOT ASSUMED. unk_82FAC100 is a dynamically-initialised .bss splat: the
// CRT thunk at 0x82C4A110 does `lfs f0, flt_82010C1C` / `vspltw` / `stvx128 -> 0x82FAC100`, and
// flt_82010C1C reads 0x3B808081 == 0.003921568859368563. That is the SAME constant
// cParticleBehaviour::Build @0x8290B044 already uses under the name KF_COLOUR_U8_TO_UNIT, and the
// lane order is the same one it establishes -- lane0 == word&0xFF, which on this host is the
// cColour8's first named channel (see ParticleBehaviour.cpp's colour banner).
//
// ⚠ THE LERP IS `vmaddfp v0, v0, v13, v12` AT 0x8290FC20, WHICH IS RAW FIELD ORDER: vD=v0,
// vA=v0, vB=v13, vC=v12, i.e. v0 = vA*vC + vB = (c1 - c0)*t + c0. Reading it left to right would
// give (c1-c0)*c0 + t, which is not a colour.
// ------------------------------------------------------------------------------------------------
struct ColourStepsBehaviour
{
    static void Process(const cParticleBehaviour& arBehaviour, RenderedParticle& arParticle);
};

namespace
{
// `clrlwi`/`extrwi`/`srwi` + `vcfux ..., 0` + `vmulfp128 <1/255>`: the four bytes of the packed
// colour, low byte first, as unit floats. Lane order pinned by cParticleBehaviour::Build's own
// unpack at 0x8290B070..0x8290B090.
inline void UnpackColour8ToUnit(const cColour8& arColour, f32 aafOut[4])
{
    aafOut[0] = static_cast<f32>(arColour.r) * 0.003921568859368563f;   // flt_82010C1C
    aafOut[1] = static_cast<f32>(arColour.g) * 0.003921568859368563f;
    aafOut[2] = static_cast<f32>(arColour.b) * 0.003921568859368563f;
    aafOut[3] = static_cast<f32>(arColour.a) * 0.003921568859368563f;
}
}  // namespace

void ColourStepsBehaviour::Process(const cParticleBehaviour& arBehaviour,
                                   RenderedParticle& arParticle)
{
    const s32 liMonitor = giColourStepsMonitor;
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    const u32 luSteps = arBehaviour.mColourSteps;
    const f32 lfLife  = arParticle.mvTimeScaleAndLifeScale.y;

    // The re-rolled four-way-unrolled search (see the banner).
    u32 luIndex = 0;
    while (luIndex < luSteps && arBehaviour.mRGBATime[luIndex] < lfLife)
    {
        ++luIndex;
    }

    f32 lafRamp[4];
    if (luIndex == 0)
    {
        UnpackColour8ToUnit(arBehaviour.mColourStepRGBA[0], lafRamp);
    }
    else if (luIndex >= luSteps)
    {
        UnpackColour8ToUnit(arBehaviour.mColourStepRGBA[luSteps - 1], lafRamp);
    }
    else
    {
        f32 lafLo[4];
        f32 lafHi[4];
        UnpackColour8ToUnit(arBehaviour.mColourStepRGBA[luIndex - 1], lafLo);
        UnpackColour8ToUnit(arBehaviour.mColourStepRGBA[luIndex], lafHi);

        const f32 lfTimeLo = arBehaviour.mRGBATime[luIndex - 1];
        const f32 lfWeight = (lfLife - lfTimeLo) / (arBehaviour.mRGBATime[luIndex] - lfTimeLo);

        for (u32 luLane = 0; luLane < 4; ++luLane)
        {
            lafRamp[luLane] = (lafHi[luLane] - lafLo[luLane]) * lfWeight + lafLo[luLane];
        }
    }

    arParticle.mvColour.x = arParticle.mvColour.x * lafRamp[0];
    arParticle.mvColour.y = arParticle.mvColour.y * lafRamp[1];
    arParticle.mvColour.z = arParticle.mvColour.z * lafRamp[2];
    arParticle.mvColour.w = arParticle.mvColour.w * lafRamp[3];

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
}

// ------------------------------------------------------------------------------------------------
// MultiFrameBehaviour::Process  @ 0x8290FC48   (308 instructions)
//
// ⭐⭐ THIS IS THE FUNCTION THAT PRODUCES THE INTER-FRAME BLEND WEIGHT the whole landed draw path
// is built around. BrnEffects::Utils::BuildUVs @0x822781E0 floors the two frame numbers to pick
// two atlas cells and QuadDraw @0x82282330 writes the fraction into the position's w lane, which
// the Lion pixel program uses to cross-fade them. Both halves were reconstructed months apart and
// this is the third, independent, corner of the same contract:
//
//     arParticle.mvRotPlusFrame.w      = <current frame> + <fraction>
//     arParticle.mvSizePlusNextFrame.w = <next frame>    + <fraction>
//
// and the two INTEGER frame numbers are parked, between frames, in the spare w lanes of two
// nucleus vectors -- mRotAcc.w (+0x50) and mOffsetRotAcc.w (+0x80). Same "Vector3Plus spare lane"
// device DragBehaviour above uses for its time accumulator. The four tail stores at
// 0x8290FFB4..0x82910014 are all `vrlimi128 ..., 1, 0` (mask 1 == word 3), so nothing else in
// those four vectors is touched.
//
// FOUR ARMS, selected by cParticleMaterial::mAnimTexOptions (+0x41). The names are the game's
// own -- the Lion authoring token table's TEX_ANIM_OPTIONS enum, read out of the X360 image at
// 0x82F34E28 (the table LionParticleParser.cpp:228 points at):
//     0 NONE               1 RANDOM_PLAYBACK      2 PLAYBACK_ONCE      3 PLAY_OVER_LIFTIME
// (the misspelling is the shipped table's, not a transcription slip).
//
//   RANDOM_PLAYBACK   accumulate dt into mvLifeTimeAndFrameTimeAndFPSAndBirthTime.y; every
//                     1/FPS seconds draw a NEW frame with the C library's rand() % mFrameCount.
//                     With FLAG_INTERFRAMEBLEND the old "next" becomes the new "current" and only
//                     the next is redrawn (so the pair is always a real transition); without it
//                     the current is redrawn and the next is left alone. The blend fraction is
//                     frameTime * FPS.
//   PLAYBACK_ONCE     frame = particleAge * FPS + <random frame base>, and BOTH the current and
//                     the next frame CLAMP at mFrameCount - 1, so the animation stops on its last
//                     cell instead of wrapping.
//   PLAY_OVER_LIFTIME the cell duration is lifeTime / mFrameCount, i.e. the atlas is stretched
//                     across the particle's whole life; the same accumulate-and-step as
//                     RANDOM_PLAYBACK but the frame number simply increments, and both outputs
//                     clamp at mFrameCount - 1.
//   NONE              frame = (particleAge * FPS + <random frame base>) MODULO mFrameCount --
//                     a free-running wrap, computed as x - trunc(x * (1/frameCount)) * frameCount
//                     (`vrfiz` + `vnmsubfp`). The next frame is current+1 (clamped) only when
//                     FLAG_INTERFRAMEBLEND is set, otherwise it is the SAME cell, which makes the
//                     blend a no-op -- exactly what a material without the blend flag wants.
//
// ⭐ THE "RANDOM FRAME BASE" IS A SEED DRAW, NOT rand(). asm 0x8290FE74 calls sub_8290A438 ==
// cParticleRandomSeed::Build(s32, s32) (the DWARF names it; ParticleRandomSeed.cpp:353 has the
// body) with mFrameBase / mFrameVariance, so it is deterministic per particle. Only the
// RANDOM_PLAYBACK arm uses the C library's rand(), and it does so twice, once per branch --
// both call sites are transcribed rather than hoisted, because they draw at different times.
//
// ⚠ THE `vmaddfp v0, v13, v12, v0` AT 0x8290FE9C IS RAW FIELD ORDER: vD=v0, vA=v13, vB=v12,
// vC=v0 => v0 = vA*vC + vB = age * FPS + frameBase. The left-to-right reading (age*frameBase +
// FPS) has the wrong dimensions and would drift with the atlas size.
//
// ⚠ vrefp + two Newton-Raphson steps appears three times (1/FPS, 1/secondsPerFrame,
// 1/frameCount) and is de-optimised back to the division each computes.
// ------------------------------------------------------------------------------------------------
struct MultiFrameBehaviour
{
    // cParticleMaterial::mAnimTexOptions (+0x41). Names from the shipped TEX_ANIM_OPTIONS enum
    // table at X360 0x82F34E28.
    enum ETexAnimOption
    {
        eTEX_ANIM_NONE              = 0,
        eTEX_ANIM_RANDOM_PLAYBACK   = 1,
        eTEX_ANIM_PLAYBACK_ONCE     = 2,
        eTEX_ANIM_PLAY_OVER_LIFTIME = 3,
    };

    static void Process(const cParticleEmitter::ParticleBuildData& arData,
                        const cParticleMaterial& arMaterial,
                        RenderedParticle& arParticle,
                        sParticleNucleus& arNucleus,
                        cParticleRandomSeed& arSeed,
                        f32 afDeltaTime);
};

void MultiFrameBehaviour::Process(const cParticleEmitter::ParticleBuildData& arData,
                                  const cParticleMaterial& arMaterial,
                                  RenderedParticle& arParticle,
                                  sParticleNucleus& arNucleus,
                                  cParticleRandomSeed& arSeed,
                                  f32 afDeltaTime)
{
    const s32 liMonitor = giMultiFrameMonitor;
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    // data +0x90 -- the atlas cell count as a float (`lvx128 v127, r27, 0x90`).
    const f32 lfFrameCount = arData.mvfFrameCount.x;

    // The two frame numbers, parked in the nucleus's two spare w lanes.
    f32 lfFrame     = arNucleus.mRotAcc.w;         // `vspltw128 v125, <n+0x50>, 3`
    f32 lfNextFrame = arNucleus.mOffsetRotAcc.w;   // `vspltw128 v124, <n+0x80>, 3`

    f32 lfOutFrame;
    f32 lfOutNextFrame;

    if (arMaterial.mAnimTexOptions == eTEX_ANIM_RANDOM_PLAYBACK)
    {
        // asm 0x8290FCB8..0x8290FE64.
        const f32 lfFps = arNucleus.FPS();

        arNucleus.FrameTime() = arNucleus.FrameTime() + afDeltaTime;

        // vrefp + two NR steps == 1/FPS; the seconds one cell lasts.
        if (arNucleus.FrameTime() >= 1.0f / lfFps)
        {
            arNucleus.FrameTime() = arNucleus.FrameTime() - 1.0f / lfFps;

            if ((arMaterial.mFlags & cParticleMaterial::eFLAG_INTERFRAMEBLEND) != 0)
            {
                // The pair walks forward: what was next becomes current, and only next is redrawn.
                lfFrame     = lfNextFrame;
                lfNextFrame = static_cast<f32>(std::rand() % arMaterial.mFrameCount);
            }
            else
            {
                lfFrame = static_cast<f32>(std::rand() % arMaterial.mFrameCount);
            }
        }

        f32 lfWeight = arNucleus.FrameTime() * lfFps;
        if (lfWeight >= KF_FRAME_WEIGHT_LIMIT)
        {
            lfWeight = KF_FRAME_WEIGHT_PIN;
        }

        lfOutFrame     = lfFrame + lfWeight;
        lfOutNextFrame = lfNextFrame + lfWeight;
    }
    else
    {
        // asm 0x8290FE68.. -- the shared head of the other three arms: a per-particle random
        // start cell plus the particle's own age in cells.
        const f32 lfFrameBase =
            static_cast<f32>(arSeed.Build(arMaterial.mFrameBase, arMaterial.mFrameVariance));

        // vmaddfp raw field order: age * FPS + base (see the banner).
        f32 lfCurrent = arParticle.mvTimeScaleAndLifeScale.x * arNucleus.FPS() + lfFrameBase;

        if (arMaterial.mAnimTexOptions == eTEX_ANIM_PLAYBACK_ONCE)
        {
            // asm 0x8290FEA8..0x8290FEE8 -- clamp both outputs on the last cell.
            f32 lfNext = lfCurrent + 1.0f;
            if (lfCurrent >= lfFrameCount)
            {
                lfCurrent = lfFrameCount - 1.0f;
            }
            if (lfNext >= lfFrameCount)
            {
                lfNext = lfFrameCount - 1.0f;
            }
            lfOutFrame     = lfCurrent;
            lfOutNextFrame = lfNext;
        }
        else if (arMaterial.mAnimTexOptions == eTEX_ANIM_PLAY_OVER_LIFTIME)
        {
            // asm 0x8290FEF4..0x82910064 -- the atlas is stretched over the particle's life.
            const f32 lfSecondsPerCell = arNucleus.LifeTime() * arData.mvfOneOverFrameCount.x;

            arNucleus.FrameTime() = arNucleus.FrameTime() + afDeltaTime;
            if (arNucleus.FrameTime() >= lfSecondsPerCell)
            {
                arNucleus.FrameTime() = arNucleus.FrameTime() - lfSecondsPerCell;
                lfFrame = lfFrame + 1.0f;
            }

            // vrefp + two NR steps == 1/secondsPerCell.
            f32 lfWeight = arNucleus.FrameTime() / lfSecondsPerCell;
            if (lfWeight >= KF_FRAME_WEIGHT_LIMIT)
            {
                lfWeight = KF_FRAME_WEIGHT_PIN;
            }

            f32 lfOut     = lfFrame + lfWeight;
            f32 lfOutNext = lfFrame + 1.0f;
            if (lfOut >= lfFrameCount)
            {
                lfOut = lfFrameCount - 1.0f;
            }
            if (lfOutNext >= lfFrameCount)
            {
                lfOutNext = lfFrameCount - 1.0f;
            }
            lfOutFrame     = lfOut;
            lfOutNextFrame = lfOutNext;
        }
        else
        {
            // asm 0x82910068..0x829100D0 -- eTEX_ANIM_NONE: a free-running modulo wrap.
            // `vrfiz` truncates toward zero and `vnmsubfp v0, v13, v0, v9` (raw field order,
            // vD = vB - vA*vC) subtracts trunc(x/frameCount) * frameCount.
            const f32 lfWrapped =
                lfCurrent - std::floor(lfCurrent / lfFrameCount) * lfFrameCount;
            lfCurrent = lfWrapped;

            if ((arMaterial.mFlags & cParticleMaterial::eFLAG_INTERFRAMEBLEND) != 0)
            {
                f32 lfNext = lfCurrent + 1.0f;
                if (lfNext >= lfFrameCount)
                {
                    lfNext = lfFrameCount - 1.0f;
                }
                lfOutNextFrame = lfNext;
            }
            else
            {
                // Without the blend flag the "next" cell IS the current one, so the shader's
                // cross-fade contributes nothing. Reproduced rather than special-cased.
                lfOutNextFrame = lfCurrent;
            }
            lfOutFrame = lfCurrent;
        }
    }

    // asm 0x8290FFB4..0x82910014 -- four `vrlimi128 ..., 1, 0` inserts into word 3 only.
    arNucleus.mRotAcc.w              = lfFrame;
    arNucleus.mOffsetRotAcc.w        = lfNextFrame;
    arParticle.mvRotPlusFrame.w      = lfOutFrame;
    arParticle.mvSizePlusNextFrame.w = lfOutNextFrame;

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
}

// ================================================================================================
// cParticleEmitter::ParticleBuild  @0x82910118      (1,142 instructions -- the largest body in the
//                                                    Lion runtime, and the whole of its per-frame
//                                                    per-particle simulation)
//                                (DWARF ParticleEmitter.cpp:1414 / ParticleEmitter.h:284)
//
// ⭐⭐ THIS FUNCTION HAS BEEN DECLINED BY TWO EARLIER WAVES, both citing "dense VMX128 driven by
// rodata that reads zero". That reasoning was wrong for the same reason it was wrong on the draw
// halves: the constants read zero because they are dynamically-initialised .bss, and
// tools/re/findinit.py -> ppcdis.py -> x360rd.py recovers every one of them. Everything this
// function needed is now a number:
//
//   unk_82FAB7B0 <- CRT thunk 0x82C4A150 : splat4(flt_820037C8) == splat4(-1.0)
//   unk_82FAC140 <- CRT thunk 0x82C4A178 : splat4(flt_82001D9C) == splat4( 2.0)
//   flt_820FEC38  = 0x34000000 == FLT_EPSILON        (the "is this channel worth it" guard)
//   unk_82181510  = (0, 1, 0, 0)                     (the ribbon's fallback direction: UP)
//   unk_82CDA350  = (A.x, B.y, A.x, A.x)             (the life/age gather permute)
//   unk_82000BD0/BE0/BF0 = 1, -1/3!, 1/5! ... 1/23!  (twelve SIN Taylor coefficients)
//   unk_82000C00/C10/C20 = 1, -1/2!, 1/4! ... 1/22!  (twelve COS Taylor coefficients)
//   unk_82000C60         = (pi, 2pi, 1/pi, 1/2pi)    (the range reduction)
//
// AND THE VMX128 LISTING ITSELF WAS MIS-READ, which is the second reason it looked undecodable.
// tools/re/vmx128.py (landed with this change) decodes the raw words: IDA prints the vA operand
// of a VMX128 instruction with its two high bits SWAPPED, so 46 instructions in this function
// name a register in v86..v95 that the function's own __savevmx_124 prologue forbids it from
// touching. Every one of them is really v54..v63. Reading them as printed is why the Euler block
// looked like it referenced undefined registers.
//
// ------------------------------------------------------------------------------------------------
// WHAT IT DOES, in the console's order. Sixteen blocks, each gated by one bit of
// cParticleBehaviour::mFlags (+0x2C4) whose name is the Lion authoring token table's own
// (LionParticleParser.cpp:89-111, read out of the X360 image -- these are not derived names):
//
//   1  life gate            age = currentTime - birthTime; > lifetime -> Dead, < 0 -> NotBornYet
//   2  time scale           a per-particle random draw over (mTimeScale, mTimeScaleVariance)
//   3  DO_REVERSE  0x0008   age = lifetime - age   (the particle plays backwards)
//   4  publish              mvTimeScaleAndLifeScale = (age, age/lifetime, age, age)
//                           mLocatorVel = nucleus.mLocatorVel, w = the scaled delta time
//   5  DO_RADIAL   0x0010   push the particle onto a ring of mRingRadius along a random axis
//   6  pos evaluator        the locator's iLionPosEvaluator, if one is attached, INSTEAD of 7
//   7  integrate            pos += vel * dt * mScale ; vel += acc * dt
//   8  shape 3/6            mPos1 = mPos + vel * orientStep  (the ribbon's second point)
//   9  DO_OFFSETROT 0x0020  rotate the position by an integrating Euler XYZ triple
//   10 DO_WAVEX 0x8000 / DO_WAVEY 0x10000 / DO_WAVEZ 0x20000 -- scale one position lane by a
//                           cParticleWaveForm sampled at the particle's age
//   11 shape 4/7            mPos1 = mPos + mAxisBase
//   12 RotationBehaviour / SizeBehaviour / (DO_DRAG 0x0100) DragBehaviour
//   13 publish position     particle.mPos from either the working vector or the raw nucleus
//   14 BaseColourWithVariance / (mColourSteps) ColourSteps / AlphaFade
//   15 material MULTIFRAME  MultiFrameBehaviour, else zero the two frame lanes
//
// ⭐ THE DWARF NAMES EVERY LOCAL AND THE HELPERS, which is the corroboration this reconstruction
// rests on beyond the asm (ParticleEmitter.cpp:1414-1690): lvfLifeTime / lvfDeltaTime /
// lvfCurrentTime / lvfParticleAge / lvfGlobalTimeScale / lvfScaledDeltaTime / lvAcc /
// p_position_evaluator / lRad / lRadBase / lRadRand / lRot / lRotVel / lRotAcc / lMat /
// lOffsetPos / lWave / p_material -- and it names the Euler helper
// `rw::math::vpu::Matrix44FromEulerXYZ` and the apply `rw::math::vpu::TransformVector`, which is
// exactly what the 700-instruction block reduces to.
//
// ⚠ THE EULER MATRIX WAS *MEASURED*, not assumed from that name. Working the lane gather through
// the stack red zone (0x82910964..0x82910A64) gives the three columns the transform multiplies
// pos.x / pos.y / pos.z by:
//     col_x = ( cb*cc,              cb*sc,              -sb   )
//     col_y = ( sa*sb*cc - ca*sc,   sa*sb*sc + ca*cc,   sa*cb )
//     col_z = ( ca*sb*cc + sa*sc,   ca*sb*sc - sa*cc,   ca*cb )
// which is the standard R = Rz(c) * Ry(b) * Rx(a). The identification of the six trig registers
// falls out of it with nothing left over -- (sa,ca) = (v57,v56) from the FIRST polynomial, whose
// input is `vspltw128 v11, v58, 0` (the X angle); (sb,cb) = (v62,v59) from the second (Y); and
// (sc,cc) = (v20,v19) from the third (Z). Three angles, three polynomial pairs, nine products,
// no spare terms.
//
// ⚠ AND THE SECOND MATRIX IS THE SAME CONSTRUCTION WITH ITS COLUMNS IN DIFFERENT STACK SLOTS.
// The ribbon arm builds a second Euler triple one orient-step ahead and applies it to mPos1; the
// compiler parked its columns at (0xA0, 0x80, 0x70) against the first matrix's (0x70, 0xA0,
// 0x80), so the two multiplies read the same three addresses in a different order. Reading the
// slots as if they were stable is the trap here, and it would silently transpose the rotation.
//
// ⚠ THE SIN/COS IS A TWELVE-TERM TAYLOR SERIES, NOT A CALL. It is inlined SIX times (three
// angles x two matrices) and is what makes this function 1,142 instructions rather than ~450.
// Per the project's inlining-reversal rule it is outlined here into LionSinCos.
// ================================================================================================
namespace
{
s32 giParticleBuildMonitor = -1;   // X360 dword_82FAB648 (LionPerfMon + 0x10)
s32 giRadialMonitor        = -1;   // X360 dword_82FAB684 (LionPerfMon + 0x4C)
s32 giOffsetRotMonitor     = -1;   // X360 dword_82FAB688 (LionPerfMon + 0x50)

// unk_82000C60, .rdata: (pi, 2pi, 1/pi, 1/2pi). Only lanes y and w are used -- the range
// reduction is `x - 2pi * round(x * (1/2pi))`, a `vrfin` (round to nearest) plus a `vnmsubfp`.
const f32 KF_TWO_PI        = 6.2831854820251465f;    // unk_82000C60 lane y
const f32 KF_ONE_OVER_2PI  = 0.15915493667125702f;   // unk_82000C60 lane w

// unk_82000BD0 / BE0 / BF0 -- sin(x) = x + c[0]*x^3 + c[1]*x^5 + ... + c[10]*x^23.
// Read out of the image; they are 1/(2k+1)! with alternating sign, twelve terms.
const f32 KAF_SIN_COEFF[11] = {
    -0.1666666716337204f,        0.008333333767950535f,      -0.00019841270113829523f,
     2.7557318844628753e-06f,   -2.5052107943679403e-08f,     1.6059044372074283e-10f,
    -7.647163609812713e-13f,     2.8114573589663704e-15f,    -8.220635078476521e-18f,
     1.9572941524685808e-20f,   -3.868170297964731e-23f,
};

// unk_82000C00 / C10 / C20 -- cos(x) = 1 + c[0]*x^2 + c[1]*x^4 + ... + c[10]*x^22.
const f32 KAF_COS_COEFF[11] = {
    -0.5f,                       0.0416666679084301f,        -0.0013888889225199819f,
     2.4801587642286904e-05f,   -2.755731998149713e-07f,      2.08767581000302e-09f,
    -1.147074536050896e-11f,     4.7794772561329454e-14f,    -1.5619206814541513e-16f,
     4.110317590937049e-19f,    -8.896790959566848e-22f,
};

// The inlined sin/cos of the DO_OFFSETROT block, outlined (project rule: inlining reversal).
// ⛔ NOT std::sinf/cosf. The console evaluates this exact truncated series on the exact
// range-reduced argument; a library call agrees to about a ulp but is not the same arithmetic,
// and the whole point of the reconstruction is that the numbers are the console's.
void LionSinCos(f32 afAngle, f32& arSin, f32& arCos)
{
    // asm 0x82910540..0x8291056C: x = angle - 2pi * round(angle / 2pi), i.e. fold to [-pi, pi].
    // `vrfin` is round-to-NEAREST (not trunc, not floor), which is what makes the fold symmetric.
    const f32 lfRounded = std::nearbyintf(afAngle * KF_ONE_OVER_2PI);
    const f32 lfX       = afAngle - KF_TWO_PI * lfRounded;

    const f32 lfX2 = lfX * lfX;

    f32 lfSin  = lfX;
    f32 lfCos  = 1.0f;
    f32 lfOdd  = lfX * lfX2;   // x^3, then x^5, x^7, ...
    f32 lfEven = lfX2;         // x^2, then x^4, x^6, ...
    for (u32 luTerm = 0; luTerm < 11; ++luTerm)
    {
        lfSin += KAF_SIN_COEFF[luTerm] * lfOdd;
        lfCos += KAF_COS_COEFF[luTerm] * lfEven;
        lfOdd  = lfOdd * lfX2;
        lfEven = lfEven * lfX2;
    }

    arSin = lfSin;
    arCos = lfCos;
}

// The DWARF names this helper `rw::math::vpu::Matrix44FromEulerXYZ` (ParticleEmitter.cpp:1558);
// its real home is the RenderWare vpu vocabulary, which this tree has not reconstructed, so it
// lives here until that header exists -- do NOT fork a second copy elsewhere.
//
// The three returned vectors are the matrix COLUMNS, in the order TransformVector multiplies
// them by x, y and z (see the banner: the lane gather is what pins them).
struct LionEulerMatrix
{
    cVector mColX;
    cVector mColY;
    cVector mColZ;
};

LionEulerMatrix Matrix44FromEulerXYZ(f32 afX, f32 afY, f32 afZ)
{
    f32 lfSa;
    f32 lfCa;
    f32 lfSb;
    f32 lfCb;
    f32 lfSc;
    f32 lfCc;
    LionSinCos(afX, lfSa, lfCa);
    LionSinCos(afY, lfSb, lfCb);
    LionSinCos(afZ, lfSc, lfCc);

    LionEulerMatrix lMat;
    lMat.mColX.x = lfCb * lfCc;
    lMat.mColX.y = lfCb * lfSc;
    lMat.mColX.z = -lfSb;
    lMat.mColX.w = 0.0f;

    lMat.mColY.x = lfSb * lfSa * lfCc - lfCa * lfSc;
    lMat.mColY.y = lfSb * lfSa * lfSc + lfCa * lfCc;
    lMat.mColY.z = lfCb * lfSa;
    lMat.mColY.w = 0.0f;

    lMat.mColZ.x = lfSb * lfCa * lfCc + lfSa * lfSc;
    lMat.mColZ.y = lfSb * lfCa * lfSc - lfSa * lfCc;
    lMat.mColZ.z = lfCb * lfCa;
    lMat.mColZ.w = 0.0f;
    return lMat;
}

// rw::math::vpu::TransformVector (DWARF ParticleEmitter.cpp:1558 block): the console's
// `col_x * splat(v.x) + col_y * splat(v.y) + col_z * splat(v.z)` (asm 0x82910A80..0x82910AA8).
// The w lane is not produced -- every caller re-inserts the source's own w with a vrlimi128.
void TransformVector(cVector& arOut, const LionEulerMatrix& arMat, const cVector& arVector)
{
    arOut.x = arMat.mColX.x * arVector.x + arMat.mColY.x * arVector.y + arMat.mColZ.x * arVector.z;
    arOut.y = arMat.mColX.y * arVector.x + arMat.mColY.y * arVector.y + arMat.mColZ.y * arVector.z;
    arOut.z = arMat.mColX.z * arVector.x + arMat.mColY.z * arVector.y + arMat.mColZ.z * arVector.z;
}
}  // namespace

cParticleEmitter::EParticleBuildResult cParticleEmitter::ParticleBuild(
        RenderedParticle& arRenderedParticle,
        cParticleRandomSeed& arSeed,
        sParticleNucleus& arSimulatedParticle,
        const cParticleDescriptor& arDes,
        const cParticleBehaviour& arBhv,
        const ParticleBuildData& arParticleBuildData)
{
    const s32 liMonitor = giParticleBuildMonitor;
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    // ---- 1. the life gate (asm 0x82910160..0x829101F8) ------------------------------------
    const f32 lvfLifeTime    = arSimulatedParticle.LifeTime();
    const f32 lvfDeltaTime   = arParticleBuildData.mvDeltaTimeAndCurrentTime.x;
    const f32 lvfCurrentTime = arParticleBuildData.mvDeltaTimeAndCurrentTime.y;

    f32 lvfParticleAge = lvfCurrentTime - arSimulatedParticle.BirthTime();

    if (lvfParticleAge > lvfLifeTime)
    {
        CgsDev::PerfMonCpu::StopMonitor(liMonitor);
        return eParticleBuildResultDead;
    }
    if (0.0f > lvfParticleAge)
    {
        CgsDev::PerfMonCpu::StopMonitor(liMonitor);
        return eParticleBuildResultNotBornYet;
    }

    // ---- 2/3. the per-particle time scale, and DO_REVERSE (asm 0x829101FC..0x82910244) -----
    const f32 lvfGlobalTimeScale = arSeed.Build(arBhv.mTimeScale, arBhv.mTimeScaleVariance);
    lvfParticleAge = lvfParticleAge * lvfGlobalTimeScale;
    const f32 lvfScaledDeltaTime = lvfDeltaTime * lvfGlobalTimeScale;

    if ((arBhv.mFlags & cParticleBehaviour::E_DO_REVERSE) != 0)
    {
        lvfParticleAge = lvfLifeTime - lvfParticleAge;
    }

    // ---- 4. publish the age / life fraction and the locator velocity -----------------------
    // asm 0x82910248..0x8291029C. The reciprocal is a `vrefp128` plus two Newton-Raphson
    // refinements, de-optimised back to the division it computes; the four lanes are laid out
    // by `vperm` through unk_82CDA350 == (A.x, B.y, A.x, A.x), so the age lands in three of them
    // and the fraction in one. The DWARF types the member Vector2 -- only x and y are read --
    // but the store is a whole register and is reproduced as one.
    const f32 lfLifeFraction = lvfParticleAge / lvfLifeTime;
    arRenderedParticle.mvTimeScaleAndLifeScale.x = lvfParticleAge;
    arRenderedParticle.mvTimeScaleAndLifeScale.y = lfLifeFraction;
    arRenderedParticle.mvTimeScaleAndLifeScale.z = lvfParticleAge;
    arRenderedParticle.mvTimeScaleAndLifeScale.w = lvfParticleAge;

    // asm 0x829102A0..0x829102AC -- copied whole, then the w lane overwritten.
    arRenderedParticle.mLocatorVel   = arSimulatedParticle.mLocatorVel;
    arRenderedParticle.mLocatorVel.w = lvfScaledDeltaTime;

    // ---- 5. DO_RADIAL (asm 0x829102B0..0x82910388) -----------------------------------------
    // The acceleration the integrate below uses. Normally the nucleus's own; under DO_RADIAL it
    // is redirected onto the random axis with everything else.
    cVector lvAcc;
    if ((arBhv.mFlags & cParticleBehaviour::E_DO_RADIAL) != 0)
    {
        const s32 liRadialMonitor = giRadialMonitor;
        CgsDev::PerfMonCpu::StartMonitor(liRadialMonitor);

        // splat4(-1) and splat4(2): a per-lane uniform in [-1, 1). Both are .bss splats the
        // CRT fills (see the banner) -- they are numbers, not flagged zeros.
        const cVector lRadBase = { -1.0f, -1.0f, -1.0f, -1.0f };   // unk_82FAB7B0
        const cVector lRadVar  = {  2.0f,  2.0f,  2.0f,  2.0f };   // unk_82FAC140
        cVector lRadRand;
        arSeed.Build(lRadRand, lRadBase, lRadVar);

        // asm 0x8291031C..0x8291035C -- the axis is the per-lane PRODUCT of the draw and the
        // particle's velocity, then normalised (vmsum3fp128 + vrsqrtefp + two NR steps).
        cVector lRad = { lRadRand.x * arSimulatedParticle.mVel.x,
                         lRadRand.y * arSimulatedParticle.mVel.y,
                         lRadRand.z * arSimulatedParticle.mVel.z,
                         lRadRand.w * arSimulatedParticle.mVel.w };
        const f32 lfLen2   = lRad.x * lRad.x + lRad.y * lRad.y + lRad.z * lRad.z;
        const f32 lfInvLen = 1.0f / std::sqrt(lfLen2);
        lRad.x *= lfInvLen;
        lRad.y *= lfInvLen;
        lRad.z *= lfInvLen;
        lRad.w *= lfInvLen;

        // asm 0x82910360..0x82910384. mRingRadius is a vector, so the offset is per-lane; the
        // velocity and the acceleration are then scaled by the same axis, again per-lane.
        // Every store re-inserts the destination's own w lane (vrlimi128 ..., 1, 0).
        const cVector lvPos = arSimulatedParticle.mPos;
        const cVector lvVel = arSimulatedParticle.mVel;
        arSimulatedParticle.mPos.x = lRad.x * arBhv.mRingRadius.x + lvPos.x;
        arSimulatedParticle.mPos.y = lRad.y * arBhv.mRingRadius.y + lvPos.y;
        arSimulatedParticle.mPos.z = lRad.z * arBhv.mRingRadius.z + lvPos.z;

        arSimulatedParticle.mVel.x = lvVel.x * lRad.x;
        arSimulatedParticle.mVel.y = lvVel.y * lRad.y;
        arSimulatedParticle.mVel.z = lvVel.z * lRad.z;

        lvAcc.x = arSimulatedParticle.mAcc.x * lRad.x;
        lvAcc.y = arSimulatedParticle.mAcc.y * lRad.y;
        lvAcc.z = arSimulatedParticle.mAcc.z * lRad.z;
        lvAcc.w = lvfParticleAge;

        CgsDev::PerfMonCpu::StopMonitor(liRadialMonitor);
    }
    else
    {
        // asm 0x82910390..0x829103A0.
        lvAcc   = arSimulatedParticle.mAcc;
        lvAcc.w = lvfParticleAge;
    }

    // ---- 6/7. the position evaluator, or the integrate (asm 0x829103A4..0x82910418) --------
    iLionPosEvaluator* lpPositionEvaluator = mpBindings->GetpLocator()->GetpPosEvaluator();
    if (lpPositionEvaluator != 0)
    {
        // asm 0x829103C0..0x829103E0. The three vectors ride in v1/v2/v3 (by value -- a
        // reference would have consumed a GPR and none is set), the two scalars in f1/f2, and
        // the console DISCARDS the result and skips the integrate entirely. Reproduced as it
        // is: an attached evaluator owns the particle's motion.
        lpPositionEvaluator->Evaluate(lvfParticleAge,
                                      arRenderedParticle.mvTimeScaleAndLifeScale.y,
                                      arSimulatedParticle.mPos,
                                      arSimulatedParticle.mVel,
                                      lvAcc);
    }
    else
    {
        // asm 0x829103E8..0x82910418. The velocity step is scaled by the behaviour's overall
        // SCALE (build data slot 2 lane x == cParticleBehaviour::mScale, the authoring token
        // table's SCALE at +0x2BC), so a scaled-up effect moves proportionally faster.
        const f32 lfStep = lvfScaledDeltaTime * arParticleBuildData.mvScaleAndProportionalScaleYXAndZX.x;
        const cVector lvPos = arSimulatedParticle.mPos;
        const cVector lvVel = arSimulatedParticle.mVel;

        arSimulatedParticle.mPos.x = lvVel.x * lfStep + lvPos.x;
        arSimulatedParticle.mPos.y = lvVel.y * lfStep + lvPos.y;
        arSimulatedParticle.mPos.z = lvVel.z * lfStep + lvPos.z;

        arSimulatedParticle.mVel.x = lvAcc.x * lvfScaledDeltaTime + lvVel.x;
        arSimulatedParticle.mVel.y = lvAcc.y * lvfScaledDeltaTime + lvVel.y;
        arSimulatedParticle.mVel.z = lvAcc.z * lvfScaledDeltaTime + lvVel.z;
    }

    // ---- 8. the ribbon's second point (asm 0x8291041C..0x829104C8) -------------------------
    // mShape 3 and 6 are the two ribbon/tilt shapes; RenderTilts @0x82282FC8 is the draw half
    // that consumes mPos -> mPos1 as a segment, which is what this pair is for.
    const u32 luShape = arDes.mShape;
    if (luShape == 3 || luShape == 6)
    {
        const cVector& lrVel = arSimulatedParticle.mVel;
        const f32 lfVelLen2 = lrVel.x * lrVel.x + lrVel.y * lrVel.y + lrVel.z * lrVel.z;

        cVector lvTip;
        if (lfVelLen2 > KF_DRAG_EPSILON)
        {
            // The step is the build data's ORIENT STEP (slot 3 lane x, 0.05s).
            const f32 lfOrientStep = arParticleBuildData.mvOrientStepAndDragFrameRateConstants.x;
            lvTip.x = lrVel.x * lfOrientStep + arSimulatedParticle.mPos.x;
            lvTip.y = lrVel.y * lfOrientStep + arSimulatedParticle.mPos.y;
            lvTip.z = lrVel.z * lfOrientStep + arSimulatedParticle.mPos.z;
        }
        else
        {
            // unk_82181510 == (0, 1, 0, 0): a stationary particle's ribbon points straight up.
            lvTip.x = arSimulatedParticle.mPos.x + 0.0f;
            lvTip.y = arSimulatedParticle.mPos.y + 1.0f;
            lvTip.z = arSimulatedParticle.mPos.z + 0.0f;
        }
        arRenderedParticle.mPos1.x = lvTip.x;
        arRenderedParticle.mPos1.y = lvTip.y;
        arRenderedParticle.mPos1.z = lvTip.z;   // .w kept
    }

    // ---- 9. DO_OFFSETROT (asm 0x829104CC..0x82910FA4) --------------------------------------
    // lOffsetPos (DWARF :1549) is the working position from here to the publish at the tail: it
    // starts as the nucleus position and is replaced by the rotated one, then possibly scaled
    // lane by lane by the three wave forms.
    cVector lOffsetPos = arSimulatedParticle.mPos;

    if ((arBhv.mFlags & cParticleBehaviour::E_DO_OFFSETROT) != 0)
    {
        const s32 liOffsetRotMonitor = giOffsetRotMonitor;
        CgsDev::PerfMonCpu::StartMonitor(liOffsetRotMonitor);

        // asm 0x82910504..0x82910518 -- integrate the offset rotation, exactly as
        // RotationBehaviour::Process integrates the particle's own.
        cVector lRot    = arSimulatedParticle.mOffsetRot;
        cVector lRotVel = arSimulatedParticle.mOffsetRotVel;
        const cVector lRotAcc = arSimulatedParticle.mOffsetRotAcc;

        lRot.x = lRotVel.x * lvfScaledDeltaTime + lRot.x;
        lRot.y = lRotVel.y * lvfScaledDeltaTime + lRot.y;
        lRot.z = lRotVel.z * lvfScaledDeltaTime + lRot.z;
        lRot.w = lRotVel.w * lvfScaledDeltaTime + lRot.w;

        lRotVel.x = lRotAcc.x * lvfScaledDeltaTime + lRotVel.x;
        lRotVel.y = lRotAcc.y * lvfScaledDeltaTime + lRotVel.y;
        lRotVel.z = lRotAcc.z * lvfScaledDeltaTime + lRotVel.z;
        lRotVel.w = lRotAcc.w * lvfScaledDeltaTime + lRotVel.w;

        // asm 0x8291051C..0x82910AAC -- the first matrix, applied to the nucleus position.
        const LionEulerMatrix lMat = Matrix44FromEulerXYZ(lRot.x, lRot.y, lRot.z);
        TransformVector(lOffsetPos, lMat, arSimulatedParticle.mPos);

        if (luShape == 3 || luShape == 6)
        {
            // asm 0x82910ABC..0x82910F84 -- a SECOND Euler triple, one orient step ahead, applied
            // to the ribbon tip this function already wrote into mPos1. The step is the build
            // data's orient step again.
            const f32 lfOrientStep = arParticleBuildData.mvOrientStepAndDragFrameRateConstants.x;
            const cVector lTipRot = { lRotVel.x * lfOrientStep + lRot.x,
                                      lRotVel.y * lfOrientStep + lRot.y,
                                      lRotVel.z * lfOrientStep + lRot.z,
                                      lRotVel.w * lfOrientStep + lRot.w };

            const LionEulerMatrix lTipMat =
                Matrix44FromEulerXYZ(lTipRot.x, lTipRot.y, lTipRot.z);
            cVector lvTip;
            TransformVector(lvTip, lTipMat, arRenderedParticle.mPos1);
            arRenderedParticle.mPos1.x = lvTip.x;
            arRenderedParticle.mPos1.y = lvTip.y;
            arRenderedParticle.mPos1.z = lvTip.z;   // .w kept
        }

        // asm 0x82910F88..0x82910FA0 -- both integrated triples written back, w lanes preserved.
        arSimulatedParticle.mOffsetRot.x = lRot.x;
        arSimulatedParticle.mOffsetRot.y = lRot.y;
        arSimulatedParticle.mOffsetRot.z = lRot.z;

        arSimulatedParticle.mOffsetRotVel.x = lRotVel.x;
        arSimulatedParticle.mOffsetRotVel.y = lRotVel.y;
        arSimulatedParticle.mOffsetRotVel.z = lRotVel.z;

        CgsDev::PerfMonCpu::StopMonitor(liOffsetRotMonitor);
    }

    // ---- 10. the three wave forms (asm 0x82910FA8..0x82911154) -----------------------------
    // ⚠ EACH ONE MULTIPLIES THE *NUCLEUS* LANE, NOT THE WORKING ONE. The console reloads
    // `lvx128 v0, r0, r31` (the nucleus) for every axis and writes the product into
    // lOffsetPos's lane, so a wave form on an axis DISCARDS whatever DO_OFFSETROT put there.
    // That is the binary's behaviour and smoothing it into `lOffsetPos.x *= wave` would be a
    // different function.
    if ((arBhv.mFlags & cParticleBehaviour::E_DO_WAVEX) != 0 && arBhv.mpWaveFormX.Get() != 0)
    {
        const f32 lWave = arBhv.mpWaveFormX.Get()->Evaluate(lvfParticleAge);
        lOffsetPos.x = arSimulatedParticle.mPos.x * lWave;
        arRenderedParticle.mPos1.x = arRenderedParticle.mPos1.x * lWave;
    }
    if ((arBhv.mFlags & cParticleBehaviour::E_DO_WAVEY) != 0 && arBhv.mpWaveFormY.Get() != 0)
    {
        const f32 lWave = arBhv.mpWaveFormY.Get()->Evaluate(lvfParticleAge);
        lOffsetPos.y = arSimulatedParticle.mPos.y * lWave;
        arRenderedParticle.mPos1.y = arRenderedParticle.mPos1.y * lWave;
    }
    if ((arBhv.mFlags & cParticleBehaviour::E_DO_WAVEZ) != 0 && arBhv.mpWaveFormZ.Get() != 0)
    {
        const f32 lWave = arBhv.mpWaveFormZ.Get()->Evaluate(lvfParticleAge);
        lOffsetPos.z = arSimulatedParticle.mPos.z * lWave;
        arRenderedParticle.mPos1.z = arRenderedParticle.mPos1.z * lWave;
    }

    // ---- 11. shapes 4 and 7 (asm 0x82911158..0x82911184) -----------------------------------
    if (luShape == 4 || luShape == 7)
    {
        arRenderedParticle.mPos1.x = arSimulatedParticle.mPos.x + arBhv.mAxisBase.x;
        arRenderedParticle.mPos1.y = arSimulatedParticle.mPos.y + arBhv.mAxisBase.y;
        arRenderedParticle.mPos1.z = arSimulatedParticle.mPos.z + arBhv.mAxisBase.z;   // .w kept
    }

    // ---- 12. rotation / size / drag (asm 0x82911188..0x829111D4) ---------------------------
    RotationBehaviour::Process(arBhv, arSimulatedParticle, arRenderedParticle, lvfScaledDeltaTime);
    SizeBehaviour::Process(arBhv, arParticleBuildData, arSimulatedParticle, arRenderedParticle,
                           lvfScaledDeltaTime);
    if ((arBhv.mFlags & cParticleBehaviour::E_DO_DRAG) != 0)
    {
        DragBehaviour::Process(arParticleBuildData, arBhv, arSimulatedParticle,
                               lvfScaledDeltaTime);
    }

    // ---- 13. publish the position (asm 0x829111D8..0x82911254) -----------------------------
    // The working vector only reaches the render record if something actually wrote into it;
    // otherwise the raw nucleus position is published. The four bits are re-tested here, in the
    // console's own order, rather than tracked with a flag.
    const u32 luFlags = arBhv.mFlags;
    if ((luFlags & cParticleBehaviour::E_DO_OFFSETROT) != 0 ||
        (luFlags & cParticleBehaviour::E_DO_WAVEX) != 0 ||
        (luFlags & cParticleBehaviour::E_DO_WAVEY) != 0 ||
        (luFlags & cParticleBehaviour::E_DO_WAVEZ) != 0)
    {
        arRenderedParticle.mPos.x = lOffsetPos.x;
        arRenderedParticle.mPos.y = lOffsetPos.y;
        arRenderedParticle.mPos.z = lOffsetPos.z;   // .w from the record, then overwritten below
    }
    else
    {
        arRenderedParticle.mPos.x = arSimulatedParticle.mPos.x;
        arRenderedParticle.mPos.y = arSimulatedParticle.mPos.y;
        arRenderedParticle.mPos.z = arSimulatedParticle.mPos.z;
    }

    // GetVecFloat_One / the vspltisw128-0 register: the two segment endpoints' w lanes are the
    // constants 1 and 0, written unconditionally at the end of every build.
    arRenderedParticle.mPos.w  = 1.0f;
    arRenderedParticle.mPos1.w = 0.0f;

    // ---- 14. colour (asm 0x82911258..0x8291127C) -------------------------------------------
    BaseColourWithVarianceBehaviour::Process(arParticleBuildData, arBhv, arRenderedParticle,
                                             arSeed);
    if (arBhv.mColourSteps != 0)
    {
        ColourStepsBehaviour::Process(arBhv, arRenderedParticle);
    }
    AlphaFadeBehaviour::Process(arParticleBuildData, arRenderedParticle);

    // ---- 15. the frame animation (asm 0x82911280..0x829112CC) ------------------------------
    cParticleMaterial* lpMaterial = arDes.Material();
    if ((lpMaterial->mFlags & cParticleMaterial::eFLAG_MULTIFRAME) != 0)
    {
        MultiFrameBehaviour::Process(arParticleBuildData, *lpMaterial, arRenderedParticle,
                                     arSimulatedParticle, arSeed, lvfScaledDeltaTime);
    }
    else
    {
        // No atlas: both frame lanes are zero, which makes BuildUVs pick cell 0 twice and the
        // shader's cross-fade a no-op.
        arRenderedParticle.mvRotPlusFrame.w      = 0.0f;
        arRenderedParticle.mvSizePlusNextFrame.w = 0.0f;
    }

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
    return eParticleBuildResultAlive;
}

// ================================================================================================
// cParticleEmitter::Update  @0x829153D8      (201 instructions)
//                           (DWARF ParticleEmitter.h -- Update; ParticleEmitter.cpp:1224)
//
// ⭐⭐ THE HEAD OF THE LION SIMULATION. cParticleEmitterManager::Update @0x82915700 calls this
// once per registered emitter per frame, and it is the only thing that advances an effect.
// Until this landed the whole runtime was inert: emitters were created, linked onto the used
// list, and never stepped -- which is exactly the state LionRuntimeLinkStubs.cpp described.
//
// WHAT IT DOES, store for store:
//   1  age            m_age = (time - trigger->mTimeStart) * (1/3000)   -- seconds since the
//                     effect's trigger fired, NOT since the emitter was created
//   2  mDt            = clamp(m_age - <previous m_age>, 0, 1)
//   3  (mFlags & 2)   -> stamp mUpdateLastTime and return 1, doing nothing else
//   4  locator        lMatrix = mpBindings->GetpLocator()->GetMat(time)
//   5  (mFlags & 8)   the SUB-EMITTER arm (see below); otherwise straight to 6
//   6  (mFlags & 1)   -> Generate(time), else mFlags &= ~4
//   7                 mUpdateLastTime = time; return 1
//
// THE SUB-EMITTER ARM is the interesting half, and it is why ParticleBuild has a caller here
// at all. A sub-emitter follows a PARENT PARTICLE, so before it can emit anything it has to
// know where that particle is *now* -- and the parent particle is not stored anywhere, it is
// RE-SIMULATED from the parent's nucleus and a copy of the parent's random seed:
//
//   copy mParentRandomSeed  -> a local (the seed must not be advanced in place, or the parent
//                              particle would follow a different path every frame)
//   parentDes = mpDescriptor->mpParent
//   if      (parentDes->mFlags & E_FLAG_NEEDS_BUCKET) leave mParentBaseMatrix alone
//   else if (parentDes->mFlags & E_FLAG_IGNORE_ROT)   mParentBaseMatrix = identity with the
//                                                     locator's TRANSLATION only
//   else                                              mParentBaseMatrix = the locator matrix
//   buildData.mvDeltaTimeAndCurrentTime = (mDt, time * (1/3000))
//   if (ParticleBuild(...) == Alive) { mParentVel = 0; mParentTime = time; ...continue... }
//   else                             { return the emitter's total live particle count }
//
// ⚠ THE RETURN VALUE IS NOT A BOOLEAN. It is 1 on every normal path, and on the "my parent
// particle is dead" path it is the SUM of mnNextParticlePositionToFill over this emitter's
// whole bucket list (asm 0x82911690..0x829116B0). cParticleEmitterManager::Update unregisters
// the emitter when it comes back 0 -- i.e. a bereaved sub-emitter survives exactly as long as
// it still has particles of its own on screen. Returning `false` there would kill every
// sub-emitter effect the instant its parent expired.
//
// ⚠ THE IDENTITY ARM'S LAST STORE IS A vsel, NOT A COPY. asm 0x82915550..0x8291556C builds row
// 3 as `vsel(splat(1.0), locatorRow3, unk_820FEBD0)` with unk_820FEBD0 == (FFFFFFFF, FFFFFFFF,
// FFFFFFFF, 00000000) -- the xyz-keep / w-drop selector, whose next quadword is splat(1.0).
// So the translation comes from the locator and the w lane is forced to 1: a position-only
// transform. (Classic `vsel vD, vA, vB, vC` prints raw field order and means vD = vC ? vB : vA,
// which is what makes the mask lanes select the LOCATOR and the clear lane select the 1.0.)
//
// ⚠ THE TWO fsel PAIRS ARE A CLAMP, de-optimised back. `fsel fD, fA, fC, fB` is fD = (fA >= 0)
// ? fC : fB, and the console writes it as max-then-min against flt_82001CC0 == 0.0 and
// flt_82001C98 == 1.0 (the same pair BuildUVs' non-atlas path uses). A frame longer than one
// second therefore simulates as one second, and a backwards clock as zero -- both are the
// console's own guards, not defensive code added here.
// ================================================================================================
u32 cParticleEmitter::Update(const cTime& arTime)
{
    const s32 liMonitor = giEmitterUpdateMonitor;
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    const u32 luFlags = mFlags;
    const f32 lfPreviousAge = m_age;

    // asm 0x82915424..0x8291545C -- the age is measured from the TRIGGER's start stamp.
    const s32 liElapsedTicks = arTime.GetTicks() - mpBindings->GetpTrigger()->GetTimeStart().GetTicks();
    const f32 lfAge = static_cast<f32>(liElapsedTicks) * KF_TICKS_TO_SECONDS;
    m_age = lfAge;

    // asm 0x82915460..0x82915474 -- clamp(age - previousAge, 0, 1).
    f32 lfDeltaTime = lfAge - lfPreviousAge;
    if (lfDeltaTime <= 0.0f)
    {
        lfDeltaTime = 0.0f;
    }
    if (lfDeltaTime > 1.0f)
    {
        lfDeltaTime = 1.0f;
    }
    mDt = lfDeltaTime;

    if ((luFlags & KU_FLAG_FROZEN) == 0)
    {
        // asm 0x8291547C..0x829154A8 -- the locator's transform for this frame, copied whole.
        cMatrix lLocatorMatrix = mpBindings->GetpLocator()->GetMat(arTime);

        if ((mFlags & KU_FLAG_SUB_EMITTER) != 0)
        {
            // asm 0x829154BC..0x829154DC -- the parent's seed is COPIED, never advanced in
            // place: the parent particle has to replay identically every frame.
            cParticleRandomSeed lParentSeed = mParentRandomSeed;

            const cParticleDescriptor& lrParentDes = *mpDescriptor->mpParent.Get();
            if ((lrParentDes.mFlags & cParticleDescriptor::E_FLAG_NEEDS_BUCKET) == 0)
            {
                if ((lrParentDes.mFlags & cParticleDescriptor::E_FLAG_IGNORE_ROT) != 0)
                {
                    // asm 0x82915508..0x8291556C -- identity, then the locator's translation
                    // with w forced to 1 (see the banner's vsel note).
                    mParentBaseMatrix.xa = { 1.0f, 0.0f, 0.0f, 0.0f };
                    mParentBaseMatrix.ya = { 0.0f, 1.0f, 0.0f, 0.0f };
                    mParentBaseMatrix.za = { 0.0f, 0.0f, 1.0f, 0.0f };
                    mParentBaseMatrix.wa = { lLocatorMatrix.wa.x,
                                             lLocatorMatrix.wa.y,
                                             lLocatorMatrix.wa.z,
                                             1.0f };
                }
                else
                {
                    // asm 0x82915574..0x829155AC -- all four rows.
                    mParentBaseMatrix = lLocatorMatrix;
                }
            }

            // asm 0x829155B0..0x829155D0 -- the working seed the build consumes. The console
            // makes a second copy (the locator matrix's stack slot is dead by now and gets
            // reused for it); reproduced as the one copy it is.
            cParticleRandomSeed lSeed = lParentSeed;

            // asm 0x829155D4..0x8291563C -- this frame's delta time and absolute time, pushed
            // into the two live lanes of build-data slot 0. PrecalculateParticleBuildData
            // deliberately leaves that slot alone (it is per-frame, not per-behaviour); this
            // is the only writer.
            mPrecalculatedParticleBuildData.mvDeltaTimeAndCurrentTime.x = mDt;
            mPrecalculatedParticleBuildData.mvDeltaTimeAndCurrentTime.y =
                static_cast<f32>(arTime.GetTicks()) * KF_TICKS_TO_SECONDS;

            // asm 0x82915640..0x82915654 -- re-simulate the parent particle into a scratch
            // render record. Only its EFFECT on mParentEmitterNucleus matters here; the
            // record itself is a local the console never reads back.
            RenderedParticle lParentParticle;
            const EParticleBuildResult leResult =
                ParticleBuild(lParentParticle, lSeed, mParentEmitterNucleus,
                              *mpDescriptor->mpParent.Get(), *mpCurrentBehaviour,
                              mPrecalculatedParticleBuildData);

            if (leResult != eParticleBuildResultAlive)
            {
                // asm 0x82911690..0x829116B0 -- the parent particle is gone. Report how many
                // particles this emitter still owns; the manager retires it at zero.
                u32 luLiveParticles = 0;
                for (const cParticleBucket* lpBucket = mpBucket;
                     lpBucket != 0;
                     lpBucket = lpBucket->GetEmitterNext())
                {
                    luLiveParticles += lpBucket->GetNumParticles();
                }
                CgsDev::PerfMonCpu::StopMonitor(liMonitor);
                return luLiveParticles;
            }

            // asm 0x82915658..0x8291566C.
            mParentVel.x = 0.0f;
            mParentVel.y = 0.0f;
            mParentVel.z = 0.0f;
            mParentVel.w = 0.0f;
            mParentTime = arTime;
        }

        // asm 0x82915670..0x8291568C / 0x829156D0.
        if ((mFlags & KU_FLAG_ACTIVE) != 0)
        {
            Generate(arTime);
        }
        else
        {
            mFlags &= ~KU_FLAG_EMITTING;
        }
    }

    // asm 0x829156D8..0x829156E8.
    mUpdateLastTime = arTime;
    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
    return 1;
}

// ================================================================================================
// cParticleEmitter::ParentMatrixCurrentBuild  @0x829113E8      (175 instructions)
//                                             (DWARF ParticleEmitter.h -- ParentMatrixCurrentBuild)
//
// Produce the world transform of the PARENT PARTICLE a sub-emitter is following, right now. The
// parent particle is stored nowhere, so this re-simulates it (ParticleBuild against a COPY of
// the parent seed) and then builds an orthonormal basis out of the result:
//
//   forward = (parentDes->mShape is 3 or 6) ? (particle.mPos1 - particle.mPos)
//                                           : mParentEmitterNucleus.mVel      normalised
//   right   = normalise(cross(forward, (0,1,0)))          unk_82181510 is the world UP
//   up      = cross(right, forward)
//   out     = { right, up, forward, (particle.mPos, 1) } * mParentBaseMatrix
//
// ⚠ THE ARGUMENT LIST HAS A HOLE IN IT, and it is the documented f32-eats-a-GPR-slot pattern.
// The callers set r3 (this), r4 (the out matrix), r5 (a cTime), f1 (the delta time) and r7 (a
// second cTime) -- but NOT r6. That is not a bug: an f32 parameter takes f1 and consumes the r6
// slot, so r7 is the fourth declared parameter. cParticleEmitter::Emit @0x82914D70 sets only r4,
// r7 and f1 and lets r5 ride in from its own argument, which is what proves r6 is not a
// parameter at all. ⚠ And r7 is read by NOTHING in the body -- stated rather than dropped, the
// same call QuadDraw's fifth parameter got.
//
// ⚠ THE TWO CROSS PRODUCTS ARE THE `vpermwi128 ..., 0x63` YZX IDIOM: a*yzx(b) - yzx(a)*b, whose
// result is itself yzx-rotated and permuted back by the next vpermwi128. `vnmsubfp vD, vA, vB,
// vC` prints raw field order and means vD = vB - vA*vC, which is what makes the pair a cross
// product rather than a scaled difference. Both normalises are vrsqrtefp + two Newton-Raphson
// steps, de-optimised back to the division they compute.
//
// ⚠ EVERY BASIS ROW IS MASKED WITH unk_820FEBD0 == (FFFFFFFF, FFFFFFFF, FFFFFFFF, 00000000)
// before it is stored -- the w lane is forced to zero on the three axis rows and to 1.0 on the
// translation row (flt_82001C98). A basis row with a stray w would translate on multiply.
// ================================================================================================
void cParticleEmitter::ParentMatrixCurrentBuild(cMatrix& arOutMatrix,
                                                const cTime& arTime,
                                                f32 afDeltaTime,
                                                const cTime& /*arCurrentTime*/)
{
    // asm 0x82911400..0x82911420 -- the parent's seed is COPIED, never advanced in place.
    cParticleRandomSeed lSeed = mParentRandomSeed;

    const cParticleDescriptor& lrParentDes = *mpDescriptor->mpParent.Get();

    // asm 0x82911444..0x82911494 -- this frame's delta time and absolute time.
    mPrecalculatedParticleBuildData.mvDeltaTimeAndCurrentTime.x = afDeltaTime;
    mPrecalculatedParticleBuildData.mvDeltaTimeAndCurrentTime.y =
        static_cast<f32>(arTime.GetTicks()) * KF_TICKS_TO_SECONDS;

    RenderedParticle lParticle;
    ParticleBuild(lParticle, lSeed, mParentEmitterNucleus, lrParentDes, *mpCurrentBehaviour,
                  mPrecalculatedParticleBuildData);

    // asm 0x829114A8..0x829114D8 -- the forward axis.
    cVector lvForward;
    if (lrParentDes.mShape == 3 || lrParentDes.mShape == 6)
    {
        lvForward.x = lParticle.mPos1.x - lParticle.mPos.x;
        lvForward.y = lParticle.mPos1.y - lParticle.mPos.y;
        lvForward.z = lParticle.mPos1.z - lParticle.mPos.z;
        lvForward.w = lParticle.mPos1.w - lParticle.mPos.w;
    }
    else
    {
        lvForward = mParentEmitterNucleus.mVel;
    }

    // asm 0x829114DC..0x82911560 -- normalise it.
    {
        const f32 lfLen2 = lvForward.x * lvForward.x + lvForward.y * lvForward.y
                         + lvForward.z * lvForward.z;
        const f32 lfInv  = 1.0f / std::sqrt(lfLen2);
        lvForward.x *= lfInv;
        lvForward.y *= lfInv;
        lvForward.z *= lfInv;
        lvForward.w *= lfInv;
    }

    // asm 0x82911564..0x829115AC -- right = normalise(cross(forward, worldUp)), where worldUp
    // is unk_82181510 == (0, 1, 0, 0).
    const cVector lvWorldUp = { 0.0f, 1.0f, 0.0f, 0.0f };
    cVector lvRight = { lvForward.y * lvWorldUp.z - lvForward.z * lvWorldUp.y,
                        lvForward.z * lvWorldUp.x - lvForward.x * lvWorldUp.z,
                        lvForward.x * lvWorldUp.y - lvForward.y * lvWorldUp.x,
                        0.0f };
    {
        const f32 lfLen2 = lvRight.x * lvRight.x + lvRight.y * lvRight.y + lvRight.z * lvRight.z;
        const f32 lfInv  = 1.0f / std::sqrt(lfLen2);
        lvRight.x *= lfInv;
        lvRight.y *= lfInv;
        lvRight.z *= lfInv;
    }

    // asm 0x829115C0..0x829115CC -- up = cross(right, forward).
    const cVector lvUp = { lvRight.y * lvForward.z - lvRight.z * lvForward.y,
                           lvRight.z * lvForward.x - lvRight.x * lvForward.z,
                           lvRight.x * lvForward.y - lvRight.y * lvForward.x,
                           0.0f };

    // asm 0x829115B4..0x829115F4 -- the basis, w lanes masked to 0 and the translation to 1.
    cMatrix lBasis;
    lBasis.xa = { lvRight.x,   lvRight.y,   lvRight.z,   0.0f };
    lBasis.ya = { lvUp.x,      lvUp.y,      lvUp.z,      0.0f };
    lBasis.za = { lvForward.x, lvForward.y, lvForward.z, 0.0f };
    lBasis.wa = { lParticle.mPos.x, lParticle.mPos.y, lParticle.mPos.z, 1.0f };

    // asm 0x829115F8..0x8291169C -- out = lBasis * mParentBaseMatrix, all four rows.
    const cMatrix& lrBase = mParentBaseMatrix;
    const cVector* lapRow[4] = { &lBasis.xa, &lBasis.ya, &lBasis.za, &lBasis.wa };
    cVector* lapOut[4] = { &arOutMatrix.xa, &arOutMatrix.ya, &arOutMatrix.za, &arOutMatrix.wa };
    for (u32 luRow = 0; luRow < 4; ++luRow)
    {
        const cVector& lrR = *lapRow[luRow];
        lapOut[luRow]->x = lrR.x * lrBase.xa.x + lrR.y * lrBase.ya.x
                         + lrR.z * lrBase.za.x + lrR.w * lrBase.wa.x;
        lapOut[luRow]->y = lrR.x * lrBase.xa.y + lrR.y * lrBase.ya.y
                         + lrR.z * lrBase.za.y + lrR.w * lrBase.wa.y;
        lapOut[luRow]->z = lrR.x * lrBase.xa.z + lrR.y * lrBase.ya.z
                         + lrR.z * lrBase.za.z + lrR.w * lrBase.wa.z;
        lapOut[luRow]->w = lrR.x * lrBase.xa.w + lrR.y * lrBase.ya.w
                         + lrR.z * lrBase.za.w + lrR.w * lrBase.wa.w;
    }
}

// ================================================================================================
// cParticleEmitter::Emit  @0x82914D38      (173 instructions)
//
// Emit ONE particle. Work out the spawn transform and the velocity to inherit, find a bucket
// with a free slot (or allocate one), initialise the particle in it, and spawn any child
// emitters that follow it.
//
//   mEmissionCount++                                                 -- unconditional, first
//   if (mFlags & KU_FLAG_SUB_EMITTER)  lMatrix = ParentMatrixCurrentBuild(...)
//   else                               lMatrix = *locator->GetMat(arTime)
//   lVelocity = lMatrix.translation + <the emitter's own velocity> * elapsedSeconds
//   for (b = mpBucket; b; b = b->GetEmitterNext())
//       if (!b->IsFull() && b->AllocateParticle(...)) { InitialiseParticle(...);
//                                                       b->SetLatestBirthTime(arSpawnTime);
//                                                       goto spawned; }
//   b = manager.AllocateBucket(descriptor->mLodGroup, arSpawnTime, descriptor->GetRequiredBucketType())
//   if (!b) return;                       -- out of buckets: the particle is simply not born
//   link b onto this emitter; ParticleInsert(b, ...)
// spawned:
//   SpawnSubEmitter(b, 0, arTime)
//
// ⚠ THE VELOCITY TERM IS SCALED BY THE TIME SINCE A DIFFERENT STAMP IN EACH ARM. The sub-emitter
// arm measures from mParentTime (`lwz r9, 0x198`) and the locator arm from arTime itself
// (`lwz r9, 0(r28)`), both against arSpawnTime -- so a burst emitted between frames gets the
// parent's motion since the parent was last rebuilt, and a locator-driven one gets zero on the
// frame it is emitted. That asymmetry is in the binary.
//
// ⚠ THE LOCATOR ARM READS THE VELOCITY OFF THE LOCATOR (`lfs f13/f12/f11, 0/4/8(r29+0x40)` ==
// cParticleLocator::mVel), the sub-emitter arm off the EMITTER (`this + 0x50` == mParentVel,
// which cParticleEmitter::Update zeroes on every frame the parent rebuild succeeds).
//
// ⚠ SpawnSubEmitter IS CALLED ON EVERY PATH THAT PRODUCED A BUCKET -- including the one where
// AllocateParticle failed on an existing bucket and ParticleInsert filled a fresh one -- but NOT
// when AllocateBucket returned null. The `goto`/label shape is reproduced with an early return.
// ================================================================================================
void cParticleEmitter::Emit(cParticleRandomSeed& arSeed,
                            const cTime& arSpawnTime,
                            const cTime& arTime)
{
    ++mEmissionCount;

    cMatrix lMatrix;
    cVector lvVelocity;

    if ((mFlags & KU_FLAG_SUB_EMITTER) != 0)
    {
        // asm 0x82914D70..0x82914DF4.
        ParentMatrixCurrentBuild(lMatrix, arSpawnTime, mDt, arTime);

        const f32 lfElapsed =
            static_cast<f32>(arSpawnTime.GetTicks() - mParentTime.GetTicks()) * KF_TICKS_TO_SECONDS;
        lvVelocity.x = mParentVel.x * lfElapsed + lMatrix.wa.x;
        lvVelocity.y = mParentVel.y * lfElapsed + lMatrix.wa.y;
        lvVelocity.z = mParentVel.z * lfElapsed + lMatrix.wa.z;
    }
    else
    {
        // asm 0x82914DF8..0x82914EB0.
        const cParticleLocator& lrLocator = *mpBindings->GetpLocator();
        lMatrix = lrLocator.GetMat(arTime);

        const f32 lfElapsed =
            static_cast<f32>(arSpawnTime.GetTicks() - arTime.GetTicks()) * KF_TICKS_TO_SECONDS;
        lvVelocity.x = lrLocator.mVel.x * lfElapsed + lMatrix.wa.x;
        lvVelocity.y = lrLocator.mVel.y * lfElapsed + lMatrix.wa.y;
        lvVelocity.z = lrLocator.mVel.z * lfElapsed + lMatrix.wa.z;
    }

    // asm 0x82914EB4..0x82914ED8 -- the spawn point is the velocity-advanced translation, with
    // w == 1 (flt_82001C98). The console writes it back over the matrix's own translation row.
    lMatrix.wa.x = lvVelocity.x;
    lMatrix.wa.y = lvVelocity.y;
    lMatrix.wa.z = lvVelocity.z;
    lMatrix.wa.w = 1.0f;

    // asm 0x82914EDC..0x82914F68 -- walk this emitter's bucket list for a free slot.
    cParticleBucket* lpBucket = mpBucket;
    for (; lpBucket != 0; lpBucket = lpBucket->GetEmitterNext())
    {
        if (lpBucket->IsFull())
        {
            continue;
        }

        u32 luSlot = 0;
        sParticleNucleus* lpNucleus = 0;
        cVector* lpVector = 0;
        cMatrix* lpMatrix = 0;
        if (!lpBucket->AllocateParticle(luSlot, &lpNucleus, &lpVector, &lpMatrix))
        {
            continue;
        }

        InitialiseParticle(*lpNucleus, lpVector, lpMatrix, lMatrix, lvVelocity, arSeed,
                           arSpawnTime, arTime);
        lpBucket->SetLatestBirthTime(arSpawnTime);
        SpawnSubEmitter(lpBucket, 0, arTime);
        return;
    }

    // asm 0x82914F6C..0x82914FD0 -- no free slot anywhere: take a fresh bucket and link it on.
    const cParticleDescriptor& lrDes = *mpDescriptor;
    cParticleBucket* lpNewBucket =
        cParticleBucketManager::Instance().AllocateBucket(lrDes.mLodGroup, arSpawnTime,
                                                          lrDes.GetRequiredBucketType());
    if (lpNewBucket == 0)
    {
        return;
    }

    lpNewBucket->SetEmitterNext(mpBucket);
    mpBucket = lpNewBucket;
    lpNewBucket->SetEmitter(this);
    ParticleInsert(lpNewBucket, &lMatrix, lvVelocity, arSpawnTime, arSeed, 0, arTime);

    SpawnSubEmitter(lpNewBucket, 0, arTime);
}

// ================================================================================================
// cParticleEmitter::Generate  @0x82915158      (159 instructions)
//
// Decide how many particles this frame emits, and call Emit for each. Update's only real work.
//
//   if (!IsGenerating(<a COPY of mEmitterSeed>, mUpdateLastTime, arTime))
//       { mFlags &= ~KU_FLAG_EMITTING; mEmissionCount = 0;
//         mNextEmissionTime = arTime / 3; return; }
//   Blend()                                            -- pick the behaviour layer first
//   rate     = seedCopy.Build(mEmissionRateBase, mEmissionRateVariance)
//   interval = 1000 / max(rate, 0.1)                   -- milliseconds between particles
//   if (behaviour & DO_BURST) { emit the whole clamp at once, once; return }
//   if (descriptor & DO_PREFORM && !EMITTING) back-date mNextEmissionTime by the particle life
//   budget = mEmissionCountClamp ? seedCopy.Build(clamp, clampVariance) - mEmissionCount : 256
//   while (budget && now > mNextEmissionTime) { Emit(...); mNextEmissionTime += interval }
//
// ⭐ THE SEED IS USED TWO DIFFERENT WAYS AND THAT IS DELIBERATE. Every draw Generate itself makes
// (IsGenerating's schedule, the rate, the count) goes through a 64-byte COPY of mEmitterSeed on
// the stack, so it does not advance the emitter's stream; but Emit is handed `r25 == this +
// 0x1B0`, the MEMBER seed, so every particle actually born does advance it. Reading the copy as
// the seed throughout would make every particle in a burst identical.
//
// ⚠ THE 0.1 IS A FLOOR, NOT AN EPSILON. flt_820FEC40 == 0.1 and the console computes
// `fsubs f13, rate, 0.1 ; fsel f13, f13, rate, 0.1` -- max(rate, 0.1). An effect authored with
// a zero emission rate therefore emits at 10 Hz, not never and not by dividing by zero.
//
// ⚠ THE TICK/3 IS EVERYWHERE AND IT IS AN INTEGER DIVIDE. mNextEmissionTime is kept in
// MILLISECONDS (3000 ticks/second / 3 == 1000), which is why the interval is 1000/rate and why
// Emit's spawn stamp is `3 * mNextEmissionTime` converted back to ticks.
//
// ⚠ THE LOOP'S ACCUMULATOR IS FLOAT AND ITS BASE IS THE *ORIGINAL* mNextEmissionTime (r26, read
// once at 0x829152EC), so the emission times do not drift by repeated integer truncation:
// each is `(s32)(k * interval) + start`, not `previous + (s32)interval`.
// ================================================================================================
void cParticleEmitter::Generate(const cTime& arTime)
{
    const s32 liMonitor = giEmitterGenerateMonitor;
    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

    // asm 0x82915188..0x829151AC -- the scratch seed every decision below draws from.
    cParticleRandomSeed lSeedCopy = mEmitterSeed;

    if (!IsGenerating(lSeedCopy, mUpdateLastTime, arTime))
    {
        mFlags &= ~KU_FLAG_EMITTING;
        mEmissionCount    = 0;
        mNextEmissionTime = arTime.GetTicks() / 3;
        CgsDev::PerfMonCpu::StopMonitor(liMonitor);
        return;
    }

    Blend();

    const cParticleBehaviour& lrBhv = *mpCurrentBehaviour;

    // asm 0x829151DC..0x82915210.
    f32 lfRate = lSeedCopy.Build(lrBhv.mEmissionRateBase, lrBhv.mEmissionRateVariance);
    if (lfRate < KF_MIN_EMISSION_RATE)
    {
        lfRate = KF_MIN_EMISSION_RATE;
    }
    const f32 lfIntervalMs = KF_MILLISECONDS_PER_SECOND / lfRate;

    if ((lrBhv.mFlags & cParticleBehaviour::E_DO_BURST) != 0)
    {
        // asm 0x82915218..0x82915280 -- the whole burst at once, once per activation.
        if ((mFlags & KU_FLAG_EMITTING) == 0)
        {
            s32 liCount = lSeedCopy.Build(static_cast<s32>(lrBhv.mEmissionCountClamp),
                                          static_cast<s32>(lrBhv.mEmissionCountClampVariance))
                        - static_cast<s32>(mEmissionCount);
            const cTime lSpawnTime = arTime;
            while (liCount > 0)
            {
                Emit(mEmitterSeed, lSpawnTime, arTime);
                --liCount;
            }
            mFlags |= KU_FLAG_EMITTING;
        }
        mNextEmissionTime = arTime.GetTicks() / 3;
        CgsDev::PerfMonCpu::StopMonitor(liMonitor);
        return;
    }

    // asm 0x82915284..0x829152D8 -- DO_PREFORM: start the effect already running by back-dating
    // the emission clock by one whole particle lifetime, so the first frame emits a full stream
    // instead of a single particle.
    if ((mpDescriptor->mFlags & cParticleDescriptor::E_FLAG_PREFORM) != 0 &&
        (mFlags & KU_FLAG_EMITTING) == 0)
    {
        const s32 liLifeMs = static_cast<s32>((lrBhv.mLifeVariance + lrBhv.mLifeBase)
                                              * KF_MILLISECONDS_PER_SECOND);
        mNextEmissionTime = (arTime.GetTicks() / 3) - liLifeMs;
    }

    // asm 0x829152DC..0x82915318 -- the emission budget.
    s32 liBudget = 256;
    if (lrBhv.mEmissionCountClamp != 0)
    {
        liBudget = lSeedCopy.Build(static_cast<s32>(lrBhv.mEmissionCountClamp),
                                   static_cast<s32>(lrBhv.mEmissionCountClampVariance))
                 - static_cast<s32>(mEmissionCount);
        if (liBudget <= 0)
        {
            CgsDev::PerfMonCpu::StopMonitor(liMonitor);
            return;
        }
    }

    // asm 0x8291531C..0x82915398.
    const s32 liStartMs = mNextEmissionTime;
    const s32 liNowMs   = arTime.GetTicks() / 3;
    f32 lfAccumulatedMs = 0.0f;

    if (liNowMs > mNextEmissionTime)
    {
        while (liBudget != 0)
        {
            const cTime lSpawnTime(static_cast<u32>(3 * mNextEmissionTime));
            Emit(mEmitterSeed, lSpawnTime, arTime);

            lfAccumulatedMs = lfAccumulatedMs + lfIntervalMs;
            --liBudget;

            mNextEmissionTime = static_cast<s32>(lfAccumulatedMs) + liStartMs;
            if (liNowMs <= mNextEmissionTime)
            {
                break;
            }
        }
    }

    // asm 0x82915380..0x82915388 -- only catch the clock up to `now` when the budget ran out;
    // if it broke out with budget left, mNextEmissionTime is already the next due time.
    if (liBudget == 0)
    {
        mNextEmissionTime = liNowMs;
    }
    mFlags |= KU_FLAG_EMITTING;

    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
}

// =================================================================================================
// THE BUCKET WALK -- cParticleEmitter::SimulateParticlesInBucketGeneral<T> and its three helpers.
//
// This is the bridge between the per-particle simulation (ParticleBuild) and the draw path. The
// render driver hands one kernel a bucket, a run of RenderedParticle slots and a side array; the
// kernel advances every live slot one frame and returns how many are still alive -- which is the
// vertex count cParticleRender::EmitterRender builds its LionBatch from.
//
// X360: @0x829120C0 <MatrixSimulationHelper> (199 instructions), @0x829123E0
// <VectorSimulationHelper> (138), @0x82912610 <LocalSimulationHelper> (209). Everything above the
// helper call is instruction-for-instruction identical in all three -- which is what proves they
// are one template rather than three hand-written loops. The Vector specialisation is the shortest
// only because its helper reads no matrix, so the compiler dead-stripped three of the four arms of
// the locator-transform selector below (it could NOT strip the ParentMatrixCurrentBuild call, an
// opaque one, and the slot it writes into is reused for the seed copy -- which is exactly how the
// dead-store elimination is visible from outside).
// =================================================================================================

namespace
{
    // ---------------------------------------------------------------------------------------------
    // sub_8290D3B8 (95 instructions) -- DO_EMITTER_WEIGHTING, the shared body of the Matrix and
    // Vector helpers. Unnamed in the idb; named here for what it does.
    //
    // Fade the particle's inherited LOCATOR VELOCITY contribution out over its life:
    //
    //     w =  1                                        while life <  mEmitterStartWeight
    //       =  1 - (life - start) / (end - start)       while life <  mEmitterEndWeight
    //       =  0                                        thereafter
    //     arAccumulator += particle.mLocatorVel * (particle.mLocatorVel.w * w)
    //
    // GATES, both of them: cParticleBehaviour::mFlags & E_DO_EMITTER_WEIGHTING (0x2000000,
    // `rlwinm r11, r11, 0,6,6` @0x8290D3BC) AND cParticleDescriptor::mFlags &
    // E_FLAG_NEEDS_BUCKET (0x10, `extrwi r11, r11, 1,27` @0x8290D3CC). Either clear and the
    // accumulator is left exactly as it was.
    //
    // ⚠ THE LIFE IT MEASURES IS mvTimeScaleAndLifeScale.y (`vspltw v10, v13, 1` off particle+0x60
    // @0x8290D408), i.e. RenderedParticle::LifeScale() -- not the age and not the time.
    // ⚠ AND THE SCALE FACTOR IS THE VELOCITY'S OWN W LANE (`vspltw v13, v11, 3` off particle+0x20
    // @0x8290D414), the "Plus" of the DWARF's Vector3Plus. mLocatorVel carries its own weight.
    //
    // The two arms are continuous at life == start (w == 1 both sides), which is what makes the
    // reading safe rather than merely plausible; the console re-loads mEmitterStartWeight into the
    // splat slot at 0x8290D48C precisely so the ramp starts from it.
    // ---------------------------------------------------------------------------------------------
    void ApplyEmitterWeighting(const cParticleBehaviour* apBehaviour,
                               const cParticleDescriptor* apDescriptor,
                               const RenderedParticle* apParticle,
                               cVector& arAccumulator)
    {
        if ((apBehaviour->mFlags & cParticleBehaviour::E_DO_EMITTER_WEIGHTING) == 0)
            return;
        if ((apDescriptor->Flags() & cParticleDescriptor::E_FLAG_NEEDS_BUCKET) == 0)
            return;

        const f32 lfStart = apBehaviour->mEmitterStartWeight;
        const f32 lfLife  = apParticle->LifeScale();
        const cVector& lrVel = apParticle->mLocatorVel;

        f32 lfWeight;
        if (lfStart > lfLife)                       // vcmpgtfp. @0x8290D420
        {
            lfWeight = 1.0f;
        }
        else
        {
            const f32 lfEnd = apBehaviour->mEmitterEndWeight;
            if (lfEnd > lfLife)                     // vcmpgtfp. @0x8290D474
            {
                // asm 0x8290D490..0x8290D51C: t = (life - start) * (1 / (end - start)),
                // then the weight is 1 - t (the `fsubs f0, f0, f13` at 0x8290D504 against the
                // 1.0 flt_82001C98 already in f0).
                const f32 lfT = (lfLife - lfStart) * (1.0f / (lfEnd - lfStart));
                lfWeight = 1.0f - lfT;
            }
            else
            {
                lfWeight = 0.0f;                    // no contribution; the store below is a no-op
            }
        }

        const f32 lfScale = lrVel.w * lfWeight;
        arAccumulator.x = lrVel.x * lfScale + arAccumulator.x;
        arAccumulator.y = lrVel.y * lfScale + arAccumulator.y;
        arAccumulator.z = lrVel.z * lfScale + arAccumulator.z;
        arAccumulator.w = lrVel.w * lfScale + arAccumulator.w;
    }
}

// -------------------------------------------------------------------------------------------------
// MatrixSimulationHelper::UpdateLocatorVelocity  @ 0x8290DF28  (an EXPORT-SET HOLE -- no
// 0x8290DF28.json; disassembled out of the image with tools/re/ppcdis.py + tools/re/vmx128.py)
//
// Publish the per-particle MATRIX this particle is drawn with, and advance its translation row by
// the emitter-weighting blend:
//
//     mpMatrices[out] = apBucket->GetMatrices()[slot];             (four lvx128/stvx128 pairs,
//                                                                   0x8290DF6C..0x8290DF90)
//     v = mpMatrices[out].wa;                                      (the ld/std pair @0x8290DFA0)
//     ApplyEmitterWeighting(bhv, des, particle, v);                (bl 0x8290D3B8 @0x8290DFB0)
//     mpMatrices[out].wa = (v.x, v.y, v.z, 1);                     (vsel with unk_820FEBD0 /
//                                                                   unk_820FEBE0 @0x8290DFD8)
//     apBucket->GetMatrices()[slot] = mpMatrices[out];             (four more pairs, 0x8290DFF0..)
//
// ⚠ THE WRITE-BACK TO THE BUCKET IS REAL AND IS THE POINT: the blend is CUMULATIVE, so the
// particle's own matrix carries the accumulated locator drift from frame to frame. Dropping it
// (an easy "the helper only publishes" tidy-up) would restart the drift every frame and the
// exhaust plume would stop trailing behind the car.
//
// ⚠ unk_820FEBD0 == (FFFFFFFF, FFFFFFFF, FFFFFFFF, 0) and unk_820FEBE0 == splat4(1.0), both read
// out of the image, so the vsel is "xyz from the blend, w forced to 1" -- an affine translation
// row. A stray w here would translate on every later multiply.
//
// ⚠ THE LOCATOR MATRIX (the 8th argument, r10) IS READ BY NOTHING in this body. The register is
// set by the caller @0x82912364 and never touched again; stated rather than dropped, the same
// call ParentMatrixCurrentBuild's r7 and QuadDraw's fifth parameter got.
// -------------------------------------------------------------------------------------------------
void MatrixSimulationHelper::UpdateLocatorVelocity(cParticleBucket* apBucket,
                                                   const cParticleBehaviour* apBehaviour,
                                                   const cParticleDescriptor* apDescriptor,
                                                   const RenderedParticle* apParticle,
                                                   u32 auOutIndex,
                                                   u32 auSlot,
                                                   const cMatrix& /*arLocatorMat*/)
{
    cMatrix* const lpBucketMatrices = apBucket->GetMatrices();

    mpMatrices[auOutIndex] = lpBucketMatrices[auSlot];

    cVector lvTranslation = mpMatrices[auOutIndex].wa;
    ApplyEmitterWeighting(apBehaviour, apDescriptor, apParticle, lvTranslation);

    mpMatrices[auOutIndex].wa.x = lvTranslation.x;
    mpMatrices[auOutIndex].wa.y = lvTranslation.y;
    mpMatrices[auOutIndex].wa.z = lvTranslation.z;
    mpMatrices[auOutIndex].wa.w = 1.0f;

    lpBucketMatrices[auSlot] = mpMatrices[auOutIndex];
}

// -------------------------------------------------------------------------------------------------
// VectorSimulationHelper::UpdateLocatorVelocity -- inlined by the X360 into
// SimulateParticlesInBucketGeneral<VectorSimulationHelper> @0x82912580..0x829125B4; re-outlined
// here per the project's inlining-reversal rule (and because that is what makes the three
// specialisations one template).
//
// The same three steps as the Matrix helper, on the bucket's cVector side array rather than the
// translation row of a matrix:
//     mpVectors[out] = apBucket->GetVectorArray()[slot];     (lvx128 v0, r26, r11 / stvx128 r28)
//     ApplyEmitterWeighting(bhv, des, particle, mpVectors[out]);
//     apBucket->GetVectorArray()[slot] = mpVectors[out];     (stvx128 v0, r26, r11)
//
// ⚠ NO W FORCE HERE, and the asymmetry is the console's: the matrix arm rebuilds an affine
// translation row (w := 1) because it is writing a matrix; the vector arm stores the blended
// vector verbatim, w lane included.
// -------------------------------------------------------------------------------------------------
void VectorSimulationHelper::UpdateLocatorVelocity(cParticleBucket* apBucket,
                                                   const cParticleBehaviour* apBehaviour,
                                                   const cParticleDescriptor* apDescriptor,
                                                   const RenderedParticle* apParticle,
                                                   u32 auOutIndex,
                                                   u32 auSlot,
                                                   const cMatrix& /*arLocatorMat*/)
{
    cVector* const lpBucketVectors = apBucket->GetVectorArray();

    mpVectors[auOutIndex] = lpBucketVectors[auSlot];
    ApplyEmitterWeighting(apBehaviour, apDescriptor, apParticle, mpVectors[auOutIndex]);
    lpBucketVectors[auSlot] = mpVectors[auOutIndex];
}

// -------------------------------------------------------------------------------------------------
// LocalSimulationHelper::UpdateLocatorVelocity -- inlined by the X360 into
// SimulateParticlesInBucketGeneral<LocalSimulationHelper> @0x829128E4..0x829128FC.
//
// A bucket with NEITHER side array: every particle in it is drawn against the emitter's own
// locator transform, so the helper just publishes that same 64 bytes per emitted particle. The
// console parks the four rows in v126/v125/v124/v127 before the loop (0x8291282C..0x82912850,
// which is why this specialisation is the one that calls __savevmx_124) and stores them with the
// four `stvx128 v126, r30, r19` / `v125, r30, r20` / `v124, r0, r30` / `v127, r30, r16` at
// r30 == &mpMatrices[out] + 0x20 with r19/r20/r16 == -0x20/-0x10/+0x10 -- i.e. rows 0/1/2/3 in
// order, then `addi r30, r30, 0x40`.
//
// ⚠ It touches NEITHER the bucket NOR the emitter-weighting blend. There is no side array to
// accumulate into, so DO_EMITTER_WEIGHTING has no effect on a light bucket. That is in the binary.
// -------------------------------------------------------------------------------------------------
void LocalSimulationHelper::UpdateLocatorVelocity(cParticleBucket* /*apBucket*/,
                                                  const cParticleBehaviour* /*apBehaviour*/,
                                                  const cParticleDescriptor* /*apDescriptor*/,
                                                  const RenderedParticle* /*apParticle*/,
                                                  u32 auOutIndex,
                                                  u32 /*auSlot*/,
                                                  const cMatrix& arLocatorMat)
{
    mpMatrices[auOutIndex] = arLocatorMat;
}

// -------------------------------------------------------------------------------------------------
// cParticleEmitter::SimulateParticlesInBucketGeneral<T>
//   @0x829120C0 <Matrix> / @0x829123E0 <Vector> / @0x82912610 <Local>
//   (DWARF ParticleEmitter.cpp:782 / :914 / :1048)
//
// FOUR WAYS TO BUILD THE LOCATOR TRANSFORM, in the console's own order (0x829120F8..0x8291223C):
//   1. descriptor E_FLAG_NEEDS_BUCKET (0x10)  -> IDENTITY. Sixteen scalar stores of
//      flt_82001C98 (1.0) / flt_82001CC0 (0.0), both read out of the image.
//   2. emitter KU_FLAG_SUB_EMITTER (0x8)      -> ParentMatrixCurrentBuild: re-derive where the
//      PARENT particle is this frame, because a sub-emitter's parent is stored nowhere.
//      ⚠ It rides the f32-eats-a-GPR pattern -- `lfs f1, 0x1AC(r29)` is mDt in f1 and r7 is
//      lCurrentLocatorTime, with r6 skipped. See ParentMatrixCurrentBuild's own note.
//   3. descriptor E_FLAG_IGNORE_ROT (0x100)   -> identity ROTATION, the binding locator's
//      TRANSLATION. The console does it with a vsel of unk_820FEBD0 (FFFFFFFF x3, 0) between
//      lBindingsLocatorMat.wa and unk_820FEBE0 (splat4(1.0)), i.e. exactly SetTrans's w := 1.
//   4. otherwise                              -> the binding locator verbatim (four lvx128/
//      stvx128 pairs, 0x82912208..0x8291223C).
//
// THEN, once per bucket:
//   * a 64-byte copy of the bucket's random seed onto the stack (the 8-iteration ld/std loop
//     @0x82912250) -- the per-slot draws advance the COPY, so the bucket's own stream is not
//     disturbed by rendering;
//   * two lanes of mPrecalculatedParticleBuildData.mvDeltaTimeAndCurrentTime are refreshed:
//     lane 0 := mDt (`lvlx v0, r29, 0x1AC` + `vrlimi128 v12, v0, 8, 0`) and lane 1 :=
//     aTime's tick count in SECONDS (`fcfid` + `frsp` + `* flt_82F369A8`, and flt_82F369A8 is
//     1/3000 -- the Lion clock is 3000 ticks per second, the same divisor Generate's
//     millisecond arithmetic uses). The member's DWARF name says exactly that.
//
// THEN, per slot 0..15:
//   * skip the slot unless its occupancy bit is set;
//   * advance the seed copy one Park-Miller step (cParticleRandomSeed::Update), take a COPY of
//     it, and Offset that copy by the particle's BIRTH TIME in ticks
//     (`lfs f13, 0xDC(r28)` == nucleus.BirthTime(), `* flt_820FEC3C` == 3000.0, `fctidz`).
//     ⭐ That is what makes a particle's random stream a function of WHEN IT WAS BORN rather
//     than of what order the bucket happens to be walked in -- so a particle looks the same on
//     every frame of its life, and a replay reproduces it.
//   * ParticleBuild, and switch on its three-valued result: ALIVE -> publish through the helper
//     and take the next output slot; DEAD -> retire the slot; NOT-BORN-YET -> nothing.
//
// AND FINALLY: a bucket whose last particle just died is handed straight back to the pool
// (cParticleBucketManager::Free, `lwz r11, 0x54(r24)` + `bne` @0x829123B8). The render pass is
// where buckets are recycled, which is why an emitter that is not being rendered leaks none --
// its buckets are simply never walked.
// -------------------------------------------------------------------------------------------------
template <class T>
u32 cParticleEmitter::SimulateParticlesInBucketGeneral(T lHelper,
                                                       RenderedParticle* laSimulatedParticles,
                                                       cParticleBucket* lpBucket,
                                                       const cTime& aTime,
                                                       const cTime& lCurrentLocatorTime,
                                                       const cMatrix& lBindingsLocatorMat)
{
    if (lpBucket->IsEmpty())
    {
        return 0;
    }

    // ---- the transform every particle in this bucket is simulated against --------------------
    cMatrix lLocatorMat;
    const u32 luDescriptorFlags = mpDescriptor->Flags();

    if ((luDescriptorFlags & cParticleDescriptor::E_FLAG_NEEDS_BUCKET) != 0)
    {
        lLocatorMat.BuildIdentity();
    }
    else if ((mFlags & KU_FLAG_SUB_EMITTER) != 0)
    {
        ParentMatrixCurrentBuild(lLocatorMat, aTime, mDt, lCurrentLocatorTime);
    }
    else if ((luDescriptorFlags & cParticleDescriptor::E_FLAG_IGNORE_ROT) != 0)
    {
        lLocatorMat.BuildIdentity();
        lLocatorMat.SetTrans(lBindingsLocatorMat.wa.x,
                             lBindingsLocatorMat.wa.y,
                             lBindingsLocatorMat.wa.z);
    }
    else
    {
        lLocatorMat = lBindingsLocatorMat;
    }

    // ---- the bucket's seed, copied so the draw does not disturb the bucket's own stream ------
    cParticleRandomSeed lBucketSeed = lpBucket->GetRandomSeed();

    // ---- this frame's two build-data lanes (asm 0x82912284..0x829122D4) ----------------------
    mPrecalculatedParticleBuildData.mvDeltaTimeAndCurrentTime.x = mDt;
    mPrecalculatedParticleBuildData.mvDeltaTimeAndCurrentTime.y =
        aTime.GetTimeSeconds();

    sParticleNucleus* const laNuclei = lpBucket->GetParticles();

    u32 luOutCount = 0;
    for (u32 luSlot = 0; luSlot < cParticleBucket::KU_MAX_PARTICLES; ++luSlot)
    {
        if (!lpBucket->IsParticleActive(luSlot))
        {
            continue;
        }

        lBucketSeed.Update();

        cParticleRandomSeed lParticleSeed = lBucketSeed;
        lParticleSeed.Offset(static_cast<u32>(
            static_cast<s32>(laNuclei[luSlot].BirthTime() * msfTicksPerSecond)));

        const EParticleBuildResult leResult =
            ParticleBuild(laSimulatedParticles[luOutCount],
                          lParticleSeed,
                          laNuclei[luSlot],
                          *mpDescriptor,
                          *mpCurrentBehaviour,
                          mPrecalculatedParticleBuildData);

        if (leResult == eParticleBuildResultAlive)
        {
            lHelper.UpdateLocatorVelocity(lpBucket,
                                          mpCurrentBehaviour,
                                          mpDescriptor,
                                          &laSimulatedParticles[luOutCount],
                                          luOutCount,
                                          luSlot,
                                          lLocatorMat);
            ++luOutCount;
        }
        else if (leResult == eParticleBuildResultDead)
        {
            lpBucket->RetireParticle(luSlot);
        }
    }

    if (lpBucket->IsEmpty())
    {
        cParticleBucketManager::Instance().Free(lpBucket);
    }

    return luOutCount;
}

// The three the console emitted, and the only three that exist. `extern template` in the header
// lets cParticleRender::EmitterRender / ::EmitterCubeRender call them without dragging this body
// (and cParticleBucketManager) into ParticleRender.cpp -- which is also how the console ended up
// with exactly one copy of each, in this TU.
template u32 cParticleEmitter::SimulateParticlesInBucketGeneral<MatrixSimulationHelper>(
    MatrixSimulationHelper, RenderedParticle*, cParticleBucket*, const cTime&, const cTime&,
    const cMatrix&);
template u32 cParticleEmitter::SimulateParticlesInBucketGeneral<VectorSimulationHelper>(
    VectorSimulationHelper, RenderedParticle*, cParticleBucket*, const cTime&, const cTime&,
    const cMatrix&);
template u32 cParticleEmitter::SimulateParticlesInBucketGeneral<LocalSimulationHelper>(
    LocalSimulationHelper, RenderedParticle*, cParticleBucket*, const cTime&, const cTime&,
    const cMatrix&);
