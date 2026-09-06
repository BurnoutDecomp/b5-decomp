// =================================================================================================
// GameSource/Effects/Particles/ParticleModule_SparkEvents.cpp
//
// THE DISPATCH-THREAD END OF THE GRINDING-SPARK CHAIN.
//
//   BrnParticle::ParticleModule::ProcessEventQueue               @0x8229C418  (113 instr)
//   BrnParticle::ParticleModule::HandleSpawnSparksAlongLineEvent @0x8229A138  (329 instr)
//   ... and the three sibling handlers, ANNOUNCED (see the bottom of this file).
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
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // the announcements

#include <cmath>    // sqrtf
#include <cstdio>   // snprintf (the announcements)

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
                // ⛔ NOT CALLED, AND THE REASON IS A MOUNT, NOT A MISSING BODY: BrnDebrisArray.cpp
                // has both bodies but is DELIBERATELY UNMOUNTED -- its Construct binds
                // `_gaDebrisArrayParams`, an `extern const` with no definition anywhere in the
                // tree (the note is in tools/build/build_game_exe.bat beside the Effects block).
                // Calling it is an unresolved external, which is how this was found.
                // ⚠️ CORRECTED 2026-09-06: this used to add "it is also UNREACHABLE on this
                // build: nothing publishes a type-0 record". That is no longer true --
                // EffectsModule::HandlePlayerTriangleCache @0x82296EA0 posts exactly one
                // type-0 record on the frame a crash ENDS (AllocateEventSafe(0, 0)), and that
                // post was landed in the same wave as this drain. So this arm IS reached, once
                // per crash, and the announcement below is the only thing standing between the
                // console's call and maDebris[0..4].ClearAllBuckets().
                static bool sbLogged = false;
                LogSparkEventNotReconstructed(sbLogged,
                    "ProcessEventQueue case 0 (clear all debris buckets): maDebris[0..4]."
                    "ClearAllBuckets(). BLOCKED -- BrnDebrisArray.cpp is unmounted");
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
                // ⛔ SAME BLOCK AS CASE 0: BrnDebrisArray.cpp is unmounted. And nothing publishes a
                // type-4 record either -- PreRenderUpdate's single AllocateEventSafe is fed by the
                // spawn-buffer pair at +0x2B7A0, whose count no producer in this build raises.
                static bool sbLogged = false;
                LogSparkEventNotReconstructed(sbLogged,
                    "ProcessEventQueue case 4 (debris batch spawn): the n*80 record walk into "
                    "BrnDebrisArray::SpawnDebris. BLOCKED -- BrnDebrisArray.cpp is unmounted");
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
    // The three sibling handlers. NOT RECONSTRUCTED in this wave; each says so in the log the
    // first time a record of its type reaches it, so a dropped record is visible rather than
    // invisible. Nothing in this build publishes any of their types yet:
    //   * type 1 / type 2 come from ParticleModule::SpawnSparksFromPoint / SpawnSparkShowerFromPoint
    //     and from EffectsModule::HandleRaceCarRaceCarSparks @0x82290A48 and ::DoSparkShower
    //     @0x822920C0, which are themselves reached only through ProcessRaceCarContacts;
    //   * type 5 comes from ParticleModule::FireDebrisBurst.
    // Each is a real function on the console (290 / 284 / 600 instructions) and each is the next
    // obvious piece of this file.
    // ---------------------------------------------------------------------------------------------
    void ParticleModule::HandleSpawnSparksFromPointEvent(const SpawnSparksFromPointEvent* /*lpEvent*/,
                                                         const ParticleRenderData& /*lrRenderData*/)
    {
        static bool sbLogged = false;
        LogSparkEventNotReconstructed(sbLogged,
            "ParticleModule::HandleSpawnSparksFromPointEvent @0x82299840 (290 instr) -- a type-1 "
            "record reached ProcessEventQueue and was dropped");
    }

    void ParticleModule::HandleSpawnSparkShowerFromPointEvent(const SpawnSparkShowerFromPointEvent* /*lpEvent*/,
                                                              const ParticleRenderData& /*lrRenderData*/)
    {
        static bool sbLogged = false;
        LogSparkEventNotReconstructed(sbLogged,
            "ParticleModule::HandleSpawnSparkShowerFromPointEvent @0x82299CC8 (284 instr) -- a "
            "type-2 record reached ProcessEventQueue and was dropped");
    }

    void ParticleModule::HandleFireDebrisBurstEvent(const FireDebrisBurstEvent* /*lpEvent*/)
    {
        static bool sbLogged = false;
        LogSparkEventNotReconstructed(sbLogged,
            "ParticleModule::HandleFireDebrisBurstEvent @0x8229A660 (600 instr) -- a type-5 "
            "record reached ProcessEventQueue and was dropped");
    }
}
