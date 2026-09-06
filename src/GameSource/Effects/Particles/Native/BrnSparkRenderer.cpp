// ============================================================================
// GameSource/Effects/Particles/Native/BrnSparkRenderer.cpp
//
// BrnParticle::Native::SparkArray / SparkArray::SparkBank / SparkRenderer --
// the grinding-metal spark family.
//
// X360 ARTIST bodies reconstructed here:
//   SparkArray::SparkBank::GetNewSpark        @0x82295478   (90 instr)
//   SparkArray::SparkBank::FreeUnusedBuckets  @0x82283B60   (44 instr)
//   SparkArray::SpawnSpark                    @0x822955E0   (211 instr)
//   SparkFrameData::Set                       -- inlined (Reset emits it 8x)
// and the four bodies the console INLINES into their single caller, re-outlined
// here at the shape the DWARF declares them:
//   SparkArray::Construct / SparkBank::Construct   <- ParticleModule::Prepare @0x8229BEA0
//   SparkArray::FreeUnusedBuckets                  <- BeginParticleRenderJob @0x8228A7C0
//   SparkArray::UpdateParams                       <- EffectsModule::Prepare @0x8229E690
//                                                     and ::Update @0x8229EC28
//   SparkArray::AcquireTexture / GetTexture        <- LoadFXBundle @0x8229C950 stage 12
//                                                     and SparkRenderer::Dispatch @0x8228BBC8
//   SparkRenderer::Construct                       <- ParticleModule::Prepare
//
// Every offset in this file is the console's own store or load; the derivations are
// spelled at each body. Nothing here is authored to "look like sparks".
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // the one NOT-RECONSTRUCTED announcement
#include <cmath>                                            // std::sqrt (the console's fsqrts)

