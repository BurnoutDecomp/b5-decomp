#include "GameSource/Effects/Particles/ParticleModuleBringUp.h"

#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"   // BrnGame::DispatchThreadInputBuffer + ParticleRenderData
#include "GameSource/Director/Camera/Camera.h"              // BrnDirector::Camera::Camera (+ CameraState / CameraEffects)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log::WriteToLog (the one-shot proof line)

#include <cstdio>   // snprintf (the one-shot proof line)
#include <cmath>    // std::isfinite (the one-shot proof line's finiteness check)

// ==================================================================================================
// [FLAG PC bring-up] THE ParticleRenderData PRODUCER.
//
// STANDS IN FOR, function for function:
//   BrnParticle::ParticleModule::Update                 @0x822817D8  (DWARF ParticleModule.h:434,
//                                                        `virtual void Update(float32_t, float32_t,
//                                                         float32_t, const Camera*)`)
//   BrnParticle::ParticleModule::GenerateRenderRequests @0x82281BD8  (DWARF ParticleModule.h:573,
//                                                        `void GenerateRenderRequests(
//                                                         const DispatchInputBuffer*,
//                                                         DispatchThreadInputBuffer*)`)
// and the write-lock bracket their caller BrnEffects::EffectsModule::GenerateDispatchLists
// @0x82296668 puts round the second one:
//     CgsModule::IOBuffer::LockForWrite(a4);
//     BrnParticle::ParticleModule::GenerateRenderRequests(a1 + 2688, particleIn, a4);
//     CgsModule::IOBuffer::UnlockForWrite(a4);
//
// WHY IT IS HERE AT ALL. On the console the record lives on the particle module itself, at
// ParticleModule+0x8E00 (the pointer-free 528-byte ParticleRenderData); Update refreshes it once per
// simulation sub-step and GenerateRenderRequests memcpy's it into the dispatch-thread input buffer
// once per frame. Neither the particle module nor the effects module exists on this build (see the
// header's pasted grep of the build list), so BrnRendererModule::Render has been passing a NULL
// render-data pointer to BrnRendererUpdatePostFxMotionBlur since the rung-7 producers wave and
// MotionBlurState has been sitting on Construct's identity/identity WVP pair. This TU is the
// missing half.
//
// THE TWO-PHASE SHAPE IS KEPT, and it is load-bearing rather than cosmetic: mfCurrentTimeStep is an
// ACCUMULATOR that lives on the MODULE, not on the IO buffer, and the IO buffer is DOUBLE-BUFFERED
// and re-Constructed on every Swap. Accumulating into the buffer would alternate between two
// independent accumulators. So the module-side record is modelled here as a file-static and the
// buffer gets a whole-record copy, exactly as the console does.
//
// ⚠ THE COPY IS AN ASSIGNMENT, NOT A 528-BYTE memcpy. 0x210 == 528 is the GUEST size of
// ParticleRenderData (32-bit pointers); on the x64 host mpParticleModule and mpEnvironmentMap widen
// and CgsGraphics::Camera is 368 bytes, so the host size differs. `*dst = src` copies by member
// (and routes mCgsCamera through the real CgsGraphics::Camera::operator= @0x82218ED0, which is
// bodied in CgsCamera.cpp and on the build list). AGENTS.md rule 1.
//
// FIELD BY FIELD, against the asm (every instruction address below is from
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x822817D8.json and /0x82281BD8.json):
//
//   ParticleModule::Update @0x822817D8 -- r3 = module, f1/f2/f3 = the three floats, r7 = the Camera*
//   (the PPC float-argument rule: each float SKIPS its GPR slot, so the camera lands in r7 and NOT
//   in r4; Hex-Rays' `(a1, a2..a4 doubles, a5..a8 ints)` prototype is wrong, AGENTS.md rule 4):
//     0x82281814  lfsx  f0, r31, 0x230F0     scale   = module->mfTimeStepScale  (Construct: 1.0f)
//     0x8228181C  fmuls f31, f0, f2          f31     = scale * f2               (the TIME)
//     0x82281824  fmuls f29, f0, f1          f29     = scale * f1               (the TIME STEP)
//     0x8228182C  lfs   f0, flt_8200DCD4     3000.0f (DATA_DUMP.md flt_8200DCD4 lane 0 = 0x453B8000)
//     0x82281830  fctiwz / stfiwx 0, r10     **(module+0x53E0) = (s32)(scale * f2 * 3000)
//                                            -- a CgsDev perf-monitor slot. DROPPED here: the slot
//                                            is a pointer into the module's own monitor block,
//                                            which does not exist on PC. Nothing else reads it.
//     0x82281838  bl CopyToCgsCamera(r3=camera, r4=module+0x8E60)
//                                            -> record.mCgsCamera  (the console's own call; the
//                                               committed Camera::CopyToCgsCamera, Camera.cpp:505)
//     0x82281868  stfsx f30(=f3), r31, 0x8E10 -> record.mfTimeStepMultiplier = f3
//     0x8228185C/0x82281864/0x82281870
//                 lfs / fadds / stfs @module+0x8E0C
//                                            -> record.mfCurrentTimeStep += scale * f1
//                                               ⚠ AN ACCUMULATOR, not an assignment. Construct
//                                               @0x82294220 seeds it (`*(a1 + 36364) = 0.0`) and
//                                               NOTHING in the image ever resets it: an image-wide
//                                               scan for stores to +0x8E0C finds exactly three
//                                               functions --
//                                                 0x822817D8 Update           (this += )
//                                                 0x82294220 Construct        (= 0.0)
//                                                 0x82294760 PreRenderUpdate  (READS it into
//                                                            DispatchThreadUpdateData+4)
//                                               so it is a monotonically growing sum of scaled sim
//                                               steps. Downstream only its NON-ZERO-NESS matters:
//                                               MotionBlurState::Update @0x823F8490 branches on
//                                               `lfTimeStep == 0.0f` and nothing else reads it.
//     0x82281878  stfsx f31, r31, 0x8E08     -> record.mfCurrentTime = scale * f2
//     0x82281880-0x822818AC  4x lvx128/stvx128 from r30(=camera+0x00,0x10,0x20,0x30)
//                                            -> record.mCameraTransform = camera->mTransform
//                                               (Camera.h pins mTransform at +0x00 with a
//                                                static_assert; 64 bytes == Matrix44Affine)
//     0x822818B0-0x822819F8  the muFlags rebuild -- see BuildRenderDataFlags below.
//
//   ParticleModule::GenerateRenderRequests @0x82281BD8 -- r3 = module, r4 = the PARTICLE dispatch
//   input buffer, r5 = the DispatchThreadInputBuffer (Hex-Rays prints r5 as `a3`):
//     0x82281BFC-0x82281C34  3x lvx128/stvx128 r4+0x10/+0x20/+0x30 -> module+0x8FD0/+0x8FE0/+0x8FF0
//                                            -> record.mvSunDirection / mvSunColour / mvAmbientColour
//     0x82281C38/0x82281C44  lwz  r4+0x40 -> module+0x9004   -> record.mpEnvironmentMap
//     0x82281C50/0x82281C58  lfs  r4+0x44 -> module+0x9008   -> record.mfWhiteLevel
//                                            ⚠ ALL FIVE ARE BLOCKED ON PC -- see the FLAG below.
//     0x82281C4C-0x82281C78  if (module+0x23137) { module+0x23137 = 0; muFlags |= 1; }
//                                            -> the CAMERA-SWITCHED latch, consumed here
//     0x82281C80-0x82281C9C  if (!*(dispatchThreadInput + 0x99B0)) muFlags |= 0x40
//                                            -> +0x99B0 == 39344 == mbIsRenderingAtFullFrameRate
//                                               (BrnDispatchThreadInputBuffer.h:192, and the same
//                                               byte Render reads for the present-interval select)
//     0x82281CA0-0x82281CB4  ++*(module+0x8E04)          -> ++record.muCurrentFrame
//     0x82281CB8-0x82281CC8  memcpy(GetParticleRenderData(r5), module+0x8E00, 0x210)
//
// ⚠ FIVE FIELDS ARE BLOCKED, DELIBERATELY LEFT AT THE RECORD'S OWN ZERO IMAGE, AND NOTHING READS
// THEM. Their console source is the PARTICLE dispatch input buffer that
// BrnEffects::EffectsModule::GenerateDispatchLists @0x82296668 builds one statement earlier, out of
// the EFFECTS dispatch input buffer:
//     particleIn+0x10 <- effectsIn+0x20  mKeyLightDirection        -> record.mvSunDirection
//     particleIn+0x20 <- effectsIn+0x30  mKeyLightColour           -> record.mvSunColour
//     particleIn+0x30 <- effectsIn+0x40  mAverageIrradianceColour  -> record.mvAmbientColour
//     particleIn+0x40 <- GetEnvironmentMap(effectsIn)              -> record.mpEnvironmentMap
//     particleIn+0x44 <- GetWhiteLevel(effectsIn)                  -> record.mfWhiteLevel
// (member names from BrnEffectsModuleIO_DispatchInputBuffer.h:88-95). NO BrnEffects::EffectsIO::
// DispatchInputBuffer IS EVER CREATED ON THIS BUILD -- the type has a committed header and accessor
// bodies but no instance and no build-list entry:
//   $ grep -rn "EffectsIO::DispatchInputBuffer" b5-decomp/src --include=*.cpp
//   .../BrnEffectsModuleIO_DispatchInputBuffer.cpp:5       (the accessor bodies' own banner)
//   .../BrnEffectsModuleIO_DispatchInputBuffer_IOHelper.cpp:7,25   (the IOHelper ctor)
//   .../BrnRendererModule.cpp:335,349,1173                 (comments on the camera stand-in)
//   $ grep -n "BrnEffectsModuleIO_DispatchInputBuffer" tools/build/build_game_exe.bat
//   (no output -- not on the build list)
// -- so there is no live value to copy, and INVENTING one would be exactly the fabrication AGENTS.md
// rule 6 forbids. Nothing in this tree reads them off a ParticleRenderData either:
//   $ grep -rn "mvSunDirection\|mvSunColour\|mvAmbientColour" b5-decomp/src --include=*.cpp --include=*.h
//   (no output outside ParticleModule.h's own declaration)
// and the only consumer of the whole record, BrnRendererUpdatePostFxMotionBlur, reads exactly
// mCgsCamera.mView / mCgsCamera.mProjection / mfCurrentTimeStep. So the five stay zero and are
// listed as BLOCKED.
// DELETE-WHEN: the world's live key light is published for PC consumers -- i.e. when
// EnvironmentManager::GenerateShaderConstants @0x827D0098 + WorldModule::
// SetupShaderConstantsBeforeRendering @0x827D1410 retire PublishWorldShadingConstantsBringUp
// (BrnWorldModule.cpp:4308), which is the same wave as this file. Take mvSunDirection/mvSunColour
// from the key-light pair that publishes and mvAmbientColour from the irradiance rig's constant
// term, and the white level from EnvironmentManager::mfWhiteLevel.
//
// ⚠ CADENCE DEVIATION, FLAGGED. On the console Update runs once per SIMULATION SUB-STEP (from
// EffectsModule::Update, inside the module scheduler's per-sub-step walk) while
// GenerateRenderRequests runs once per FRAME (from EffectsModule::GenerateDispatchLists). This
// producer runs BOTH once per frame, from DoDispatch. Two consequences, both bounded:
//   * mfCurrentTimeStep accumulates ONE sim step per frame instead of miNumSimFramesRequired of
//     them, so it grows more slowly than the console's would. It is a monotonically rising number
//     whose only consumer tests it against 0.0f, so the difference is unobservable here.
//   * mfCurrentTime is an ASSIGNMENT of the sim clock, so it is exact either way.
// DELETE-WHEN the module scheduler drives the real per-sub-step Update.
//
// ⚠ STENCIL-MASK DEVIATION (inherited from rung 7, NOT fixed here). The console's composite blurs
// CARS and WORLD separately behind the stencil mask BrnPostFx writes; this tree's composite has no
// stencil mask yet, so a real, non-zero velocity blurs the WHOLE frame uniformly. That is a visual
// deviation the moment this producer goes live, and it is deliberate: the fix is the stencil mask,
// not a weaker velocity. See BrnRendererModulePostFx.cpp's MotionBlurState::Update banner.
// ==================================================================================================

