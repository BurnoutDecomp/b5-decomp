// =================================================================================================
// GameSource/Effects/Particles/ParticleModule_SparkEvents.cpp
//
// THE DISPATCH-THREAD END OF THE GRINDING-SPARK CHAIN.
//
//   BrnParticle::ParticleModule::ProcessEventQueue                    @0x8229C418  (113 instr)
//   BrnParticle::ParticleModule::HandleSpawnSparksAlongLineEvent      @0x8229A138  (329 instr)
//   BrnParticle::ParticleModule::HandleSpawnSparkShowerFromPointEvent @0x82299CC8  (284 instr)
//   ... and the two remaining sibling handlers, ANNOUNCED (see the bottom of this file).
//
// ⛔⛔ WHICH ROUTE THE CONSOLE ACTUALLY DRAWS (FX-CRASHVFX 2026-09-24). The along-line chain drawn
// below is the console's CODE but not its BEHAVIOUR: its only producer, EffectsModule::
// HandleSparkContacts, returns at its first instruction on the console (byte_82CDB40C, the
// function-local lbDisableThisEffect, is initialised TRUE and never written -- see the note there),
// so no type-3 record is ever posted. The sparks a player sees at a scrape or a crash are the
// SHOWERS: EffectsModule::ProcessRaceCarContacts -> DoSparkShower ->
// ParticleModule::SpawnSparkShowerFromPoint (type 2) -> HandleSpawnSparkShowerFromPointEvent (HERE)
// -> SparkArray::SpawnSpark. The PC build drew the along-line sparks from 6b2d999c until then.
//
// The chain, end to end, and why every link had to exist before one spark could fire from a real
// crash:
//
//   BrnPhysics::ContactSpy  --(RaceCar / PhysicalCarPart / HingedPart queues)-->
//   BrnEffects::EffectsModule::ProcessCarContactQueues @0x8229B7F8
//     -> ProcessRaceCarContacts / ProcessCarDetatchedPartContacts / ProcessHingedPartContacts
//     -> EffectsModule::HandleSparkContacts @0x822906A8
//        -> mParticleModule.mInterThreadEventQueue.AddEventSafe(&rec, 3, 80)
//   ParticleModule::PreRenderUpdate @0x82294760   -- appends that queue into the dispatch buffer
//   ParticleModule::DispatchThreadUpdate @0x8229C5F0
//     -> ProcessEventQueue (HERE)
//        -> HandleSpawnSparksAlongLineEvent (HERE)
//           -> SparkArray::SpawnSpark @0x822955E0
//
// Before this wave the queue in the middle was an asm-sized u8[] placeholder, so the two ends
// could not be joined even though both were partly written: the geometry half (RenderBank, the
// vertex builder) and the device half (SparkRenderer::Dispatch) had been landing since
// 2dac4368/5f23438f/dfb711d7, driven only by the BRN_SPARK_TEST instrument.
// =================================================================================================

#include "GameSource/Effects/Particles/ParticleModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"// StartMonitor / StopMonitor
#include "GameSource/Effects/Particles/ParticleCpuMonitors.h"            // gRaceCpuMonitors / gCrashCpuMonitors
#include "GameSource/Effects/BrnEffectsUtils.h"                          // Vector3Randomiser
#include "GameSource/Effects/BrnEffectsDebrisColourRandomiser.h"         // Utils::DebrisColourRandomiser (the burst's colours)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // the announcements

#include <cmath>    // sqrtf
#include <cstdio>   // snprintf (the announcements)
#include <cstdlib>  // getenv (the [debris-burst] witness)

namespace BrnParticle
{
    // ---------------------------------------------------------------------------------------------
    // The literals HandleSpawnSparksAlongLineEvent bakes, each read out of the ARTIST image with
    // tools/re/x360rd.py. Nothing here is tuned; the names describe the role the asm gives them.
    // ---------------------------------------------------------------------------------------------

    // flt_820138DC / flt_82005574 -- above this speed the spread is used at full width; below it,
    // it is scaled linearly down to nothing. 0.02 is exactly 1/50, so the pair is one ramp.
    static const f32 KF_SPARK_SPREAD_FULL_SPEED   = 50.0f;                  // flt_820138DC
    static const f32 KF_SPARK_SPREAD_SPEED_RECIP  = 0.019999999552965164f;  // flt_82005574

    // The spread bounds, per spark type. The ordinary (world/car grinding) set throws sparks up
    // and out; the body-part set is narrower laterally, much taller, and throws them BACKWARDS
    // (both z bounds negative).
    //   flt_820139E8 -3.0   flt_82001DA0 0.5    flt_8200DD24 3.0   flt_820139EC 13.0
    static const f32 KF_GRIND_SPREAD_MIN_X = -3.0f,  KF_GRIND_SPREAD_MIN_Y =  0.5f,  KF_GRIND_SPREAD_MIN_Z = -3.0f;
    static const f32 KF_GRIND_SPREAD_MAX_X =  3.0f,  KF_GRIND_SPREAD_MAX_Y = 13.0f,  KF_GRIND_SPREAD_MAX_Z =  3.0f;
    //   flt_8200D588 -1.7   flt_82013A88 5.9    flt_82013A84 -4.6
    //   flt_82013A80  1.8   flt_82004A20 10.0   flt_82013A7C -10.0
    static const f32 KF_BODYPART_SPREAD_MIN_X = -1.7000000476837158f,
                     KF_BODYPART_SPREAD_MIN_Y =  5.900000095367432f,
                     KF_BODYPART_SPREAD_MIN_Z = -4.599999904632568f;
    static const f32 KF_BODYPART_SPREAD_MAX_X =  1.7999999523162842f,
                     KF_BODYPART_SPREAD_MAX_Y = 10.0f,
                     KF_BODYPART_SPREAD_MAX_Z = -10.0f;

    // flt_82013A78 / flt_82013A74 -- every spark's size multiplier is drawn in [0.85, 1.0).
    static const f32 KF_SPARK_SIZE_MIN = 0.8500000238418579f;   // flt_82013A78
    static const f32 KF_SPARK_SIZE_MAX = 1.0f;                  // == KF_SPARK_SIZE_MIN + flt_82013A74 (0.15)

    // ⚠️ unk_82FAB950 -- the SPAWN-POSITION JITTER half-extent, used as Prepare(-K, +K) for every
    // spark type EXCEPT eSparkArray_BodyPart_Contact (which spawns exactly on the line).
    //     0x8229A32C  lis r9, unk_82FAB950@ha ; lvx128 v13, r0, r9
    //     vspltisw v0,-1 ; vslw v0,v0,v0 ; vxor v0, v13, v0     ; v0 = -K   (sign-bit flip)
    //     vsubfp   v0, v13, v0                                  ; K - (-K) = 2K == the range
    // IT READS ALL-ZERO, AND THAT IS THE HONEST VALUE HERE: the whole ARTIST export set contains
    // exactly ONE reference to that address -- the load above (grep over every per-function json
    // in .ida-exports/BURNOUT_X360_ARTIST.XEX) -- and findinit.py finds no store. Its .bss
    // neighbourhood (0x82FAB920..0x82FAB934) is zero too. So it is either a genuine zero or an
    // initialiser that lives in one of the export set's known holes; nothing is invented here, the
    // console's own word is used. If it ever turns out non-zero the ONLY visible change is that
    // non-body-part sparks gain a symmetric spawn-position jitter.
    static const Vector3 KV_SPARK_SPAWN_JITTER = { 0.0f, 0.0f, 0.0f, 0.0f };   // unk_82FAB950