namespace BrnParticle
{
namespace Native
{

// ---------------------------------------------------------------------------------------------
// The four-entry texture table. STATIC, not a member -- LoadFXBundle @0x8229C950 publishes into
// the fixed table at unk_82FAC230 with `addi r26, r26, 8` per array and the loop bound
// qword_82FAC250 (base + 0x20 == 4 * sizeof(ResourceHandle)), while separately advancing the
// SparkArray cursor by the array stride 0x90. SparkRenderer::Dispatch @0x8228BD00 loads the same
// base (`lis r11, unk_82FAC230@ha`) and indexes it by the batch's meArrayId.
// ---------------------------------------------------------------------------------------------
CgsResource::SafeResourceHandle<renderengine::Texture> SparkArray::maTextures[4];

// =============================================================================================
// SparkFrameData::Set -- inlined at every use.
//
// Reset @0x8227A610 emits this five-store sequence eight times, once per ring frame, always in
// this order: the four view rows to +0x00, the four projection rows to +0x40, the timestamp to
// +0x80 AND +0x84, then the view rows again to +0x90 (`addi r26, r3, 0x90`). Update @0x82283C10
// emits it once, on frame 0.
// =============================================================================================
void SparkFrameData::Set(rw::math::vpu::Matrix44Affine::InParam lViewMatrix,
                         rw::math::vpu::Matrix44::InParam       lProjectionMatrix,
                         f32                                    lfTimeStamp)
{
    mViewMatrix                       = lViewMatrix;
    mProjectionMatrix                 = lProjectionMatrix;
    mfTimeStamp                       = lfTimeStamp;
    mfTimeStampCopyForDebugBuildsOnly = lfTimeStamp;
    mViewMatrixCopyForDebugBuildsOnly = lViewMatrix;
}

// =============================================================================================
// SparkFrameDataSet::Reset @0x8227A610 -- 243 instructions, all of them the eight fully unrolled
// copies of the Set above (the frame bases the asm materialises are r3+0x00, +0xD0, +0x1A0, ...
// i.e. the 0xD0 stride, and each one writes +0x00/+0x40/+0x80/+0x84/+0x90).
//
// Collapsing the console's unrolled sequence back into a loop is the "re-roll unrolled loops"
// de-optimisation AGENTS.md requires; the stores, their order and their values are unchanged.
// =============================================================================================
void SparkFrameDataSet::Reset(rw::math::vpu::Matrix44Affine::InParam lViewMatrix,
                              rw::math::vpu::Matrix44::InParam       lProjectionMatrix,
                              f32                                    lfTimeStamp)
{
    for (u32 luFrame = 0; luFrame < knSparksMaxBlurFrames; ++luFrame)
    {
        maFrames[luFrame].Set(lViewMatrix, lProjectionMatrix, lfTimeStamp);
    }
}

// =============================================================================================
// SparkFrameDataSet::Update @0x82283C10.
//
// The motion-blur ring only takes a new SAMPLE when enough time has passed; frame 0 always
// tracks the live camera. The console's shipping path is the tail of this function
// (0x822842D8..0x82284460):
//
//   f0  = maFrames[0].mfTimeStamp          lfs  f0, 0x80(r3)
//   f0  = f0 + lfDeltaTime                 fadds f0, f0, f1        <- the argument is a DELTA
//   f13 = maFrames[1].mfTimeStamp          lfs  f13, 0x150(r3)     (0xD0 + 0x80)
//   if ( (f0 - f13) >= 0.015625f )         flt_8200DD54 == 1/64 s
//       for ( i = 7 ; i >= 1 ; --i )       `addi r11, r11, -0xD0` / `bne`
//           maFrames[i] = maFrames[i-1];   the 13 lvx/stvx + 2 lfs/stfs pairs per iteration
//   maFrames[0].Set( view, projection, f0 );
//
// The per-iteration copy is spelled out as thirteen 16-byte moves plus the two debug floats
// (`lfs f13, 0x20(r11)` -> `stfs f13, 0xF0(r11)` is mfTimeStamp/+0x84 at +0xD0), i.e. exactly a
// whole 0xD0 SparkFrameData; re-rolled here to the struct assignment it came from.
//
// [!] THE lfDeltaTime == 0.0f ARM IS NOT RECONSTRUCTED. The console's entry test is
//     `fcmpu cr6, f1, flt_82001CC0(0.0) ; bne -> the path above`, so a delta of exactly zero
//     takes a separate ~430-instruction block (0x82283C5C..0x822842D4) that re-derives the ring
//     from three static matrices at unk_82FAD0B0 / unk_82FAD120 / unk_82FAD140 behind a
//     one-shot latch word (dword_82FAD1B0) -- a debug-build validation pass, not a shipping
//     path. It is announced, not faked: this build's caller (BeginParticleRenderJob) passes the
//     frame delta, which is zero only on a stalled frame.
// =============================================================================================
void SparkFrameDataSet::Update(rw::math::vpu::Matrix44Affine::InParam lViewMatrix,
                               rw::math::vpu::Matrix44::InParam       lProjectionMatrix,
                               f32                                    lfDeltaTime)
{
    // flt_8200DD54 == 0.015625 == 1/64 s -- the minimum spacing between two ring samples.
    const f32 KF_MIN_BLUR_SAMPLE_SPACING = 0.015625f;

    if (lfDeltaTime == 0.0f)
    {
        static bool sbLogged = false;
        if (!sbLogged)
        {
            sbLogged = true;
            CgsDev::Log::WriteToLog(
                "[NOT RECONSTRUCTED] SparkFrameDataSet::Update's lfDeltaTime == 0 arm "
                "(X360 0x82283C5C, the debug ring re-derivation)\n");
        }
        return;
    }

    const f32 lfNewTimeStamp = maFrames[0].mfTimeStamp + lfDeltaTime;

    if ((lfNewTimeStamp - maFrames[1].mfTimeStamp) >= KF_MIN_BLUR_SAMPLE_SPACING)
    {
        for (u32 luFrame = knSparksMaxBlurFrames - 1; luFrame >= 1; --luFrame)
        {
            maFrames[luFrame] = maFrames[luFrame - 1];
        }
    }

    maFrames[0].Set(lViewMatrix, lProjectionMatrix, lfNewTimeStamp);
}

// =============================================================================================
// SparkArray::SparkBank::Construct -- inlined into ParticleModule::Prepare @0x8229C164..0x8229C210
// as four identical four-word records per array:
//     stw 0,      0(bank)      mpBuckets       = 0
//     stw mgr,    4(bank)      mpBucketManager = &module.mBucketManager
//     stw 0,      8(bank)      muNumBuckets    = 0
//     stw 0x7D0/0x3E8, 0xC(bank)  muMaxNumSparks = 2000 (regular) / 1000 (crash)
// The two caps are `li r7, 0x7D0` and `li r8, 0x3E8`, hoisted once and reused by all four arrays.
// =============================================================================================
void SparkArray::SparkBank::Construct(FXBucketManager* lpBucketManager, bool lbIsCrashBank)
{
    // ParticleModule::Prepare's own two literals.
    const u32 KU_MAX_SPARKS_REGULAR = 2000;  // li r7, 0x7D0
    const u32 KU_MAX_SPARKS_CRASH   = 1000;  // li r8, 0x3E8

    mpBuckets       = 0;
    mpBucketManager = lpBucketManager;
    muNumBuckets    = 0;
    muMaxNumSparks  = lbIsCrashBank ? KU_MAX_SPARKS_CRASH : KU_MAX_SPARKS_REGULAR;
}

// =============================================================================================
// SparkArray::SparkBank::GetNewSpark @0x82295478.
//
// Three outcomes, in the console's own order:
//   * the head bucket exists and has room  -> take a slot from it;
//   * the bank is below its spark cap      -> borrow another bucket from the manager, push it at
//                                             the head, ++muNumBuckets, take a slot from it;
//   * the bank is at its cap               -> RECYCLE: unlink the list's TAIL (the oldest
//                                             bucket), move it to the head, rewind its write
//                                             cursor to 0 and take a slot from it.
// The cap test is `mulli r11, muNumBuckets, 0x9C ; cmplw muMaxNumSparks ; bge` -- i.e. the cap is
// compared against the bucket count times SparkBucket::KuMaxNumParticles (156), not against a
// live spark count.
//
// The head-insert is the generic doubly-linked "insert before head" the console emits at both
// sites, `head->mpPreviousBucket` guard included; in a list only ever built by this function that
// pointer is null on the head, so the guarded arm never runs -- reproduced because it is what the
// binary does, not because it can fire.
//
// [!] THE RECYCLED BUCKET IS *NOT* CLEARED. The console rewinds only mu16NextPositionInBucket
//     (`sth 0, 0xE(bucket)`); mu16NumberOfParticlesInBucket and mfFinalParticleBirthTime keep
//     their old values, so the recycled bucket keeps reporting a full particle count while its
//     slots are overwritten from the start. That is the console's behaviour, not an omission.
// =============================================================================================
BrnSpark* SparkArray::SparkBank::GetNewSpark(f32 lfBirthTime)
{
    SparkBucket* lpHeadBucket = mpBuckets;

    if (lpHeadBucket != 0
        && lpHeadBucket->mu16NextPositionInBucket < SparkBucket::KuMaxNumParticles)
    {
        return lpHeadBucket->GetNewParticle(lfBirthTime);
    }

    if ((SparkBucket::KuMaxNumParticles * muNumBuckets) < muMaxNumSparks)
    {
        // Room under the cap: borrow one more bucket.
        SparkBucket* const lpNewBucket = mpBucketManager->AllocateBucket<SparkBucket>();
        if (lpNewBucket == 0)
            return 0;

        SparkBucket* const lpOldHead = mpBuckets;
        lpNewBucket->mpNextBucket = lpOldHead;
        if (lpOldHead == 0)
        {
            lpNewBucket->mpPreviousBucket = 0;
        }
        else
        {
            FXBucketBase* const lpOldHeadPrevious = lpOldHead->mpPreviousBucket;
            lpNewBucket->mpPreviousBucket = lpOldHeadPrevious;
            if (lpOldHeadPrevious != 0)
                lpOldHeadPrevious->mpNextBucket = lpNewBucket;
            lpOldHead->mpPreviousBucket = lpNewBucket;
        }
        mpBuckets = lpNewBucket;
        ++muNumBuckets;

        return lpNewBucket->GetNewParticle(lfBirthTime);
    }

    if (lpHeadBucket == 0)
        return 0;

    // At the cap: walk to the list tail (the oldest bucket) and recycle it.
    SparkBucket* lpOldestBucket = lpHeadBucket;
    while (lpOldestBucket->mpNextBucket != 0)
    {
        lpOldestBucket = static_cast<SparkBucket*>(lpOldestBucket->mpNextBucket);
    }

    if (lpOldestBucket->mpPreviousBucket != 0)
        lpOldestBucket->mpPreviousBucket->mpNextBucket = lpOldestBucket->mpNextBucket;
    if (lpOldestBucket->mpNextBucket != 0)
        lpOldestBucket->mpNextBucket->mpPreviousBucket = lpOldestBucket->mpPreviousBucket;
    lpOldestBucket->mpNextBucket     = 0;
    lpOldestBucket->mpPreviousBucket = 0;

    SparkBucket* const lpOldHead = mpBuckets;
    lpOldestBucket->mpNextBucket = lpOldHead;
    if (lpOldHead == 0)
    {
        lpOldestBucket->mpPreviousBucket = 0;
    }
    else
    {
        FXBucketBase* const lpOldHeadPrevious = lpOldHead->mpPreviousBucket;
        lpOldestBucket->mpPreviousBucket = lpOldHeadPrevious;
        if (lpOldHeadPrevious != 0)
            lpOldHeadPrevious->mpNextBucket = lpOldestBucket;
        lpOldHead->mpPreviousBucket = lpOldestBucket;
    }
    mpBuckets = lpOldestBucket;

    lpOldestBucket->mu16NextPositionInBucket = 0;

    return lpOldestBucket->GetNewParticle(lfBirthTime);
}

// =============================================================================================
// SparkArray::SparkBank::FreeUnusedBuckets @0x82283B60.
//
// Walk the whole bucket list once (the `next` pointer is cached BEFORE the node can be moved) and
// give back every bucket whose YOUNGEST particle has already expired:
//     if ( bucket->mfFinalParticleBirthTime + lfLifetime < lfCurrentTime )
// mfFinalParticleBirthTime is the running MAXIMUM birth time FXBucket::GetNewParticle keeps (its
// fsel), so the test is "even the newest spark in this bucket is dead".
//
// The give-back is spelled inline: unlink from the bank's list, push at the manager's free-list
// head, ++manager.muNumFreeBuckets, --muNumBuckets. The console does NOT Clear() the returned
// bucket -- FXBucketManager::AllocateBucket does that when it hands it out again.
// =============================================================================================
void SparkArray::SparkBank::FreeUnusedBuckets(f32 lfCurrentTime, f32 lfLifetime)
{
    SparkBucket* lpBucket = mpBuckets;
    while (lpBucket != 0)
    {
        SparkBucket* const lpNextBucket = static_cast<SparkBucket*>(lpBucket->mpNextBucket);

        if ((lpBucket->mfFinalParticleBirthTime + lfLifetime) < lfCurrentTime)
        {
            if (mpBuckets == lpBucket)
                mpBuckets = static_cast<SparkBucket*>(lpBucket->mpNextBucket);

            FXBucketManager* const lpManager = mpBucketManager;

            if (lpBucket->mpPreviousBucket != 0)
                lpBucket->mpPreviousBucket->mpNextBucket = lpBucket->mpNextBucket;
            if (lpBucket->mpNextBucket != 0)
                lpBucket->mpNextBucket->mpPreviousBucket = lpBucket->mpPreviousBucket;

            lpBucket->mpNextBucket = lpManager->mpFreeList;
            if (lpManager->mpFreeList != 0)
                lpManager->mpFreeList->mpPreviousBucket = lpBucket;
            lpManager->mpFreeList = lpBucket;
            lpBucket->mpPreviousBucket = 0;

            ++lpManager->muNumFreeBuckets;
            --muNumBuckets;
        }

        lpBucket = lpNextBucket;
    }
}

// =============================================================================================
// SparkArray::Construct -- inlined into ParticleModule::Prepare. The console writes NOTHING here
// but the two bank records: no colour, no lifetime, no scalar and no texture name. Every other
// member is published by UpdateParams, which EffectsModule::Prepare runs before the first frame
// and EffectsModule::Update re-runs every step.
//
// leArrayId is the DWARF's second parameter and is unread by the X360 body (the array's identity
// is its index in the module's maSparks[4]); it is kept so the declaration matches the DWARF and
// so the call sites read as the console's.
// =============================================================================================
void SparkArray::Construct(FXBucketManager* lpBucketManager, ESparkArrayID leArrayId)
{
    (void)leArrayId;
    mRegularBank.Construct(lpBucketManager, false);
    mCrashBank.Construct(lpBucketManager, true);
}

// =============================================================================================
// SparkArray::FreeUnusedBuckets -- inlined into BeginParticleRenderJob @0x8228A7C0, which walks
// the four arrays from `module + 38160` (== +0x9510 == &maSparks[0].mafLifetimes[0]) with a 144
// byte stride, computes the longest of the four lifetimes with three fsels
//     fsel f0,  (l0 - l1), l0, l1        ; max(l0, l1)
//     fsel f13, (l2 - l3), l2, l3        ; max(l2, l3)
//     fsel f2,  (f0 - f13), f0, f13      ; max of those
// and hands that to BOTH banks together with the current time.
// =============================================================================================
void SparkArray::FreeUnusedBuckets(f32 lfCurrentTime)
{
    const f32 lfMaxLifetime01 = (mafLifetimes[0] >= mafLifetimes[1]) ? mafLifetimes[0] : mafLifetimes[1];
    const f32 lfMaxLifetime23 = (mafLifetimes[2] >= mafLifetimes[3]) ? mafLifetimes[2] : mafLifetimes[3];
    const f32 lfMaxLifetime   = (lfMaxLifetime01 >= lfMaxLifetime23) ? lfMaxLifetime01 : lfMaxLifetime23;

    mRegularBank.FreeUnusedBuckets(lfCurrentTime, lfMaxLifetime);
    mCrashBank.FreeUnusedBuckets(lfCurrentTime, lfMaxLifetime);
}

// =============================================================================================
// SparkArray::AcquireTexture / GetTexture -- inlined into LoadFXBundle stage 12 and into
// SparkRenderer::Dispatch. Both carry the same bounds assert on the array id; the console's
// strings are "( leArrayId >= 0 ) && ( leArrayId < eSparkArray_Max )" at BrnSparkRenderer.h:229
// (the publish, `li r5, 0xE5`) and :240 (the read, `li r5, 0xF0`).
// =============================================================================================
void SparkArray::AcquireTexture(CgsResource::SafeResourceHandle<renderengine::Texture> lTexture,
                                ESparkArrayID leArrayId)
{
    CGS_ASSERT((leArrayId >= 0) && (leArrayId < eSparkArray_Max),
               "( leArrayId >= 0 ) && ( leArrayId < eSparkArray_Max )");
    maTextures[leArrayId] = lTexture;
}

renderengine::Texture* SparkArray::GetTexture(ESparkArrayID leArrayId)
{
    CGS_ASSERT((leArrayId >= 0) && (leArrayId < eSparkArray_Max),
               "( leArrayId >= 0 ) && ( leArrayId < eSparkArray_Max )");
    return maTextures[leArrayId];
}

// =============================================================================================
// SparkArray::UpdateParams -- inlined into EffectsModule::Prepare @0x8229E910..0x8229E9E0 and
// re-emitted every step by EffectsModule::Update @0x8229EFC4..0x8229F0A8.
//
// The console's copy reads one Attrib::Gen::sparkeffect data area (0x90 bytes, the same size as
// this object) and writes the array through a cursor r11 == &maSparks[i] + 0x74, so every store
// displacement below is relative to +0x74:
//     stvx128 v11, r11, -0x74   maColours[0]   <- attrib +0x30
//     stvx128 v12, r11, -0x64   maColours[1]   <- attrib +0x20
//     stvx128 v13, r11, -0x54   maColours[2]   <- attrib +0x10
//     stvx128 v0,  r11, -0x44   maColours[3]   <- attrib +0x00
//     stfs f7/f8 -0x34/-0x30, f7/f8 -0x2C/-0x28   mafLifetimes[0..3] <- attrib +0x40 lanes 0..3
//     stfs f9,   0(r11)         mfBounceStrength           <- attrib +0x88
//     stfs f10,  4(r11)         mfMotionBlurTime           <- attrib +0x74
//     stfs f11,  8(r11)         mfSparkRadius              <- attrib +0x68
//     stfs f13, 0xC(r11)        mfDragInitialVelocityScale <- attrib +0x80
//     stfs f0,  0x10(r11)       mfDragTerminalVelocityScale<- attrib +0x7C
//     stfs f12, 0x14(r11)       mfDragDuration             <- attrib +0x84
//     stfs f8,  -4(r11)         mfGravityStrength          <- attrib +0x78
//     stw  r7,  0x18(r11)       mpcSparkTextureName        <- attrib +0x50
//
// ⭐ THE FOUR COLOURS ARE COPIED IN REVERSE. The four loads are v0<-+0x00, v13<-+0x10,
//    v12<-+0x20, v11<-+0x30 and the four stores are +0x00<-v11 .. +0x30<-v0, so maColours[i]
//    takes the attrib vector at (3 - i) * 0x10. That is the console's own pairing, checked
//    register by register; it is reproduced by passing the caller's four colours in that order
//    rather than by re-ordering here, which keeps this body a straight member-for-member publish.
//
// The lifetimes arrive as ONE Vector4 (the console stores attrib+0x40 to a stack slot and reads
// its four lanes back with four lfs), which is why the DWARF signature carries five Vector4s for
// four colours.
// =============================================================================================
void SparkArray::UpdateParams(f32 lfGravityStrength, f32 lfBounceStrength, f32 lfMotionBlurTime,
                              f32 lfSparkRadius, f32 lfDragInitialVelocityScale,
                              f32 lfDragTerminalVelocityScale, f32 lfDragDuration,
                              rw::math::vpu::Vector4::InParam lColour0,
                              rw::math::vpu::Vector4::InParam lColour1,
                              rw::math::vpu::Vector4::InParam lColour2,
                              rw::math::vpu::Vector4::InParam lColour3,
                              rw::math::vpu::Vector4::InParam lLifetimes,
                              const char* lpcSparkTextureName)
{
    maColours[0] = lColour0;
    maColours[1] = lColour1;
    maColours[2] = lColour2;
    maColours[3] = lColour3;

    mafLifetimes[0] = lLifetimes.x;
    mafLifetimes[1] = lLifetimes.y;
    mafLifetimes[2] = lLifetimes.z;
    mafLifetimes[3] = lLifetimes.w;

    mfGravityStrength           = lfGravityStrength;
    mfBounceStrength            = lfBounceStrength;
    mfMotionBlurTime            = lfMotionBlurTime;
    mfSparkRadius               = lfSparkRadius;
    mfDragInitialVelocityScale  = lfDragInitialVelocityScale;
    mfDragTerminalVelocityScale = lfDragTerminalVelocityScale;
    mfDragDuration              = lfDragDuration;
    mpcSparkTextureName         = lpcSparkTextureName;
}

// =============================================================================================
// SparkArray::SpawnSpark @0x822955E0 -- launch one spark.
//
// ============================ WHAT THE CONSOLE ACTUALLY COMPUTES =============================
// A spark is stored ONCE and then evaluated analytically at draw time (SparkArray::RenderBank ->
// CalculateSparkPosition), so this body has to solve the spark's whole flight up front. The two
// w lanes of the two stored Vector4s are NOT padding: they carry that solve.
//
// The flight model has two phases, driven by the three drag parameters:
//     phase 1, t in [0, mfDragDuration]: the vertical velocity scale ramps linearly from
//              mfDragInitialVelocityScale to mfDragTerminalVelocityScale;
//     phase 2, t >  mfDragDuration:      the scale stays at mfDragTerminalVelocityScale.
// Each phase gives a quadratic in t for "when does this spark reach the plane lfHeightAbovePlane
// below it", and the console tries phase 1 first, then phase 2:
//
//     aRamp = (term - init) * velocity.y / (2 * duration)  +  gravity / 2
//     bRamp = velocity.y * init
//     cRamp = lfHeightAbovePlane
//     discRamp = bRamp*bRamp - 4*aRamp*cRamp                       ; fmsubs f8, f10, f10, f8
//     tRamp    = (-bRamp - sqrt(discRamp)) / (2*aRamp)             ; accepted iff <= duration
//
//     aTerm = gravity / 2
//     bTerm = velocity.y * term
//     cTerm = lfHeightAbovePlane - (term - init) * velocity.y * duration / 2
//     discTerm = bTerm*bTerm - 4*aTerm*cTerm                       ; fmsubs f30, f6, f6, f30
//     tTerm    = (-bTerm - sqrt(discTerm)) / (2*aTerm)             ; accepted iff >= duration
//
// If a root is accepted, the w lanes are filled with the bounce state at that root:
//     mVelocity.w = the post-bounce vertical velocity (-mfBounceStrength scaled),
//     mPosition.w = the y the spark reaches at the bounce.
// If NEITHER root is accepted the console leaves both w lanes at what the entry permute put
// there -- see the note on that permute below -- and the spark simply never bounces.
//
// ============================ THE ENTRY PERMUTE ==============================================
// Both incoming Vector3s are widened to Vector4 through the engine-wide lane-insert control at
// [unk_8327F140 + lane*0x40 + srcword*0x10] with offset 0xD0 == lane 3, source word 1, i.e.
// `w := y`. That is the SDK's own Vector3 -> Vector4 widening (the same table Wheel.cpp,
// InterpedParam3.cpp and BrnMainMap.cpp document), and it is what both w lanes hold on the
// no-bounce path.
//
// ============================ WHAT IS *NOT* MODELLED =========================================
// [!] The tail's `vcmpeqfp. v12, v11, v12` (splat(pos.w) vs splat(pos.y)) writes its CR bits to a
//     stack slot that is never read again -- a debug predicate whose consumer the optimiser
//     removed. It has no effect and is not reproduced.
// =============================================================================================
void SparkArray::SpawnSpark(rw::math::vpu::Vector3::InParam lPosition,
                            rw::math::vpu::Vector3::InParam lVelocity,
                            f32  lfSize,
                            f32  lfCurrentTime,
                            f32  lfTimeSinceEvent,
                            f32  lfBirthTimeOffset,
                            f32  lfHeightAbovePlane,
                            bool lbIsCrashSpark)
{
    // The console's own two NaN self-checks, BrnSparkRenderer.cpp:416 and :417.
    CGS_ASSERT(lfHeightAbovePlane == lfHeightAbovePlane, "lrHeightAbovePlane == lrHeightAbovePlane");
    CGS_ASSERT(mfGravityStrength == mfGravityStrength, "mfGravityStrength == mfGravityStrength");

    SparkBank& lrBank = lbIsCrashSpark ? mCrashBank : mRegularBank;

    // fsubs f0, f30, f29 ; fadds f1, f0, f28 -- the birth time handed to GetNewSpark.
    const f32 lfBirthTime = (lfCurrentTime - lfTimeSinceEvent) + lfBirthTimeOffset;

    BrnSpark* const lpSpark = lrBank.GetNewSpark(lfBirthTime);
    if (lpSpark == 0)
        return;

    const f32 KF_HALF = 0.5f;   // flt_82001DA0
    const f32 KF_TWO  = 2.0f;   // flt_82001D9C
    const f32 KF_FOUR = 4.0f;   // flt_82004EF4
    const f32 KF_ONE  = 1.0f;   // flt_82001C98

    const f32 lfInitialScale  = mfDragInitialVelocityScale;   // +0x80
    const f32 lfTerminalScale = mfDragTerminalVelocityScale;  // +0x84
    const f32 lfScaleDelta    = lfTerminalScale - lfInitialScale;
    const f32 lfDragDuration  = mfDragDuration;               // +0x88
    const f32 lfGravity       = mfGravityStrength;            // +0x70
    const f32 lfVelocityY     = lVelocity.y;                  // lfs f0, arg_34 -- v2's homed y

    // The SDK Vector3 -> Vector4 widening (w := y) both stores start from.
    rw::math::vpu::Vector4 lPositionOut = { lPosition.x, lPosition.y, lPosition.z, lPosition.y };
    rw::math::vpu::Vector4 lVelocityOut = { lVelocity.x, lVelocity.y, lVelocity.z, lVelocity.y };

    const f32 lfHalfGravity = lfGravity * KF_HALF;

    const f32 lfRampA = ((lfScaleDelta * lfVelocityY) / (lfDragDuration * KF_TWO)) + lfHalfGravity;
    const f32 lfRampB = lfVelocityY * lfInitialScale;
    const f32 lfTermB = lfVelocityY * lfTerminalScale;
    const f32 lfTermC = lfHeightAbovePlane - ((lfScaleDelta * lfVelocityY * lfDragDuration) * KF_HALF);

    const f32 lfRampDiscriminant = (lfRampB * lfRampB) - (KF_FOUR * (lfRampA * lfHeightAbovePlane));
    const f32 lfTermDiscriminant = (lfTermB * lfTermB) - (KF_FOUR * (lfTermC * lfHalfGravity));

    bool lbSolved = false;

    if (lfRampDiscriminant >= 0.0f)
    {
        const f32 lfRampTime = (-lfRampB - std::sqrt(lfRampDiscriminant)) / (lfRampA * KF_TWO);
        if (!(lfRampTime > lfDragDuration))
        {
            // ---- phase 1: the bounce happens inside the drag ramp -------------------------
            const f32 lfNegBounce   = -mfBounceStrength;                          // +0x74
            const f32 lfScaleAtTime = (lfScaleDelta * lfRampTime) + (lfDragDuration * lfInitialScale);

            lVelocityOut.w = (lfVelocityY * lfNegBounce)
                           + ((((lfNegBounce - KF_ONE) * lfRampTime) * lfDragDuration * lfGravity)
                              / lfScaleAtTime);

            const f32 lfVelocityDelta = lfVelocityY - lVelocityOut.w;
            lPositionOut.w = lPosition.y
                           + ((lfVelocityDelta * lfRampTime) * lfInitialScale)
                           + ((((lfVelocityDelta * lfScaleDelta) * lfRampTime) * lfRampTime)
                              / (lfDragDuration * KF_TWO));
            lbSolved = true;
        }
    }

    if (!lbSolved && lfTermDiscriminant >= 0.0f)
    {
        const f32 lfTermTime = (-lfTermB - std::sqrt(lfTermDiscriminant)) / (lfHalfGravity * KF_TWO);
        if (!(lfTermTime < lfDragDuration))
        {
            // ---- phase 2: the bounce happens after the drag ramp --------------------------
            const f32 lfNegBounce = -mfBounceStrength;
            const f32 lfRampArea  = ((lfScaleDelta * lfDragDuration) * KF_HALF)
                                  - (lfTermTime * lfTerminalScale);

            // fdivs f13, f13, f9 -- f9 is mfDragTerminalVelocityScale (loaded at 0x822956B4 and
            // untouched on this path), i.e. the velocity scale at t. Phase 1 spells the same
            // quantity as (scaleDelta*t + duration*init) / duration, so both arms are
            // (-bounce - 1) * t * gravity / scale(t).
            lVelocityOut.w = ((((lfNegBounce - KF_ONE) / lfTerminalScale) * lfTermTime) * lfGravity)
                           + (lfVelocityY * lfNegBounce);

            lPositionOut.w = lPosition.y + (lfRampArea * (lVelocityOut.w - lfVelocityY));
        }
    }

    lpSpark->mfSize    = lfSize;
    lpSpark->mPosition = lPositionOut;
    lpSpark->mVelocity = lVelocityOut;
}

// =============================================================================================
// SparkRenderer::Construct -- inlined into ParticleModule::Prepare as the single
// `stwx r27, r31, 0x94C0`. The heap argument the DWARF declares is unread by the X360 body
// (identical to TrailRenderer::Construct beside it).
// =============================================================================================
void SparkRenderer::Construct(CgsMemory::HeapMalloc* lpHeapMalloc, CgsGraphics::Im3d* lpRenderer)
{
    (void)lpHeapMalloc;
    mpRenderer = lpRenderer;
}

} // namespace Native
} // namespace BrnParticle