namespace BrnParticle
{
namespace
{
    typedef BrnParticle::ParticleModule::ParticleRenderData RenderData;

    // ----------------------------------------------------------------------------------------
    // THE MODULE-SIDE STATE, all of it attested by BrnParticle::ParticleModule::Construct
    // @0x82294220 -- the only place any of it is initialised. Its pseudocode, filtered to the
    // offsets this producer needs:
    //     *(a1 + 143600) = 1.0;      // +0x230F0  the time-step scale
    //     *(a1 + 143664) = 1;        // +0x23130  \.
    //     *(a1 + 143665) = 1;        // +0x23131   |
    //     *(a1 + 143666) = 1;        // +0x23132   |- the five per-system enable bytes, ALL 1
    //     *(a1 + 143667) = 1;        // +0x23133   |
    //     *(a1 + 143668) = 1;        // +0x23134  /
    //     *(a1 + 143671) = 1;        // +0x23137  the camera-switched latch, seeded SET
    //     *(a1 + 36352)  = a1;       // +0x8E00   mpParticleModule = the module itself
    //     *(a1 + 36356)  = 0;        // +0x8E04   muCurrentFrame
    //     *(a1 + 36364)  = 0.0;      // +0x8E0C   mfCurrentTimeStep (the accumulator's seed)
    // An image-wide scan of the 30k exports for `ori rN, rM, 0x30F0|0x3130..0x3134|0x3137`
    // (the way the module forms each of these offsets) plus the matching Hex-Rays decimal forms
    // (143600 / 143664..143668 / 143671) finds exactly three ParticleModule bodies:
    //     0x82294220  ParticleModule::Construct              writes ALL of them
    //     0x822817D8  ParticleModule::Update                 reads the five enables + the scale,
    //                                                        writes only +0x23137
    //     0x82281BD8  ParticleModule::GenerateRenderRequests clears +0x23137
    // (the scan's other hits -- CheckSupportedCCFormat, ClassifyCCConversion, SetSequencePointers,
    //  two unnamed subs and MainDirector::UpdateDebugInfo -- are the same IMMEDIATE against a
    //  different base register, i.e. a different object, not this module.)
    // So on PC these are fixed at Construct's values. Rule 7(b): only values the shipped data
    // attests. FLAG: whatever UI would have toggled the five enable bytes (a debug component)
    // is not identified here; it simply does not exist on PC either way.
    // ----------------------------------------------------------------------------------------