    // ---------------------------------------------------------------------------------------------
    // HandleSpawnSparkShowerFromPointEvent's constants and VMX idioms (FX-CRASHVFX 2026-09-24).
    // ---------------------------------------------------------------------------------------------
    // flt_82001D9C -- the reflection amount is doubled before it scales the frame's x axis
    // (`fmuls f0, f13, f0` at 0x82299D7C): a spark heading INTO the surface loses 2 * amount of its
    // into-the-surface speed, i.e. amount 1.0 is a mirror bounce.
    static const f32 KF_SHOWER_REFLECTION_SCALE = 2.0f;                   // flt_82001D9C
    // flt_820139F8 -- one 60 Hz frame (0x3C888889): the burst's sparks are spread back in time over
    // it, each spawned (1/60) / count earlier than the one before (`fdivs f28, f0, f13`).
    static const f32 KF_SHOWER_SPAWN_WINDOW = 0.0166666675f;              // flt_820139F8
    // The CgsNumeric::TrigBaseFunctions5 sin/cos polynomial the shower evaluates its two angles
    // with -- every one a CRT-initialised .bss splat (thunk cited), the same seven the simple-particle
    // quad builder reads (ShadedRotatingRenderMethod.cpp):
    //   0x8307A680 1/2pi (0x82C6EB00)   0x8307A590 (-0.25, 0, -0.25, 0) (0x82C6EBC8)
    //   0x8307A3C0 1.0   (0x82C6EB78)   0x8307A560 -0.25 (0x82C6EBA0)
    //   0x8307A670 C5    (0x82C6F150)   0x8307A3B0 C3    (0x82C6F128)   0x8307A5F0 C1 (0x82C6F100)
    static const f32 KF_SHOWER_ONE_OVER_TWO_PI = 0.15915493667125702f;    // 0x3E22F983
    static const f32 KF_SHOWER_SIN_PHASE       = -0.25f;                  // unk_8307A590 lanes 0 / 2
    static const f32 KF_SHOWER_COS_PHASE       = 0.0f;                    // unk_8307A590 lanes 1 / 3
    static const f32 KF_SHOWER_FOLD_PERIOD     = 1.0f;                    // unk_8307A3C0
    static const f32 KF_SHOWER_FOLD_BIAS       = -0.25f;                  // unk_8307A560
    static const f32 KF_SHOWER_POLY_C5         = -71.17897033691406f;     // unk_8307A670 (0xC28E5BA2)
    static const f32 KF_SHOWER_POLY_C3         = 40.897369384765625f;     // unk_8307A3B0 (0x422396E8)
    static const f32 KF_SHOWER_POLY_C1         = -6.278042793273926f;     // unk_8307A5F0 (0xC0C8E5BA)

    namespace
    {
        // vnmsubfp: -(a*c - b), rounded ONCE, then NEGATED -- zeros too (a QNaN keeps its sign).
        inline f32 ShowerVnmsub(f32 lfA, f32 lfC, f32 lfB)
        {
            const f32 lfDifference = std::fma(lfA, lfC, -lfB);
            return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
        }

        // vminfp: a NaN operand propagates (AltiVec: "the result is a QNaN"), the first one first.
        inline f32 ShowerVMin(f32 lfA, f32 lfB)
        {
            if (lfA != lfA) return lfA;
            if (lfB != lfB) return lfB;
            return (lfA < lfB) ? lfA : lfB;
        }

        // vrefp + two Newton-Raphson steps (vnmsubfp 1 - est*x, vmaddfp est + est*r). FLAG (model):
        // the hardware estimate is taken as its correctly rounded value; the refinements pin it.
        inline f32 ShowerRefinedRecip(f32 lfX)
        {
            f32 lfEstimate = static_cast<f32>(1.0 / static_cast<f64>(lfX));
            for (u32 luStep = 0; luStep < 2u; ++luStep)
                lfEstimate = std::fma(lfEstimate, ShowerVnmsub(lfEstimate, lfX, 1.0f), lfEstimate);
            return lfEstimate;
        }