    // +0x230F0. FLAG: the module does not exist on PC, so nothing can ever change it; 1.0f is
    // Construct's value and therefore the only value this build can observe.
    const f32 KF_TIME_STEP_SCALE = 1.0f;

    // The five enable bytes, at Construct's values. Named after the muFlags bit each one gates
    // (the mapping is Update's own store order, not the byte order):
    //   +0x23130 -> eRenderDataFlagRenderSparks (2)   +0x23131 -> eRenderDataFlagRenderTrails (0x20)
    //   +0x23132 -> eRenderDataFlagRenderDebris (4)   +0x23133 -> eRenderDataFlagRenderSimple (8)
    //   +0x23134 -> eRenderDataFlagRenderLion  (0x10)
    const bool KB_RENDER_SPARKS = true;   // +0x23130
    const bool KB_RENDER_TRAILS = true;   // +0x23131
    const bool KB_RENDER_DEBRIS = true;   // +0x23132
    const bool KB_RENDER_SIMPLE = true;   // +0x23133
    const bool KB_RENDER_LION   = true;   // +0x23134

    // BrnDirector::Camera::CameraState flag indices. The console reads them as
    // `ld r11, 0x140(r30)` (== &Camera::mState.mCurrentFlags, the 64-bit BitArray<30>) followed by
    // `rlwinm r11, r11, 0, 29, 29` (mask bit 61 of the doubleword == value 4 == BitArray index 2)
    // and `rlwinm r11, r11, 0, 25, 25` (mask bit 57 == value 0x40 == BitArray index 6). Hex-Rays
    // renders the same two as `*(camera + 324) & 4` / `& 0x40` -- i.e. the low word of that
    // doubleword, which is the same bits on a big-endian load. Reached BY NAME here through
    // CameraState::IsFlagSet so nothing forms camera+324 on the host, where mCurrentFlags is one
    // u64 field and +324 is not it.
    // The NAMES are the DecFIGS DWARF's own enumerators for CameraState::EFlag
    // (references/DecFIGS/dwarfdump/GameSource/Director/Camera/BrnCameraState.h:9-39, E_FLAG_COUNT
    // == 30 == the committed CameraState::KU_NUM_FLAGS, so the two enumerations are the same one).
    // FLAG (cross-group): the committed BrnCameraState.h does not carry the EFlag enum yet, so the
    // two indices are named here instead of being spelled CameraState::E_FLAG_HIDE_PLAYER /
    // ::E_FLAG_NEW_THIS_FRAME. DELETE-WHEN that enum lands on the type.
    const u32 KU_CAMERA_FLAG_HIDE_PLAYER   = 2;   // DWARF BrnCameraState.h:11
    const u32 KU_CAMERA_FLAG_NEW_THIS_FRAME = 6;  // DWARF BrnCameraState.h:15

    // The slow-motion window, off the camera's requested sim-time scale
    // (CameraEffects::mfSimTimeScale, BrnCameraEffects.h:118 == camera +0x104 == the +260 Hex-Rays
    // prints). Both constants are read out of the SAME rodata run the DATA_DUMP pins:
    //   flt_8200DB9C @0x8200DB9C = 0x3D088889 = 0.0333333f  (1/30)   -- xref 0x822819A4 (this fn)
    //   flt_8200DBA0 @0x8200DBA0 = 0x3E924925 = 0.2857143f  (2/7)    -- xref 0x82281820/0x822819B0
    // and both are corroborated at the WRITER end: ArbStateTakedown stores 2/7 into mfSimTimeScale
    // (the takedown slow-mo) and MomentHardStop::Update @0x82271788 stores 0.005..0.01 (the crash
    // ULTRA slow-mo) -- so the upper constant is exactly the takedown value and the lower one
    // separates takedown slow-mo from ultra slow-mo.
    // (the literals are the EXACT decimal forms of those two words -- 0.0333333f would assemble
    //  to 0x3D088880, one ulp low, which the gate's `__real@` symbol would show)
    const f32 KF_ULTRA_SLOW_MOTION_TIME_SCALE = 0.033333335f;   // flt_8200DB9C == 0x3D088889
    const f32 KF_SLOW_MOTION_TIME_SCALE       = 0.2857143f;     // flt_8200DBA0 == 0x3E924925