        // vmsum3fp128 (one rounding of the exact dot -- FLAG (model), as EffectsModule's Dot3),
        // then vrsqrtefp + two steps, and the vcmpeqfp / vsel that maps a zero vector to 0.
        inline f32 ShowerGuardedLength3(const Vector3& lrv)
        {
            const f32 lfSquared = static_cast<f32>(static_cast<f64>(lrv.x) * lrv.x + static_cast<f64>(lrv.y) * lrv.y
                                                 + static_cast<f64>(lrv.z) * lrv.z);
            if (lfSquared == 0.0f)
                return 0.0f;
            f32 lfEstimate = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfSquared)));
            for (u32 luStep = 0; luStep < 2u; ++luStep)
            {
                const f32 lfEstimateSquared = lfEstimate * lfEstimate;
                const f32 lfHalf            = lfEstimate * 0.5f;
                lfEstimate = std::fma(lfHalf, ShowerVnmsub(lfSquared, lfEstimateSquared, 1.0f), lfEstimate);
            }
            return lfSquared * lfEstimate;
        }

        inline f32 ShowerDot3(const Vector3& lrA, const Vector3& lrB)
        {
            return static_cast<f32>(static_cast<f64>(lrA.x) * lrB.x + static_cast<f64>(lrA.y) * lrB.y
                                  + static_cast<f64>(lrA.z) * lrB.z);
        }

        // One lane of the sin/cos polynomial, step for step and FUSED where the console fuses:
        //   x = angle * 1/2pi + phase        vmaddfp
        //   t = min(x - floor(x), 1 - (x - floor(x))) - 0.25     vrfim / vsubfp / vsubfp / vminfp / vaddfp
        //   p = (t^2 * (t^2 * C5 + C3) + C1) * t                 vmulfp128 / vmaddfp / vmaddfp / vmulfp128
        // Phase -0.25 gives sin(angle), phase 0 gives cos(angle).
        inline f32 ShowerSinCosLane(f32 lfAngle, f32 lfPhase)
        {
            const f32 lfX        = std::fma(lfAngle, KF_SHOWER_ONE_OVER_TWO_PI, lfPhase);
            const f32 lfFraction = lfX - std::floor(lfX);
            const f32 lfT        = ShowerVMin(lfFraction, KF_SHOWER_FOLD_PERIOD - lfFraction) + KF_SHOWER_FOLD_BIAS;
            const f32 lfT2       = lfT * lfT;
            const f32 lfInner    = std::fma(lfT2, KF_SHOWER_POLY_C5, KF_SHOWER_POLY_C3);
            const f32 lfPoly     = std::fma(lfT2, lfInner, KF_SHOWER_POLY_C1);
            return lfPoly * lfT;
        }

        // vmaddfp across four lanes: a * splat(s) + b.
        inline Vector3 ShowerMulAdd(const Vector3& lrA, f32 lfScale, const Vector3& lrB)
        {
            Vector3 lv;
            lv.x = std::fma(lrA.x, lfScale, lrB.x);
            lv.y = std::fma(lrA.y, lfScale, lrB.y);
            lv.z = std::fma(lrA.z, lfScale, lrB.z);
            lv.w = std::fma(lrA.w, lfScale, lrB.w);
            return lv;
        }

        // vmulfp across four lanes by a splat.
        inline Vector3 ShowerScale(const Vector3& lrA, f32 lfScale)
        {
            Vector3 lv;
            lv.x = lrA.x * lfScale;
            lv.y = lrA.y * lfScale;
            lv.z = lrA.z * lfScale;
            lv.w = lrA.w * lfScale;
            return lv;
        }
    }

    // The once-only announcement helper every not-reconstructed arm in this file shares.
    namespace
    {
        void LogSparkEventNotReconstructed(bool& lrbLogged, const char* lpcWhat)
        {
            if (lrbLogged)
                return;
            lrbLogged = true;
            char lacMsg[320];
            std::snprintf(lacMsg, sizeof(lacMsg), "[particle] NOT RECONSTRUCTED: %s\n", lpcWhat);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // ProcessEventQueue @0x8229C418 (DWARF ParticleModule.h:576).
    //
    // Walk the dispatch buffer's copy of the inter-thread queue and dispatch each record on its
    // type id. The switch is a real 6-entry jump table (jpt_8229C494) with an "Unhandled Event"
    // assert default at ParticleModule.cpp:938.
    // ---------------------------------------------------------------------------------------------
    void ParticleModule::ProcessEventQueue(
             const CgsModule::VariableEventQueue<
                       KI_PARTICLE_MODULE_INTERTHREAD_COMMAND_QUEUE_MEMSIZE, 16>* lpQueue,
             const ParticleRenderData& lrRenderData)
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        s32 liType = lpQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent != 0)
        {
            ++gauSparkEventsDrained;   // [DIAG] DELETE-WHEN-STABLE

            switch (liType)
            {
            case eParticleEvent_ClearAllDebrisBuckets:
            {
                // `li r31, 5 ; add r30, r27, 0x22818 ; ClearAllBuckets(r30) ; r30 += 0x20` x5 --
                // i.e. maDebris[0..4].ClearAllBuckets().
                // Reached once per crash: EffectsModule::HandlePlayerTriangleCache posts exactly
                // one type-0 record on the frame a crash ENDS.
                for (u32 luArray = 0; luArray < KU_NUM_DEBRIS_ARRAYS; ++luArray)
                {
                    maDebris[luArray].ClearAllBuckets();
                }
                break;
            }

            case eParticleEvent_SpawnSparksFromPoint:
                HandleSpawnSparksFromPointEvent(
                    static_cast<const SpawnSparksFromPointEvent*>(lpEvent), lrRenderData);
                break;

            case eParticleEvent_SpawnSparkShowerFromPoint:
                HandleSpawnSparkShowerFromPointEvent(
                    static_cast<const SpawnSparkShowerFromPointEvent*>(lpEvent), lrRenderData);
                break;

            case eParticleEvent_SpawnSparksAlongLine:
                HandleSpawnSparksAlongLineEvent(
                    static_cast<const SpawnSparksAlongLineEvent*>(lpEvent), lrRenderData);
                break;

            case eParticleEvent_DebrisBatchSpawn:
            {
                // The batch is `u16 count` in the record's first 16 bytes, then count 80-byte
                // DebrisSpawnData records. Per record the console asserts
                // "lDebrisData.meType < BrnParticle::Native::eDebrisArray_Max" (ParticleModule.cpp
                // :917) and calls
                //     maDebris[meType].SpawnDebris(mvPositionPlusSize, mvVelocityPlusSpawnTime,
                //                                  mvRotationAxisPlusRotationAmount, mvColour,
                //                                  record+0x2C, record+0x0C, record+0x1C)
                // -- the four vectors loaded whole, the three scalars being the w lanes of the
                // first three, in the register order f1/f2/f3.
                // The count is re-read every iteration, so the walk is written against the
                // event's own header word rather than a cached copy.
                {
                    const DebrisBatchSpawnEvent* const lpBatch =
                        static_cast<const DebrisBatchSpawnEvent*>(lpEvent);
                    for (u32 luRecord = 0; luRecord < lpBatch->mu16DebrisCount; ++luRecord)
                    {
                        const DebrisBatchSpawnEvent::DebrisSpawnData& lrData =
                            lpBatch->maDebris[luRecord];

                        CGS_ASSERT(lrData.meType < Native::eDebrisArray_Max,
                                   "lDebrisData.meType < BrnParticle::Native::eDebrisArray_Max");

                        maDebris[lrData.meType].SpawnDebris(
                            lrData.mvPositionPlusSize.GetVector3(),
                            lrData.mvVelocityPlusSpawnTime.GetVector3(),
                            lrData.mvRotationAxisPlusRotationAmount.GetVector3(),
                            lrData.mvColour,
                            lrData.mvRotationAxisPlusRotationAmount.w,
                            lrData.mvPositionPlusSize.w,
                            lrData.mvVelocityPlusSpawnTime.w);
                    }
                }
                break;
            }

            case eParticleEvent_FireDebrisBurst:
                HandleFireDebrisBurstEvent(static_cast<const FireDebrisBurstEvent*>(lpEvent));
                break;

            default:
                CGS_ASSERT(false, "Unhandled Event");
                break;
            }

            liType = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // HandleSpawnSparksAlongLineEvent @0x8229A138 (DWARF ParticleModule.h:591).
    //
    // ⭐ THIS IS WHAT A GRINDING CONTACT LOOKS LIKE. EffectsModule::HandleSparkContacts turned one
    // resolved contact into a SEGMENT -- from the contact point, along the normalised friction
    // stress, for a length drawn out of the sparkeffect vault -- plus the number of sparks to put
    // on it. This walks that segment and launches them.
    //
    // SpawnSparksAlongLine (DWARF ParticleModule.h:566) is INLINED here: the console emits no
    // separate symbol for it, and the whole body below is what that call expanded to.
    //
    // THE FOUR THINGS WORTH KNOWING, all of them the console's own arithmetic:
    //
    //  1. THE SPREAD IS SCALED BY THE CONTACT'S SPEED, not by anything the caller passes. The
    //     event's mvVelocity length is ramped 0 -> 1 over 0..50 m/s and every spread bound is
    //     multiplied by it, so a slow scrape throws sparks nearly straight along the segment and a
    //     fast one throws them wide. (0x8229A1AC..0x8229A23C.)
    //
    //  2. IT SPAWNS mfNumSparks + 1 SPARKS, NOT mfNumSparks. The loop is a do/while on a POST-
    //     decrement (`cmpwi r27, 0 ; addi r27, r27, -1 ; bne`), entered only when the count is
    //     positive -- so a count of N runs N+1 times. That is not an off-by-one to correct: the
    //     step is (end - start)/N, so N+1 samples put one spark on the start point and one exactly
    //     on the end point. Reproduced as the console has it.
    //
    //  3. A CRASHING CONTACT SPAWNS THE WHOLE LINE TWICE -- once into the regular bank and once
    //     into the crash bank (`lbz r11, 0x48(r29) ; cntlzw ; ... ; addi r11, r11, 1` gives 2 when
    //     mbIsCrashing is set, 1 otherwise, and r24 -- SpawnSpark's lbIsCrashSpark -- is 0 on the
    //     first pass and forced to 1 at the bottom of every pass). The crash bank is the one
    //     SparkVertexBufferBuilder::BuildDispatchData renders into the same batch on request.
    //
    //  4. THE SPARK'S BIRTH TIME IS EXPRESSED IN THE MOTION-BLUR RING'S CLOCK. SpawnSpark computes
    //     `(lfCurrentTime - lfTimeSinceEvent) + lfBirthTimeOffset`, and this call site passes the
    //     EVENT's time as lfCurrentTime, the render data's time as lfTimeSinceEvent and the ring's
    //     newest timestamp (+0x250B0 == mSparkFrameDataSetUpdate.maFrames[0].mfTimeStamp) as the
    //     offset -- i.e. "how long ago the contact happened, measured back from the ring's now".
    //     Getting that wrong is invisible in a counter and fatal on screen: the ribbon would be
    //     evaluated at an age outside the blur window and cull.
    // ---------------------------------------------------------------------------------------------
    void ParticleModule::HandleSpawnSparksAlongLineEvent(const SpawnSparksAlongLineEvent* lpEvent,
                                                         const ParticleRenderData& lrRenderData)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent != NULL");                  // ParticleModule.cpp:1288

        // f27 / f26, hoisted out of both loops by the console exactly as they are here.
        const f32 lfRenderTime = lrRenderData.mfCurrentTime;                             // r18 + 8
        const f32 lfRingTime   = mSparkFrameDataSetUpdate.GetFrame(0).mfTimeStamp;       // +0x250B0

        // (1) the speed ramp.
        const Vector3& lrEventVelocity = lpEvent->mvVelocity;
        const f32 lfSpeed = sqrtf(lrEventVelocity.x * lrEventVelocity.x
                                + lrEventVelocity.y * lrEventVelocity.y
                                + lrEventVelocity.z * lrEventVelocity.z);
        const f32 lfSpreadScale = (lfSpeed > KF_SPARK_SPREAD_FULL_SPEED)
                                ? 1.0f
                                : lfSpeed * KF_SPARK_SPREAD_SPEED_RECIP;

        const bool lbIsBodyPartContact =
            (lpEvent->meSparkType == Native::eSparkArray_BodyPart_Contact);

        Vector3 lvSpreadMin;
        Vector3 lvSpreadMax;
        if (lbIsBodyPartContact)
        {
            lvSpreadMin.x = KF_BODYPART_SPREAD_MIN_X; lvSpreadMin.y = KF_BODYPART_SPREAD_MIN_Y;
            lvSpreadMin.z = KF_BODYPART_SPREAD_MIN_Z; lvSpreadMin.w = 0.0f;
            lvSpreadMax.x = KF_BODYPART_SPREAD_MAX_X; lvSpreadMax.y = KF_BODYPART_SPREAD_MAX_Y;
            lvSpreadMax.z = KF_BODYPART_SPREAD_MAX_Z; lvSpreadMax.w = 0.0f;
        }
        else
        {
            lvSpreadMin.x = KF_GRIND_SPREAD_MIN_X;    lvSpreadMin.y = KF_GRIND_SPREAD_MIN_Y;
            lvSpreadMin.z = KF_GRIND_SPREAD_MIN_Z;    lvSpreadMin.w = 0.0f;
            lvSpreadMax.x = KF_GRIND_SPREAD_MAX_X;    lvSpreadMax.y = KF_GRIND_SPREAD_MAX_Y;
            lvSpreadMax.z = KF_GRIND_SPREAD_MAX_Z;    lvSpreadMax.w = 0.0f;
        }

        Vector3 lvScaledMin, lvScaledMax;
        lvScaledMin.x = lvSpreadMin.x * lfSpreadScale; lvScaledMin.y = lvSpreadMin.y * lfSpreadScale;
        lvScaledMin.z = lvSpreadMin.z * lfSpreadScale; lvScaledMin.w = lvSpreadMin.w * lfSpreadScale;
        lvScaledMax.x = lvSpreadMax.x * lfSpreadScale; lvScaledMax.y = lvSpreadMax.y * lfSpreadScale;
        lvScaledMax.z = lvSpreadMax.z * lfSpreadScale; lvScaledMax.w = lvSpreadMax.w * lfSpreadScale;

        BrnEffects::Utils::Vector3Randomiser lVelocitySpread;      // var_150 / var_140
        lVelocitySpread.Prepare(lvScaledMin, lvScaledMax);

        Vector3 lvJitterMin, lvJitterMax;
        lvJitterMin.x = -KV_SPARK_SPAWN_JITTER.x; lvJitterMin.y = -KV_SPARK_SPAWN_JITTER.y;
        lvJitterMin.z = -KV_SPARK_SPAWN_JITTER.z; lvJitterMin.w = -KV_SPARK_SPAWN_JITTER.w;
        lvJitterMax = KV_SPARK_SPAWN_JITTER;

        BrnEffects::Utils::Vector3Randomiser lPositionJitter;      // var_170 / var_160
        lPositionJitter.Prepare(lvJitterMin, lvJitterMax);

        // The velocity the spark inherits from the contact, drawn once per spark between
        // mfVelocityInheritanceMin * v and mfVelocityInheritanceMax * v (v123 / v122 -- the
        // console keeps this pair in registers instead of on the stack, which is why its Prepare
        // shows up as two vmulfp128s and a vsubfp rather than two stvx128s).
        Vector3 lvInheritMin, lvInheritMax;
        lvInheritMin.x = lrEventVelocity.x * lpEvent->mfVelocityInheritanceMin;
        lvInheritMin.y = lrEventVelocity.y * lpEvent->mfVelocityInheritanceMin;
        lvInheritMin.z = lrEventVelocity.z * lpEvent->mfVelocityInheritanceMin;
        lvInheritMin.w = lrEventVelocity.w * lpEvent->mfVelocityInheritanceMin;
        lvInheritMax.x = lrEventVelocity.x * lpEvent->mfVelocityInheritanceMax;
        lvInheritMax.y = lrEventVelocity.y * lpEvent->mfVelocityInheritanceMax;
        lvInheritMax.z = lrEventVelocity.z * lpEvent->mfVelocityInheritanceMax;
        lvInheritMax.w = lrEventVelocity.w * lpEvent->mfVelocityInheritanceMax;

        BrnEffects::Utils::Vector3Randomiser lVelocityInheritance;
        lVelocityInheritance.Prepare(lvInheritMin, lvInheritMax);

        Native::SparkArray& lrArray = maSparks[lpEvent->meSparkType];   // this + 0x94D0 + type*0x90

        // (3) the regular pass, then the crash-bank pass when the contact is a crash.
        bool      lbUseCrashBank = false;
        const s32 liNumPasses    = lpEvent->mbIsCrashing ? 2 : 1;

        for (s32 liPass = 0; liPass < liNumPasses; ++liPass)
        {
            // Re-read per pass, exactly as the console does at loc_8229A40C.
            s32 liRemaining = static_cast<s32>(lpEvent->mfNumSparks);   // fctiwz: round toward zero

            if (liRemaining > 0)
            {
                Vector3 lvPoint         = lpEvent->mvStartPos;                       // v125
                f32     lfHeightAbove   = lpEvent->mfHeightAboveGroundOfStartPos;    // f31

                const f32 lfOneOverCount = 1.0f / static_cast<f32>(liRemaining);
                Vector3 lvStep;
                lvStep.x = (lpEvent->mvEndPos.x - lvPoint.x) * lfOneOverCount;
                lvStep.y = (lpEvent->mvEndPos.y - lvPoint.y) * lfOneOverCount;
                lvStep.z = (lpEvent->mvEndPos.z - lvPoint.z) * lfOneOverCount;
                lvStep.w = (lpEvent->mvEndPos.w - lvPoint.w) * lfOneOverCount;

                do
                {
                    // ⚠ DRAW ORDER IS PART OF THE BEHAVIOUR -- one shared LCG feeds all of these,
                    // so re-ordering them changes every spark. The console draws the inheritance
                    // first (inline, before the RandomiseXYZ call it then makes), then the spread,
                    // then -- only for non-body-part types -- the position jitter, then the size.
                    const Vector3 lvInherited = lVelocityInheritance.RandomInterpolate(mRandom);
                    const Vector3 lvSpread    = lVelocitySpread.RandomiseXYZ(mRandom);

                    Vector3 lvVelocity;
                    lvVelocity.x = lvSpread.x + lvInherited.x;
                    lvVelocity.y = lvSpread.y + lvInherited.y;
                    lvVelocity.z = lvSpread.z + lvInherited.z;
                    lvVelocity.w = lvSpread.w + lvInherited.w;

                    Vector3 lvSpawnPosition = lvPoint;
                    if (!lbIsBodyPartContact)
                    {
                        const Vector3 lvJitter = lPositionJitter.RandomiseXYZ(mRandom);
                        lvSpawnPosition.x += lvJitter.x;
                        lvSpawnPosition.y += lvJitter.y;
                        lvSpawnPosition.z += lvJitter.z;
                        lvSpawnPosition.w += lvJitter.w;
                    }

                    // The monitor set is re-selected per spark (the console reloads muFlags each
                    // iteration); +8 in ParticleCpuMonitors is miSpawnSpark.
                    const ParticleCpuMonitors& lrMonitors =
                        ((lrRenderData.muFlags & ParticleRenderData::eRenderDataFlagReducedFrameRate) != 0)
                            ? gCrashCpuMonitors : gRaceCpuMonitors;
                    const s32 liMonitor = lrMonitors.miSpawnSpark;

                    CgsDev::PerfMonCpu::StartMonitor(liMonitor);

                    const f32 lfSize = mRandom.RandomFloat(KF_SPARK_SIZE_MIN, KF_SPARK_SIZE_MAX);

                    lrArray.SpawnSpark(lvSpawnPosition,
                                       lvVelocity,
                                       lfSize,
                                       lpEvent->mfCurrentTime,   // f2
                                       lfRenderTime,             // f3
                                       lfRingTime,               // f4
                                       lfHeightAbove,            // f5
                                       lbUseCrashBank);          // r9

                    CgsDev::PerfMonCpu::StopMonitor(liMonitor);
                    ++gauSparkLineSpawned;   // [DIAG] DELETE-WHEN-STABLE

                    lfHeightAbove += lvStep.y;
                    lvPoint.x += lvStep.x;
                    lvPoint.y += lvStep.y;
                    lvPoint.z += lvStep.z;
                    lvPoint.w += lvStep.w;
                }
                while (liRemaining-- != 0);   // (2) N + 1 samples -- see the banner
            }

            lbUseCrashBank = true;            // `li r24, 1` at the bottom of every pass
        }
    }

    // ---------------------------------------------------------------------------------------------
    // The two remaining sibling handlers. NOT RECONSTRUCTED; each says so in the log the first time
    // a record of its type reaches it, so a dropped record is visible rather than invisible:
    //   * type 1 comes only from EffectsModule::HandleRaceCarRaceCarSparks @0x82290A48, which is
    //     CONSOLE-DEAD (byte_82CDB40D, its lbDisableThisEffect, is initialised TRUE): no type-1
    //     record is ever posted on the console or here, so this handler can never be reached;
    //   * type 5 comes from ParticleModule::FireDebrisBurst (the crashing / takedown debris bursts
    //     ProcessRaceCarContacts posts) -- the next piece of this file.
    // ---------------------------------------------------------------------------------------------
    void ParticleModule::HandleSpawnSparksFromPointEvent(const SpawnSparksFromPointEvent* /*lpEvent*/,
                                                         const ParticleRenderData& /*lrRenderData*/)
    {
        static bool sbLogged = false;
        LogSparkEventNotReconstructed(sbLogged,
            "ParticleModule::HandleSpawnSparksFromPointEvent @0x82299840 (290 instr) -- a type-1 "
            "record reached ProcessEventQueue and was dropped");
    }

    // ---------------------------------------------------------------------------------------------
    // HandleSpawnSparkShowerFromPointEvent @0x82299CC8 (284 instr, DWARF ParticleModule.h:594) --
    // FX-CRASHVFX 2026-09-24. ⭐ THE SPARKS A PLAYER SEES AT A SCRAPE OR A CRASH ON THE CONSOLE: the
    // world-grinding, vehicle-grinding and crash showers EffectsModule::DoSparkShower posts.
    //
    // The record carries a FRAME (mTransform: x = the contact normal, y / z across and along it,
    // w = the contact point) and the shower controller's arguments already lerped by the burst size.
    // Per record, once:
    //   * speed scale = min(1, |mVelocityToInherit| / mfVelocityScaleSpeedThreshold) -- a slow car
    //     throws slow sparks (the spawn-speed bounds are multiplied by it);
    //   * direction randomiser (lateral angle, forward angle, spawn speed, inherited share) between
    //     (latMin, fwdMin, velMin*s, inhMin) and (latMax, fwdMax, velMax*s, inhMax), and position
    //     randomiser (x, y, z offset, size) between (-rX, -rYZ, -rYZ, sizeMin) and (rX, rYZ, rYZ,
    //     sizeMax) -- both assembled with vperm / vsldoi from the record (masks 0x82CDA3C0 /
    //     0x82CDA400 / 0x82CDB420 / 0x82CDB440) and stored as base + range, exactly Prepare's form.
    // Per spark (luNumToSpawn of them, asserted > 0):
    //   * two RandomiseXYZW draws, direction first;
    //   * sin / cos of both angles (the TrigBaseFunctions5 polynomial, lanes (sinL, cosL, sinF, cosF));
    //   * direction = (x * sinL + y * cosL) * cosF + z * sinF; velocity = direction * speed +
    //     inherit * share, then the part heading INTO the surface is reflected:
    //     v -= x * (2 * reflection) * min(dot(v, x), 0);
    //   * position = w + x * ox + y * oy + z * oz; height = position.y - ground;
    //   * SparkArray::SpawnSpark into maSparks[type], the regular bank, stamped with the record's time,
    //     which then steps BACK by (1/60) / count per spark.
    // ---------------------------------------------------------------------------------------------
    void ParticleModule::HandleSpawnSparkShowerFromPointEvent(const SpawnSparkShowerFromPointEvent* lpEvent,
                                                              const ParticleRenderData& lrRenderData)
    {
        CGS_ASSERT(lpEvent->meSparkType >= 0 && lpEvent->meSparkType < Native::eSparkArray_Max,
                   "( lpEvent->meSparkType >= 0 ) && ( lpEvent->meSparkType < BrnParticle::Native::eSparkArray_Max )");

        const Matrix44Affine& lrFrame = lpEvent->mTransform;
        const f32 lfRenderTime = lrRenderData.mfCurrentTime;                            // f27 (r5 + 8)
        const f32 lfRingTime   = mSparkFrameDataSetUpdate.GetFrame(0).mfTimeStamp;      // f26 (+0x250B0)
        const f32 lfGroundY    = lpEvent->mfGroundPositionY;                           // f25

        // v121 -- the reflection axis: x * (2 * amount).
        const Vector3 lvReflectionAxis =
            ShowerScale(lrFrame.xAxis, lpEvent->mfReflectionAmount * KF_SHOWER_REFLECTION_SCALE);

        // The speed scale.
        const f32 lfSpeed      = ShowerGuardedLength3(lpEvent->mVelocityToInherit);
        const f32 lfSpeedScale =
            ShowerVMin(1.0f, ShowerRefinedRecip(lpEvent->mfVelocityScaleSpeedThreshold) * lfSpeed);

        const Vector4& lrLateral = lpEvent->mLateralAngleMinMaxForwardAngleMinMax;
        const Vector4& lrSpeeds  = lpEvent->mVelocityMinMaxInheritanceMinMax;
        const Vector4& lrSizes   = lpEvent->mSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ;

        // The direction randomiser (var_B0 / var_C0).
        Vector4 lvDirectionMin;
        lvDirectionMin.x = lrLateral.x;
        lvDirectionMin.y = lrLateral.z;
        lvDirectionMin.z = lrSpeeds.x * lfSpeedScale;
        lvDirectionMin.w = lrSpeeds.z;
        Vector4 lvDirectionMax;
        lvDirectionMax.x = lrLateral.y;
        lvDirectionMax.y = lrLateral.w;
        lvDirectionMax.z = lrSpeeds.y * lfSpeedScale;
        lvDirectionMax.w = lrSpeeds.w;
        BrnEffects::Utils::Vector4Randomiser lDirectionRandomiser;
        lDirectionRandomiser.Prepare(lvDirectionMin, lvDirectionMax);

        // The position / size randomiser (var_D0 / var_E0): the radii's negations are sign-bit
        // flips (`vxor` with splat(0x80000000)).
        Vector4 lvOffsetMin;
        lvOffsetMin.x = -lrSizes.z;
        lvOffsetMin.y = -lrSizes.w;
        lvOffsetMin.z = -lrSizes.w;
        lvOffsetMin.w = lrSizes.x;
        Vector4 lvOffsetMax;
        lvOffsetMax.x = lrSizes.z;
        lvOffsetMax.y = lrSizes.w;
        lvOffsetMax.z = lrSizes.w;
        lvOffsetMax.w = lrSizes.y;
        BrnEffects::Utils::Vector4Randomiser lOffsetRandomiser;
        lOffsetRandomiser.Prepare(lvOffsetMin, lvOffsetMax);

        const u32 luNumToSpawn = lpEvent->muNumToSpawn;
        CGS_ASSERT(luNumToSpawn > 0, "luNumToSpawn > 0");
        // fcfid of the zero-extended count, frsp, then (1/60) / count.
        const f32 lfTimeStep = KF_SHOWER_SPAWN_WINDOW / static_cast<f32>(static_cast<f64>(luNumToSpawn));
        f32 lfSpawnTime = lpEvent->mfCurrentTime;                                      // f31

        Native::SparkArray& lrArray = maSparks[lpEvent->meSparkType];                // this + 0x94D0 + type*0x90

        for (u32 luRemaining = luNumToSpawn; luRemaining != 0u; --luRemaining)
        {
            const Vector4 lvDirectionDraw = lDirectionRandomiser.RandomiseXYZW(mRandom);   // var_80
            const Vector4 lvOffsetDraw    = lOffsetRandomiser.RandomiseXYZW(mRandom);      // var_70

            // (sinL, cosL, sinF, cosF): vmrghw (lat, lat, fwd, fwd) against the phase splat.
            const f32 lfSinLateral = ShowerSinCosLane(lvDirectionDraw.x, KF_SHOWER_SIN_PHASE);
            const f32 lfCosLateral = ShowerSinCosLane(lvDirectionDraw.x, KF_SHOWER_COS_PHASE);
            const f32 lfSinForward = ShowerSinCosLane(lvDirectionDraw.y, KF_SHOWER_SIN_PHASE);
            const f32 lfCosForward = ShowerSinCosLane(lvDirectionDraw.y, KF_SHOWER_COS_PHASE);

            // direction = (x * sinL + y * cosL) * cosF + z * sinF (0x8229A078..0x8229A08C).
            const Vector3 lvAcross    = ShowerMulAdd(lrFrame.xAxis, lfSinLateral, ShowerScale(lrFrame.yAxis, lfCosLateral));
            const Vector3 lvDirection = ShowerMulAdd(lvAcross, lfCosForward, ShowerScale(lrFrame.zAxis, lfSinForward));

            // velocity = direction * speed + inherit * share, then the reflection (0x8229A034,
            // 0x8229A090..0x8229A0A0).
            const Vector3 lvInherited = ShowerScale(lpEvent->mVelocityToInherit, lvDirectionDraw.w);
            const Vector3 lvLaunch    = ShowerMulAdd(lvDirection, lvDirectionDraw.z, lvInherited);
            const f32 lfInto          = ShowerVMin(ShowerDot3(lvLaunch, lrFrame.xAxis), 0.0f);
            const Vector3 lvBounce    = ShowerScale(lvReflectionAxis, lfInto);
            Vector3 lvVelocity;
            lvVelocity.x = lvLaunch.x - lvBounce.x;
            lvVelocity.y = lvLaunch.y - lvBounce.y;
            lvVelocity.z = lvLaunch.z - lvBounce.z;
            lvVelocity.w = lvLaunch.w - lvBounce.w;

            // position = w + x * ox + y * oy + z * oz (three fused steps, 0x8229A0A8..0x8229A0B8).
            Vector3 lvPosition = ShowerMulAdd(lrFrame.xAxis, lvOffsetDraw.x, lrFrame.wAxis);
            lvPosition = ShowerMulAdd(lrFrame.yAxis, lvOffsetDraw.y, lvPosition);
            lvPosition = ShowerMulAdd(lrFrame.zAxis, lvOffsetDraw.z, lvPosition);
            const f32 lfHeightAbove = lvPosition.y - lfGroundY;                         // f29

            const ParticleCpuMonitors& lrMonitors =
                ((lrRenderData.muFlags & ParticleRenderData::eRenderDataFlagReducedFrameRate) != 0)
                    ? gCrashCpuMonitors : gRaceCpuMonitors;
            const s32 liMonitor = lrMonitors.miSpawnSpark;
            CgsDev::PerfMonCpu::StartMonitor(liMonitor);

            lrArray.SpawnSpark(lvPosition,
                               lvVelocity,
                               lvOffsetDraw.w,      // f1 -- the drawn size
                               lfSpawnTime,         // f2
                               lfRenderTime,        // f3
                               lfRingTime,          // f4
                               lfHeightAbove,       // f5
                               false);              // r9 -- the regular bank

            CgsDev::PerfMonCpu::StopMonitor(liMonitor);
            ++gauSparkShowerSpawned;   // [DIAG] DELETE-WHEN-STABLE
            lfSpawnTime -= lfTimeStep;
        }
    }

    // =============================================================================================
    // ⭐⭐⭐ THE CRASH DEBRIS BURST -- ParticleModule::HandleFireDebrisBurstEvent @0x8229A660 (DWARF
    // ParticleModule.cpp:1477), FX-CRASHVFX 2026-09-25. It consumes the type-5 record
    // ParticleModule::FireDebrisBurst posts for a crashing / taken-down car (EffectsModule::HandleBurstDebris)
    // and throws the car's debris. For each of the four burst arrays -- Coloured (the car's paint), Shiny,
    // Dark, HighDetail (each array's own preset colour, BrnDebrisArrayParams::mColour) -- the debrisparams'
    // particle count times the record's scale factor, truncated, pieces, each
    //   * placed at a random point of the emitter box (RandomFloat(-h, h) per axis, z drawn first);
    //   * thrown either AT THE CAMERA (when a draw falls below the debrisparams' camera-directed
    //     probability): towards 0.5 above the camera at 7 m/s, lifted by half a gravity drop over the
    //     flight time (the lift is a splat, so the console adds it to x, z and w too); or into a CONE: a
    //     random azimuth and a random angle within the emission-angle variance, at a random speed between the
    //     burst velocity bounds, plus 5.1..20% of the car's velocity;
    //   * a random size between the type's bounds, a random spin axis (RandomUnitVector, inlined), a spin
    //     rate in [0.5, 4.0), and a colour 25%..75% of the way from grey to the base colour, alpha 0.5..0.9
    //     (DebrisColourRandomiser);
    // and spawned into the array directly (BrnDebrisArray::SpawnDebris -- this runs on the dispatch thread).
    // Every fused op is fused; the draws are the console's, in its order; every compare keeps the PPC
    // polarity for a NaN.
    // =============================================================================================
    namespace
    {
        const f32 KF_DEBRIS_BURST_DEG_TO_RAD    = 0.0174532924f;   // flt_8200D964 (0x3C8EFA35)
        const f32 KF_DEBRIS_BURST_TWO_PI        = 6.28318548f;     // flt_8200D970 (0x40C90FDB, `lfs f0, 0xC(r16)`)
        const f32 KF_DEBRIS_BURST_CAMERA_SPEED  = 7.0f;            // flt_820054D0
        const f32 KF_DEBRIS_BURST_SECONDS_PER_M = 0.142857149f;    // flt_82013AB4 (0x3E124925, 1/7)
        const f32 KF_DEBRIS_BURST_HALF          = 0.5f;            // flt_82001DA0
        const f32 KF_DEBRIS_BURST_GRAVITY       = -9.81000042f;    // flt_82013AB8 (0xC11CF5C3)
        const f32 KF_DEBRIS_BURST_INHERIT_MIN   = 0.0509999990f;   // flt_82013AC0 (0x3D50E560)
        const f32 KF_DEBRIS_BURST_INHERIT_RANGE = 0.149000004f;    // flt_82013ABC (0x3E189375)
        const f32 KF_DEBRIS_BURST_SPIN_RANGE    = 3.5f;            // flt_82009BA0
        const f32 KF_DEBRIS_BURST_SPIN_MIN      = 0.5f;            // flt_82001DA0
        const f32 KF_DEBRIS_BURST_TWO           = 2.0f;            // flt_82001D9C (RandomUnitVector's 2r - 1 and 2 sqrt)
        const f32 KF_DEBRIS_BURST_ONE           = 1.0f;            // flt_82001C98
        // The camera target's offset: unk_82181510, the image's (0, 1, 0, 0) row (`lwz r11, 0x8C(r1)` ->
        // `lvx128 v13`, then `vmaddfp v0, v13, 0.5, camera`).
        const f32 KAF_DEBRIS_BURST_CAMERA_UP[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
        // The colour bounds: CRT-initialised .bss vectors (the image holds zeros; each init thunk was RUN
        // on emu64 to read them -- scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/crtinit.py).
        const f32 KF_DEBRIS_BURST_GREY_WEIGHT     = 0.333330005f;  // unk_82FAB840 x/y/z (0x3EAAAA3B), w 0 <- thunk 0x82C49FD8
        const f32 KF_DEBRIS_BURST_SATURATION_LOW  = 0.25f;         // unk_82FAB7F0 splat                     <- thunk 0x82C4A010
        const f32 KF_DEBRIS_BURST_SATURATION_HIGH = 0.75f;         // unk_82FAB890 splat                     <- thunk 0x82C4A038
        const f32 KF_DEBRIS_BURST_ALPHA_LOW       = 0.5f;          // unk_82FAB7D0 splat                     <- thunk 0x82C4A060
        const f32 KF_DEBRIS_BURST_ALPHA_HIGH      = 0.899999976f;  // unk_82FACC10 splat (0x3F666666)       <- thunk 0x82C4A088

        // The debrisparams layout words the handler reads (the names are the attribute names the DWARF lists
        // for the class, matched to the offsets by the reverse-alphabetical layout rule; the offsets are the
        // loads: the prologue 0x8229A6F0..0x8229A700, the four arms of the type switch 0x8229A82C..0x8229A8BC,
        // the probability at 0x8229AB44). debrisparams exposes no accessor (EffectsModule.cpp's
        // DebrisParamsLayout note).
        const u32 KU_BURST_VELOCITY_MIN       = 0x7C;   // Burst_VelocityMin
        const u32 KU_BURST_VELOCITY_MAX       = 0x80;   // Burst_VelocityMax
        const u32 KU_BURST_EMISSION_ANGLE_DEG = 0xD0;   // Burst_EmissionAngleVariance (degrees)
        const u32 KU_BURST_CAMERA_PROBABILITY = 0xD4;   // Burst_CameraDirectedBurstProbability
        struct DebrisBurstTypeFields { u32 muNumParticles, muSizeMin, muSizeMax; };
        const DebrisBurstTypeFields KA_DEBRIS_BURST_TYPE_FIELDS[4] =
        {
            { 0xB4, 0x94, 0xA4 },   // eDebrisArray_Coloured    Burst_NumParticles / SizeMin / SizeMax _Coloured
            { 0xA8, 0x88, 0x98 },   // eDebrisArray_Shiny       ..._Shiny
            { 0xB0, 0x90, 0xA0 },   // eDebrisArray_Dark        ..._Dark
            { 0xAC, 0x8C, 0x9C },   // eDebrisArray_HighDetail  ..._HighDetail
        };

        inline f32 BurstLayoutF32(const u8* lpLayout, u32 luOffset)
        {
            return *reinterpret_cast<const f32*>(lpLayout + luOffset);
        }

        // `fctiwz` + `stfiwx`: truncation to s32, saturating (a NaN gives 0x80000000).
        inline s32 BurstFctiwz(f32 lfValue)
        {
            const f64 lfWide = static_cast<f64>(lfValue);
            if (lfWide != lfWide)
                return static_cast<s32>(0x80000000u);
            if (lfWide >= 2147483647.0)
                return 0x7FFFFFFF;
            if (lfWide <= -2147483648.0)
                return static_cast<s32>(0x80000000u);
            return static_cast<s32>(lfWide);
        }

        // vmsum4fp128 (one rounding of the four-term dot -- FLAG (model), as ShowerDot3).
        inline f32 BurstDot4(const Vector4& lrA, const Vector4& lrB)
        {
            return static_cast<f32>(static_cast<f64>(lrA.x) * lrB.x + static_cast<f64>(lrA.y) * lrB.y
                                  + static_cast<f64>(lrA.z) * lrB.z + static_cast<f64>(lrA.w) * lrB.w);
        }

        // [DIAG] BRN_DEBRIS_BURST_DIAG=1 -- NOT IN THE X360 BINARY, capped at 40 lines. DELETE-WHEN-STABLE.
        // One line per burst record: the pieces spawned into each array (Coloured / Shiny / Dark / HighDetail), how
        // many were thrown at the camera, and the first piece's position and velocity.
        void DebrisBurstWitness(const FireDebrisBurstEvent* lpEvent, const u32 (&lauPerType)[4], u32 luAtCamera,
                                const Vector3& lrFirstPosition, const Vector3& lrFirstVelocity)
        {
            static const bool sbArmed = []() {
                const char* const lpcValue = std::getenv("BRN_DEBRIS_BURST_DIAG");
                return lpcValue != 0 && lpcValue[0] == '1';
            }();
            static u32 suLines = 0;
            if (!sbArmed || suLines >= 40u)
                return;
            ++suLines;
            char lacMsg[320];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[debris-burst] t=%.3f scale=%.3f pieces=%u/%u/%u/%u atCamera=%u first pos=(%.3f,%.3f,%.3f) "
                "vel=(%.3f,%.3f,%.3f)\n",
                static_cast<double>(lpEvent->mfCurrentTime), static_cast<double>(lpEvent->mfScaleFactor),
                lauPerType[0], lauPerType[1], lauPerType[2], lauPerType[3], luAtCamera,
                static_cast<double>(lrFirstPosition.x), static_cast<double>(lrFirstPosition.y),
                static_cast<double>(lrFirstPosition.z), static_cast<double>(lrFirstVelocity.x),
                static_cast<double>(lrFirstVelocity.y), static_cast<double>(lrFirstVelocity.z));
            CgsDev::Log::WriteToLog(lacMsg);
        }

        // CgsNumeric::Random::RandomUnitVector, inlined by the console (0x8229AE44..0x8229AF50): a point of the
        // unit disc by rejection -- two RandomFloat() a try, x first, each `fmsubs r, 2.0, 1.0`, the squared
        // length one fused `fmadds y*y + x*x`, retried while it is >= 1 -- lifted onto the sphere:
        // (2 sqrt(1 - s) x, 2 sqrt(1 - s) y, 2s - 1), w 0.
        inline Vector3 BurstRandomUnitVector(CgsNumeric::Random& lrRandom)
        {
            f32 lfX, lfY, lfLengthSquared;
            do
            {
                lfX = std::fma(lrRandom.RandomFloat(), KF_DEBRIS_BURST_TWO, -KF_DEBRIS_BURST_ONE);
                const f32 lfXSquared = lfX * lfX;
                lfY = std::fma(lrRandom.RandomFloat(), KF_DEBRIS_BURST_TWO, -KF_DEBRIS_BURST_ONE);
                lfLengthSquared = std::fma(lfY, lfY, lfXSquared);
            } while (!(lfLengthSquared < KF_DEBRIS_BURST_ONE));   // `fcmpu ; bge` loops on a NaN too
            const f32 lfScale = std::sqrt(KF_DEBRIS_BURST_ONE - lfLengthSquared) * KF_DEBRIS_BURST_TWO;
            Vector3 lv;
            lv.x = lfScale * lfX;
            lv.y = lfScale * lfY;
            lv.z = std::fma(lfLengthSquared, KF_DEBRIS_BURST_TWO, -KF_DEBRIS_BURST_ONE);
            lv.w = 0.0f;
            return lv;
        }
    }

    void ParticleModule::HandleFireDebrisBurstEvent(const FireDebrisBurstEvent* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Received a Null Event");

        const u8* const lpLayout =
            static_cast<const u8*>(((const Attrib::Instance&)lpEvent->mDebrisParams).GetLayoutPointer());
        const f32 lfConeHalfAngle = BurstLayoutF32(lpLayout, KU_BURST_EMISSION_ANGLE_DEG) * KF_DEBRIS_BURST_DEG_TO_RAD;
        const f32 lfSpeedMin      = BurstLayoutF32(lpLayout, KU_BURST_VELOCITY_MIN);
        const f32 lfSpeedMax      = BurstLayoutF32(lpLayout, KU_BURST_VELOCITY_MAX);

        // The type switch's four values live across the loop, all zero at entry (r14: `li r28, 0 ; mr r14, r28`;
        // v123: `vspltisw128 v125, 0 ; vmr128 v123, v125`; f20 / f15: `lfs f20, flt_82001CC0 ; fmr f15, f20`), so
        // the default arm -- unreachable, the loop runs over 0..3 -- would reuse the previous type's.
        s32     liNumParticles = 0;
        Vector4 lvBaseColour;
        lvBaseColour.x = 0.0f; lvBaseColour.y = 0.0f; lvBaseColour.z = 0.0f; lvBaseColour.w = 0.0f;
        f32     lfSizeMin = 0.0f;
        f32     lfSizeMax = 0.0f;

        u32     lauDiagPieces[4] = { 0u, 0u, 0u, 0u };          // [DIAG] DELETE-WHEN-STABLE
        u32     luDiagAtCamera = 0u;                            // [DIAG]
        Vector3 lvDiagFirstPosition, lvDiagFirstVelocity;       // [DIAG]
        lvDiagFirstPosition.x = lvDiagFirstPosition.y = lvDiagFirstPosition.z = lvDiagFirstPosition.w = 0.0f;
        lvDiagFirstVelocity = lvDiagFirstPosition;

        for (u32 luType = 0; luType < 4u; ++luType)
        {
            // The type switch (jpt 0x8229A81C): the base colour and the type's three layout words.
            switch (luType)
            {
            case Native::eDebrisArray_Coloured:
                lvBaseColour = lpEvent->mvCarColour;                                   // the car's paint (+0x40)
                liNumParticles = *reinterpret_cast<const s32*>(lpLayout + KA_DEBRIS_BURST_TYPE_FIELDS[luType].muNumParticles);
                lfSizeMin = BurstLayoutF32(lpLayout, KA_DEBRIS_BURST_TYPE_FIELDS[luType].muSizeMin);
                lfSizeMax = BurstLayoutF32(lpLayout, KA_DEBRIS_BURST_TYPE_FIELDS[luType].muSizeMax);
                break;
            case Native::eDebrisArray_Shiny:
            case Native::eDebrisArray_Dark:
            case Native::eDebrisArray_HighDetail:
                lvBaseColour = maDebris[luType].Params()->mColour;                     // the preset's +0x20
                liNumParticles = *reinterpret_cast<const s32*>(lpLayout + KA_DEBRIS_BURST_TYPE_FIELDS[luType].muNumParticles);
                lfSizeMin = BurstLayoutF32(lpLayout, KA_DEBRIS_BURST_TYPE_FIELDS[luType].muSizeMin);
                lfSizeMax = BurstLayoutF32(lpLayout, KA_DEBRIS_BURST_TYPE_FIELDS[luType].muSizeMax);
                break;
            default:
                CGS_ASSERT(false, "Unknown Debris Type in ParticleModule !");
                break;
            }

            // The colour bounds: the grey of the base colour pushed 25% / 75% of the way back to it, alpha
            // 0.5 / 0.9 (vmsum4fp128, vsubfp128, two vmaddfp, two vrlimi128 of w -- 0x8229A8F0..0x8229A92C).
            Vector4 lvWeights;
            lvWeights.x = KF_DEBRIS_BURST_GREY_WEIGHT; lvWeights.y = KF_DEBRIS_BURST_GREY_WEIGHT;
            lvWeights.z = KF_DEBRIS_BURST_GREY_WEIGHT; lvWeights.w = 0.0f;
            const f32 lfGrey = BurstDot4(lvWeights, lvBaseColour);
            Vector4 lvLow, lvHigh;
            lvLow.x  = std::fma(lvBaseColour.x - lfGrey, KF_DEBRIS_BURST_SATURATION_LOW, lfGrey);
            lvLow.y  = std::fma(lvBaseColour.y - lfGrey, KF_DEBRIS_BURST_SATURATION_LOW, lfGrey);
            lvLow.z  = std::fma(lvBaseColour.z - lfGrey, KF_DEBRIS_BURST_SATURATION_LOW, lfGrey);
            lvLow.w  = KF_DEBRIS_BURST_ALPHA_LOW;
            lvHigh.x = std::fma(lvBaseColour.x - lfGrey, KF_DEBRIS_BURST_SATURATION_HIGH, lfGrey);
            lvHigh.y = std::fma(lvBaseColour.y - lfGrey, KF_DEBRIS_BURST_SATURATION_HIGH, lfGrey);
            lvHigh.z = std::fma(lvBaseColour.z - lfGrey, KF_DEBRIS_BURST_SATURATION_HIGH, lfGrey);
            lvHigh.w = KF_DEBRIS_BURST_ALPHA_HIGH;
            BrnEffects::Utils::DebrisColourRandomiser lColourRandomiser;
            lColourRandomiser.Prepare(lvLow, lvHigh);

            // count * scale, truncated (`extsw ; fcfid ; frsp ; fmuls ; fctiwz`, 0x8229A8DC..0x8229A95C).
            const s32 liNumToSpawn = BurstFctiwz(static_cast<f32>(liNumParticles) * lpEvent->mfScaleFactor);
            if (liNumToSpawn <= 0)
                continue;

            Native::BrnDebrisArray& lrArray = maDebris[luType];                       // this + 0x22818 + type * 32
            const Vector3& lrHalf    = lpEvent->mvEmitterHalfExtents;                 // +0x10
            const Vector3& lrInherit = lpEvent->mvVelocityToInherit;                  // +0x20
            const Vector3& lrCamera  = lpEvent->mCameraPosition;                      // +0x30
            for (u32 luLeft = static_cast<u32>(liNumToSpawn); luLeft != 0u; --luLeft)
            {
                // The spot in the emitter box: RandomFloat(-h, h) per axis, z drawn first, then y, then x
                // (the range `h - (-h)`, the negation a `vxor` of the sign bit), added to the spawn position.
                const f32 lfOffsetZ = mRandom.RandomFloat(-lrHalf.z, lrHalf.z);
                const f32 lfOffsetY = mRandom.RandomFloat(-lrHalf.y, lrHalf.y);
                const f32 lfOffsetX = mRandom.RandomFloat(-lrHalf.x, lrHalf.x);
                Vector3 lvPosition;
                lvPosition.x = lpEvent->mSpawnPosition.x + lfOffsetX;
                lvPosition.y = lpEvent->mSpawnPosition.y + lfOffsetY;
                lvPosition.z = lpEvent->mSpawnPosition.z + lfOffsetZ;
                lvPosition.w = lpEvent->mSpawnPosition.w + 0.0f;

                Vector3 lvVelocity;
                const f32 lfAimDraw = mRandom.RandomFloat();
                if (!(lfAimDraw < BurstLayoutF32(lpLayout, KU_BURST_CAMERA_PROBABILITY)))   // `fcmpu ; bge` 0x8229AB48
                {
                    // ---- the cone (0x8229AC14) ----
                    const f32 lfLateral = mRandom.RandomFloat(-lfConeHalfAngle, lfConeHalfAngle);
                    const f32 lfAzimuth = mRandom.RandomFloat() * KF_DEBRIS_BURST_TWO_PI;
                    // One TrigBaseFunctions5 evaluation over (az, az, lat, lat) with the (sin, cos, sin, cos)
                    // phases (vmrghw, 0x8229ACCC..0x8229AD2C).
                    const f32 lfSinAzimuth = ShowerSinCosLane(lfAzimuth, KF_SHOWER_SIN_PHASE);
                    const f32 lfCosAzimuth = ShowerSinCosLane(lfAzimuth, KF_SHOWER_COS_PHASE);
                    const f32 lfSinLateral = ShowerSinCosLane(lfLateral, KF_SHOWER_SIN_PHASE);
                    const f32 lfCosLateral = ShowerSinCosLane(lfLateral, KF_SHOWER_COS_PHASE);
                    // (cos az sin lat, cos lat, sin az cos lat, cos az sin lat): the vperm of 0x82CDA350
                    // (00010203 14151617 00010203 00010203 -- A.x, B.y, A.x, A.x) and the vrlimi of z
                    // (0x8229AD68 / 0x8229AD8C), exactly as built -- z pairs sin az with cos lat, not sin lat.
                    Vector3 lvDirection;
                    lvDirection.x = lfCosAzimuth * lfSinLateral;
                    lvDirection.y = lfCosLateral;
                    lvDirection.z = lfSinAzimuth * lfCosLateral;
                    lvDirection.w = lvDirection.x;
                    const f32 lfSpeed   = mRandom.RandomFloat(lfSpeedMin, lfSpeedMax);
                    const f32 lfInherit = std::fma(mRandom.RandomFloat(), KF_DEBRIS_BURST_INHERIT_RANGE,
                                                   KF_DEBRIS_BURST_INHERIT_MIN);
                    // v127 = V * splat(inherit), then vmaddfp128 v127 = dir * splat(speed) + v127.
                    lvVelocity.x = std::fma(lvDirection.x, lfSpeed, lrInherit.x * lfInherit);
                    lvVelocity.y = std::fma(lvDirection.y, lfSpeed, lrInherit.y * lfInherit);
                    lvVelocity.z = std::fma(lvDirection.z, lfSpeed, lrInherit.z * lfInherit);
                    lvVelocity.w = std::fma(lvDirection.w, lfSpeed, lrInherit.w * lfInherit);
                }
                else
                {
                    // ---- at the camera: 0.5 above it, 7 m/s, lifted by half a gravity drop (0x8229AB50) ----
                    Vector3 lvToTarget;
                    lvToTarget.x = std::fma(KAF_DEBRIS_BURST_CAMERA_UP[0], KF_DEBRIS_BURST_HALF, lrCamera.x) - lvPosition.x;
                    lvToTarget.y = std::fma(KAF_DEBRIS_BURST_CAMERA_UP[1], KF_DEBRIS_BURST_HALF, lrCamera.y) - lvPosition.y;
                    lvToTarget.z = std::fma(KAF_DEBRIS_BURST_CAMERA_UP[2], KF_DEBRIS_BURST_HALF, lrCamera.z) - lvPosition.z;
                    lvToTarget.w = std::fma(KAF_DEBRIS_BURST_CAMERA_UP[3], KF_DEBRIS_BURST_HALF, lrCamera.w) - lvPosition.w;
                    const f32 lfDistance   = ShowerGuardedLength3(lvToTarget);
                    const f32 lfFlightTime = lfDistance * KF_DEBRIS_BURST_SECONDS_PER_M;   // fmuls
                    const f32 lfSpeedScale = KF_DEBRIS_BURST_CAMERA_SPEED / lfDistance;    // fdivs
                    const f32 lfLift       = (lfFlightTime * KF_DEBRIS_BURST_HALF) * KF_DEBRIS_BURST_GRAVITY;
                    lvVelocity.x = lvToTarget.x * lfSpeedScale - lfLift;
                    lvVelocity.y = lvToTarget.y * lfSpeedScale - lfLift;
                    lvVelocity.z = lvToTarget.z * lfSpeedScale - lfLift;
                    lvVelocity.w = lvToTarget.w * lfSpeedScale - lfLift;
                    ++luDiagAtCamera;                                                       // [DIAG]
                }

                const f32 lfSize     = mRandom.RandomFloat(lfSizeMin, lfSizeMax);          // f28
                const Vector3 lvAxis = BurstRandomUnitVector(mRandom);
                const f32 lfSpin     = mRandom.RandomFloat();   // `lfsx f30` -- drawn BEFORE the colour's two
                Vector4 lvColour;
                lColourRandomiser.Randomise(lvColour, mRandom);
                lrArray.SpawnDebris(lvPosition, lvVelocity, lvAxis, lvColour,
                                    std::fma(lfSpin, KF_DEBRIS_BURST_SPIN_RANGE, KF_DEBRIS_BURST_SPIN_MIN),
                                    lfSize, lpEvent->mfCurrentTime);
                if ((lauDiagPieces[0] | lauDiagPieces[1] | lauDiagPieces[2] | lauDiagPieces[3]) == 0u)   // [DIAG]
                {
                    lvDiagFirstPosition = lvPosition;
                    lvDiagFirstVelocity = lvVelocity;
                }
                ++lauDiagPieces[luType];                                                    // [DIAG]
            }
        }
        DebrisBurstWitness(lpEvent, lauDiagPieces, luDiagAtCamera, lvDiagFirstPosition, lvDiagFirstVelocity);   // [DIAG]
    }
}