    // ----------------------------------------------------------------------------------------
    // The module's record (ParticleModule+0x8E00). File-static so the mfCurrentTimeStep
    // accumulator and the camera-switched latch survive the IO buffer's per-frame
    // Destruct/Construct and its double-buffer Swap -- exactly as they do on the console, where
    // they live on the module and only a COPY reaches the buffer.
    //
    // Static storage means the whole record is zero before anything runs, which covers three of
    // Construct's four record stores (muCurrentFrame = 0, mfCurrentTimeStep = 0.0f) and leaves
    // mpParticleModule at NULL rather than at the module `this` the console writes -- correct,
    // because there is no module. Nothing in this tree dereferences it:
    //   $ grep -rn "mpParticleModule" b5-decomp/src --include=*.cpp --include=*.h
    //   -> BrnEffectsGlassManager.{h,cpp} and ParticleEffectHelper.h only, and those are that
    //      class's OWN `BrnParticle::ParticleModule*` member, not this record's slot.
    // ----------------------------------------------------------------------------------------
    RenderData gRenderData;

    // +0x23137, seeded SET by Construct (`*(a1 + 143671) = 1`) -- so the FIRST record the console
    // ever publishes carries eRenderDataFlagCameraSwitched. Reproduced.
    bool gbCameraSwitched = true;

    // Host-only. See the header for why this is not a member of either console type.
    // ⚠ PER BUFFER INSTANCE, not one global bit (step-9 verify, finding 1): the input buffer is
    // DOUBLE-BUFFERED -- DispatchThreadInputBufferManager::Swap alternates mapBuffers[0]/[1] every
    // host frame and re-Constructs the new write buffer WITHOUT clearing mParticleRenderData -- and
    // the producer only runs on the frames DoDispatch runs (E_MGS_IN_GAME). A global latch would go
    // true after the FIRST stamp and then vouch for the OTHER buffer, whose payload is still
    // uninitialised bytes, on the very next frame the producer skips. So the latch remembers WHICH
    // buffer instances have ever been stamped, and the renderer asks about the one it read-locked.
    // (After production stops, both instances hold valid records one frame apart; the renderer
    // then feeds MotionBlurState::Update alternating stale views and the pair converges to a zero
    // velocity within two calls -- finite and stale, never garbage.)
    const BrnGame::DispatchThreadInputBuffer* gapStampedBuffers[2] = { 0, 0 };
    bool gbProduced = false;   // "first record ever" -- drives the one-shot diagnostic below only

    void RememberStampedBuffer(const BrnGame::DispatchThreadInputBuffer* lpBuffer)
    {
        for (u32 li = 0; li < 2; ++li)
        {
            if (gapStampedBuffers[li] == lpBuffer)
                return;
            if (gapStampedBuffers[li] == 0)
            {
                gapStampedBuffers[li] = lpBuffer;
                return;
            }
        }
        // A third distinct instance means the manager was re-created; forget the oldest pair.
        gapStampedBuffers[0] = lpBuffer;
        gapStampedBuffers[1] = 0;
    }

    // ------------------------------------------------------------------------------------
    // Update @0x822817D8, asm 0x822818B0-0x822819F8 -- the muFlags rebuild, de-optimised into
    // one local and one store (the console zeroes the halfword at module+0x9000 and then ORs
    // into memory five times; single-threaded, that is the same value).
    // ------------------------------------------------------------------------------------
    u16 BuildRenderDataFlags(const BrnDirector::Camera::Camera& lrCamera)
    {
        // 0x822818B4 `sth 0` then 0x822818C0 `li r11, 2` / `sth` -- an ASSIGN, not an OR, and it
        // is first, so the two spellings agree.
        u16 luFlags = 0;
        if (KB_RENDER_SPARKS)                                   // lbzx +0x23130
            luFlags = RenderData::eRenderDataFlagRenderSparks;   // = 2
        if (KB_RENDER_LION)                                     // lbzx +0x23134
            luFlags |= RenderData::eRenderDataFlagRenderLion;    // |= 0x10
        if (KB_RENDER_DEBRIS)                                   // lbzx +0x23132
            luFlags |= RenderData::eRenderDataFlagRenderDebris;  // |= 4
        if (KB_RENDER_SIMPLE)                                   // lbzx +0x23133
            luFlags |= RenderData::eRenderDataFlagRenderSimple;  // |= 8

        // 0x82281928-0x82281968: trails need their enable byte AND the camera NOT hiding the
        // player. The double-negation the asm spells out (`li 1 / bne -> li 0 / clrlwi / bne ->
        // skip`) is just "flag set -> skip the OR".
        const BrnDirector::Camera::CameraState& lrState = lrCamera.GetState();
        if (KB_RENDER_TRAILS && !lrState.IsFlagSet(KU_CAMERA_FLAG_HIDE_PLAYER))
            luFlags |= RenderData::eRenderDataFlagRenderTrails;  // |= 0x20

        // 0x8228196C-0x8228199C: the camera-switched LATCH is raised here and consumed one
        // function later, in GenerateRenderRequests. It is not a flag of this record yet.
        if (lrState.IsFlagSet(KU_CAMERA_FLAG_NEW_THIS_FRAME))
            gbCameraSwitched = true;

        // 0x822819A0-0x822819F8: the slow-motion bit, from the camera's requested sim-time scale.
        // The asm computes TWO predicates and ORs them:
        //   p1 = (scale >  KF_ULTRA) && (scale <= KF_SLOW)     the takedown-slow-mo band
        //   p2 = (scale <= KF_ULTRA)                           the crash ultra-slow-mo arm
        // For any non-NaN scale `p1 || p2` reduces to `scale <= KF_SLOW`, but the two-predicate
        // form is what the binary contains (it re-loads camera+0x104 for p2 at 0x822819D0), so it
        // is what is written -- collapsing it would change the NaN behaviour and lose the two
        // named bands.
        const f32 lfSimTimeScale = lrCamera.GetEffects().mfSimTimeScale;
        const bool lbInSlowMotion =
            (lfSimTimeScale > KF_ULTRA_SLOW_MOTION_TIME_SCALE)
            && (lfSimTimeScale <= KF_SLOW_MOTION_TIME_SCALE);
        const bool lbInUltraSlowMotion = (lfSimTimeScale <= KF_ULTRA_SLOW_MOTION_TIME_SCALE);
        if (lbInSlowMotion || lbInUltraSlowMotion)
            luFlags |= RenderData::eRenderDataFlagInSlowMotion;  // |= 0x80

        return luFlags;
    }

    // ------------------------------------------------------------------------------------
    // ParticleModule::Update @0x822817D8, on the module-side record.
    // ------------------------------------------------------------------------------------
    void ParticleModuleUpdateBringUp(RenderData&                         lrRenderData,
                                     const BrnDirector::Camera::Camera&  lrCamera,
                                     f32                                 lfTimeStep,
                                     f32                                 lfTime,
                                     f32                                 lfTimeStepMultiplier)
    {
        // 0x82281830-0x82281834 -- the perf-monitor store `**(module+0x53E0) =
        // (s32)(scale * f2 * 3000)` is DROPPED (no module, no monitor block, no reader).

        // 0x82281838 -- the console's own call, `CopyToCgsCamera(camera, module + 0x8E60)`.
        lrCamera.CopyToCgsCamera(&lrRenderData.mCgsCamera);

        // 0x82281868 / 0x82281870 / 0x82281878, in the asm's own store order.
        lrRenderData.mfTimeStepMultiplier = lfTimeStepMultiplier;                 // +0x10 <- f3
        lrRenderData.mfCurrentTimeStep   += KF_TIME_STEP_SCALE * lfTimeStep;      // +0x0C <- += f1
        lrRenderData.mfCurrentTime        = KF_TIME_STEP_SCALE * lfTime;          // +0x08 <-  f2

        // 0x82281880-0x822818AC -- the four 16-byte rows of camera+0x00..0x3F, i.e. mTransform
        // (Camera.h static_asserts offsetof(Camera, mTransform) == 0x00). Reached by name.
        lrRenderData.mCameraTransform = lrCamera.GetTransform();

        // 0x822818B0-0x822819F8.
        lrRenderData.muFlags = BuildRenderDataFlags(lrCamera);
    }

    // ------------------------------------------------------------------------------------
    // ParticleModule::GenerateRenderRequests @0x82281BD8, on the module-side record + the
    // destination buffer. The caller owns the write lock (EffectsModule::GenerateDispatchLists
    // @0x82296668) -- here it is taken by the entry point below, immediately round this call.
    // ------------------------------------------------------------------------------------
    void ParticleModuleGenerateRenderRequestsBringUp(RenderData&                        lrRenderData,
                                                     BrnGame::DispatchThreadInputBuffer* lpBuffer)
    {
        // 0x82281BFC-0x82281C58 -- the five copies out of the particle dispatch input buffer.
        // BLOCKED (no EffectsIO::DispatchInputBuffer on this build); see the banner.

        // 0x82281C4C-0x82281C78 -- consume the camera-switched latch.
        if (gbCameraSwitched)
        {
            gbCameraSwitched = false;
            lrRenderData.muFlags |= RenderData::eRenderDataFlagCameraSwitched;   // |= 1
        }

        // 0x82281C80-0x82281C9C -- `!*(dispatchThreadInput + 0x99B0)`. +0x99B0 == 39344 ==
        // mbIsRenderingAtFullFrameRate (BrnDispatchThreadInputBuffer.h:192). The accessor is a
        // plain member read with no lock assert, exactly like the console's lbzx.
        if (!lpBuffer->GetIsRenderingAtFullFrameRate())
            lrRenderData.muFlags |= RenderData::eRenderDataFlagReducedFrameRate; // |= 0x40

        // 0x82281CA0-0x82281CB4.
        ++lrRenderData.muCurrentFrame;

        // 0x82281CB8-0x82281CC8 -- memcpy(GetParticleRenderData(buffer), module+0x8E00, 0x210),
        // as a host member-wise assignment (see the banner's guest-size note). The accessor is
        // the WRITE-locked overload (@0x8227F6E8) and it asserts the lock, which is why the
        // bracket in the entry point below is not optional.
        *lpBuffer->GetParticleRenderData() = lrRenderData;
    }
}

// ==================================================================================================
// The entry point. Ordering, proved from the PC frame spine rather than assumed:
//
//   BrnMain.cpp EngineUpdate (:210-224)
//     gGameModule.UpdateThread()      -> GameMain (:2058 area) -> ... -> the active flow state's
//                                        Render() -> MainGameFlowStateInGame::Render ->
//                                        BrnGameModule::DoDispatch()   <-- THIS PRODUCER RUNS HERE,
//                                                                          writing GetWriteBuffer()
//     gGameModule.OnEndOfUpdateFrame()-> mDispatchThreadInputBufferManager.Swap()
//                                        (the buffer just written BECOMES the read buffer)
//     gGameModule.DispatchThread()    -> mRenderModule.Render(GetReadBuffer())
//                                        -> LockForRead for the whole frame (BrnRendererModule.cpp
//                                           :2679-2680 .. :3119-3120) and, inside it,
//                                           BrnRendererUpdatePostFxMotionBlur
//
// So the renderer sees THIS frame's camera, not the previous one. The console is one frame LATE by
// comparison -- there the particle module runs on the update thread while the render thread is
// still walking the previously published buffer -- but the record it reads is likewise "the last
// buffer the update side published", i.e. the same logical slot; the PC host loop is simply
// serialised, so the publish and the read land in the same wall-clock frame. The motion-blur
// reprojection differences two CONSECUTIVE published views either way, so the velocity is the same;
// only its phase relative to the drawn frame differs by one, which is the same one-frame skew the
// rest of this single-threaded bring-up already has.
//
// WHICH BUFFER: GetWriteBuffer(), because that is the one Swap turns into the read buffer before
// Render runs. Writing the READ buffer instead would be written-then-immediately-swapped-away, and
// the symptom would be `[postfx-mb] update=1` with `wvpDelta` stuck at exactly 0 (the reader would
// see a record two frames stale, or an unstamped one).
// ==================================================================================================
void PCBringUpProduceParticleRenderData(BrnGame::DispatchThreadInputBuffer* lpBuffer,
                                        const BrnDirector::Camera::Camera*  lpCamera,
                                        f32                                 lfTimeStep,
                                        f32                                 lfTime,
                                        f32                                 lfTimeStepMultiplier)
{
    // [FLAG PC bring-up] the console can never be handed either as null (its caller asserts
    // `lpCamera != NULL`, EffectsModule.cpp:1035); on PC both are reachable before the director
    // has published anything.
    if (lpBuffer == 0 || lpCamera == 0)
        return;

    ParticleModuleUpdateBringUp(gRenderData, *lpCamera, lfTimeStep, lfTime, lfTimeStepMultiplier);

    // The bracket EffectsModule::GenerateDispatchLists @0x82296668 puts round the call
    // (`LockForWrite(a4) / GenerateRenderRequests(...) / UnlockForWrite(a4)`); the write-locked
    // GetParticleRenderData asserts it.
    lpBuffer->LockForWrite();
    ParticleModuleGenerateRenderRequestsBringUp(gRenderData, lpBuffer);
    lpBuffer->UnlockForWrite();
    RememberStampedBuffer(lpBuffer);

    // [FLAG PC bring-up diagnostic] one line, the first time a record is stamped. It is the proof
    // the conductor looks for that this seam ran at all, and it reports the three values the only
    // consumer actually reads. DELETE with the bring-up.
    if (!gbProduced)
    {
        gbProduced = true;

        const rw::math::vpu::Matrix44& lrView = gRenderData.mCgsCamera.mView;
        const bool lbViewFinite =
            std::isfinite(lrView.xAxis.x) && std::isfinite(lrView.yAxis.y)
            && std::isfinite(lrView.zAxis.z) && std::isfinite(lrView.wAxis.x)
            && std::isfinite(lrView.wAxis.y) && std::isfinite(lrView.wAxis.z);

        char lacMsg[224];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[postfx-mb] producer: first record frame=%u dt=%.5f accum=%.4f t=%.3f"
                      " flags=0x%02X view finite=%d viewW (%.2f, %.2f, %.2f)\n",
                      static_cast<unsigned>(gRenderData.muCurrentFrame),
                      static_cast<double>(lfTimeStep),
                      static_cast<double>(gRenderData.mfCurrentTimeStep),
                      static_cast<double>(gRenderData.mfCurrentTime),
                      static_cast<unsigned>(gRenderData.muFlags),
                      lbViewFinite ? 1 : 0,
                      static_cast<double>(lrView.wAxis.x),
                      static_cast<double>(lrView.wAxis.y),
                      static_cast<double>(lrView.wAxis.z));
        CgsDev::Log::WriteToLog(lacMsg);
    }
}

bool PCBringUpParticleRenderDataProducedFor(const BrnGame::DispatchThreadInputBuffer* lpBuffer)
{
    return lpBuffer != 0
        && (gapStampedBuffers[0] == lpBuffer || gapStampedBuffers[1] == lpBuffer);
}
}
