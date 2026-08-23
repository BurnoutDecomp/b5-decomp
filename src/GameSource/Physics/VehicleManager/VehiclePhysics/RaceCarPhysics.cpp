#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::{Dot, Add, Subtract, Mult, Normalize, MagnitudeSquared}
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <algorithm> // std::clamp
#include <cmath>     // std::sqrt, std::fabs
#include <cstddef>   // offsetof

// BrnPhysics::Vehicle::RaceCarPhysics -- the two out-of-line ledger funcs owned by the
// Vehicle-physics group (IsCrashingNormally @0x827E42B8, GetHeightAboveRoad @0x825B3998), PLUS the
// C10 showtime / aftertouch / target-assist / bounce-boost group (Update, UpdateShowtimePhysics,
// CapShowtimeVelocities, the showtime singleton PlayerParameters, ...). The X360 build is VMX128
// inline asm; these are the de-SIMD'd named-member equivalents. Showtime state lives in the
// module-static singleton msPlayerParams (defined below), not per-instance.

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    // DATA FORK RETIRED 2026-08-06 (UpdateVehiclePhysics wave). This file used to declare
    //     extern bool gbVehicleBounceBoosting;   // "X360 byte_82FB84B2, un-homed"
    // -- but byte_82FB84B2 is a byte THIS FILE ALREADY MODELS: the showtime singleton's
    // mbLaunchActive (PlayerParameters +0x32; base lbBounceBoosting @0x82FB8480; the layout is
    // committed in RaceCarPhysics.h with that exact address on the member). One console byte,
    // two PC names -- IsCrashingNormally read the extern while every writer stores
    // MS.mbLaunchActive, so the moment this TU mounted, the read and the writes would have
    // silently diverged (the invisible wrong-but-plausible class). IsCrashingNormally now reads
    // the singleton member; the extern (and its tentative definition in the embed check) is gone.

    // ---------------------------------------------------------------------------------------
    // IsCrashingNormally  @0x827E42B8
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::IsCrashingNormally() const
    {
        if (!mbPlayerCarInShowtime)
            return true;
        if (!msPlayerParams.mbLaunchActive)   // byte_82FB84B2 (was the forked extern -- see above)
            return true;
        return false;
    }

    // ---------------------------------------------------------------------------------------
    // SetCrashing  @0x825FFBB0 (vtable slot +0x08) / bool overload @0x825B8A70
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::SetCrashing()
    {
        VehiclePhysics::SetCrashing();
        mbAISlowMo = false; // stb 0,0x1434(this) -- confirmed from the Breaker slot target
    }

    void RaceCarPhysics::SetCrashing(bool lbActivateAISlowMo)
    {
        // Breaker dispatches slot +0x08 unconditionally, even when the bool is false.
        SetCrashing();

        if (lbActivateAISlowMo)
        {
            mfCrashTimer         = 0.0f;
            mInitialCrashAngVel  = GetAngularVelocity();
            mInitialCrashVel     = GetLinearVelocity();
        }
    }

    void RaceCarPhysics::Destruct()
    {
        VehiclePhysics::Destruct();
    }

    void RaceCarPhysics::Release()
    {
        VehiclePhysics::Release();
    }

    // ---------------------------------------------------------------------------------------
    // Prepare  @0x82639CB8  (41 insns -- verifier-recounted; the "42" included the pad word) -- the create leg's vcall target (console vtable slot
    // +0x30), called once per created race car by VehicleManager::ProcessCreateEvents
    // @0x826171D8..0x8261724C. LANDED 2026-08-11 from a targeted IDA export over the ARTIST
    // database (the export-set JSON hole is real; the DATABASE carries the bytes). The
    // complete stream, decoded -- every store named:
    //
    //   0x82639CC4/CC/D4  lwz r8,a52 ; lwz r7,a50 ; lwz r6,a48   <- reload the THREE trailing
    //                     pointers (lpAttribs / lpaWheelPositions / lpafWheelRadii) from this
    //                     function's own incoming stack; the inertia's r6/r7/r8 are OVERWRITTEN and r9/r10 pass
    //                     into a callee with no 9th/10th GPR parameter -- dropped in effect.
    //   0x82639CD8        bl VehiclePhysics::Prepare             (r4=&transform r5=&AABB
    //                     v1..v4 pass through untouched; result -> r28)
    //   0x82639CDC        lbz r29, a54                           <- the lu8StrengthStat byte
    //   0x82639CE8..D0C   cmplwi r29,0xB ; blt skip ; Begin/Fire/EndAssert
    //                     ("lu8StrengthStat < KU8_MAX_STRENGTH", RaceCarPhysics.cpp:192)
    //   0x82639D14        stb  r29, 0x140E(this)                 mu8StrengthStat
    //   0x82639D1C        bl VehiclePhysics::SetTransformFromPositionOnRoad(this, &transform)
    //   0x82639D20..D50   the zeroing tail (f0 = flt_82001CC0 == 0.0f; v0 = vspltisw 0):
    //       stfs f0, 0x1404    mfSlamSteering           = 0.0f
    //       stb  0,  0x135F    mbIsWedgedInWorld        = false  (base VehiclePhysics member)
    //       stfs f0, 0x1408    mfBeachedTime            = 0.0f
    //       stb  0,  0x1435    mbWroteIntoRWInSlowMo    = false
    //       stvx128 v0, 0x13F0 mPropCollisionImpulseSum = zero
    //       stb  0,  0x1434    mbAISlowMo               = false
    //       stb  0,  0x1436    mbDeformedBeyondDriveTimeLimitsInCrash = false
    //   0x82639D54/58     epilogue; r3 = r28 (the callee's bool, returned verbatim)
    //
    // lInertia: received by value (48 bytes -- r6..r10 plus one stack doubleword at the
    // call site) and NEVER READ on X360 -- see the header banner. Deliberately unnamed here
    // so a future reader cannot believe it is consumed.
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::Prepare(Matrix44Affine lOnRoadTransform, Vector3 lLinearVelocity,
                                 Vector3 lAngularVelocity, Vector3 lHandlingBodyOffset,
                                 Vector3 lHalfExtent,
                                 const CgsGeometric::AxisAlignedBox& lrAABB,
                                 rw::physics::Inertia /* dropped on X360 -- see banner */,
                                 VehicleAttribs* lpAttribs, const Vector3* lpaWheelPositions,
                                 const f32* lpafWheelRadii, u8 lu8StrengthStat)
    {
        const bool lbPreparedVehiclePhysics = VehiclePhysics::Prepare(
            lOnRoadTransform, lLinearVelocity, lAngularVelocity, lHandlingBodyOffset,
            lHalfExtent, lrAABB, lpAttribs, lpaWheelPositions, lpafWheelRadii);

        CGS_ASSERT(lu8StrengthStat < KU8_MAX_STRENGTH,
                   "lu8StrengthStat < KU8_MAX_STRENGTH");                            // :192

        mu8StrengthStat = lu8StrengthStat;                                           // stb  0x140E

        SetTransformFromPositionOnRoad(lOnRoadTransform);

        mfSlamSteering                         = 0.0f;                               // stfs 0x1404
        mbIsWedgedInWorld                      = false;                              // stb  0x135F
        mfBeachedTime                          = 0.0f;                               // stfs 0x1408
        mbWroteIntoRWInSlowMo                  = false;                              // stb  0x1435
        mPropCollisionImpulseSum               = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };  // stvx128 0x13F0
        mbAISlowMo                             = false;                              // stb  0x1434
        mbDeformedBeyondDriveTimeLimitsInCrash = false;                              // stb  0x1436

        return lbPreparedVehiclePhysics;                                             // r28
    }

    // ---------------------------------------------------------------------------------------
    // GetHeightAboveRoad  @0x825B3998   (PS3 DecFIGS 0x7A4BD8 -- the mangle is the signature
    //   authority; DWARF prototype RaceCarPhysics.h dwarfdump :310)
    //
    // REWRITTEN 2026-08-14 (walls leg 3): the old body here was built on the DROPPED-
    //   ARGUMENT TRAP -- the X360 Hex-Rays rendered the function with no parameters, and the
    //   reconstruction "resolved" the query vector (`v1`) into the wheel's own contact data,
    //   substituting mfLineDistanceToRoad for the projection. The real function takes the QUERY
    //   POINT as its Vector3 parameter (v1 across both consoles) and answers "how high is this
    //   point above the road plane under each on-ground wheel":
    //
    //   For each of the four driven wheels (asm walks +0x130/+0x210/+0x2F0/+0x3D0, stride 0xE0,
    //   copying the 48-byte RoadContact head to the stack each iteration):
    //     * skip when mbLineTestIsValid (+43 of the copy) is clear;
    //     * on-ground test: dot3(contact.mNormal, up axis @+0x20) > 0.5 (flt_82001DA0, image-read);
    //     * height = dot3(lPoint - contact.mPosition, contact.mNormal)  (`vsubfp v12, v1, v12` --
    //       v1 IS the parameter -- then vmsum3fp against the contact normal);
    //     * keep the running MINIMUM (vcmpgefp/vnot/vand mask + vsel keeps the prior accumulator
    //       for wheels that fail either gate).
    //   Seed accumulator: splat(flt_8208F5EC) == 0x7F7FFFFF == FLT_MAX (image-read this wave --
    //   the old "1.0e30 sentinel FLAG" retires to the measured value). Returns the min broadcast
    //   across the lanes (the X360 stvx128 sret; DWARF types it VecFloat).
    //
    //   No caller existed until ValidateRaceCarWorldContact (walls leg 3) -- the wrong-arity body
    //   was latent, never load-bearing (grep witness: zero in-tree callers before this wave).
    // ---------------------------------------------------------------------------------------
    VecFloat RaceCarPhysics::GetHeightAboveRoad(Vector3 lPoint)
    {
        static const f32 KF_SEED_MAX_HEIGHT         = 3.4028234663852886e+38f; // flt_8208F5EC == 0x7F7FFFFF == FLT_MAX (image-read)
        static const f32 KF_ON_GROUND_DOT_THRESHOLD = 0.5f;                    // flt_82001DA0 (image-read)

        const Vector3 lUpAxis = GetUpAxis();
        f32 lfMinHeight = KF_SEED_MAX_HEIGHT;

        static const EVehicleDrivenWheel KAE_WHEELS[eNumDrivenWheels] = {
            eFrontLeftWheel, eFrontRightWheel, eRearLeftWheel, eRearRightWheel
        };

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            const Wheel::RoadContact& lrContact = GetWheel(KAE_WHEELS[liWheel]).GetRoadContact();
            if (!lrContact.mbLineTestIsValid)
                continue;

            const bool lbOnGround = vpu::Dot(lrContact.mNormal, lUpAxis) > KF_ON_GROUND_DOT_THRESHOLD;
            if (!lbOnGround)
                continue;

            // Signed height of the QUERY POINT above this wheel's contact plane.
            const Vector3 lDelta = vpu::Subtract(lPoint, lrContact.mPosition);
            const f32 lfHeight = vpu::Dot(lDelta, lrContact.mNormal);
            if (lfHeight < lfMinHeight)
                lfMinHeight = lfHeight;
        }

        return VecFloat{ lfMinHeight, lfMinHeight, lfMinHeight, lfMinHeight };
    }

    // =========================================================================================
    // C10 group: showtime / aftertouch / target-assist / bounce-boost.
    //
    // The module-static showtime singleton (X360 msPlayerParams; base symbol lbBounceBoosting). One
    // car at a time.
    // the DEFINITION moved to the mounted slice
    // RaceCarPhysics_ShowtimeBounce.cpp (along with UpdateShowtimeBounceModifiers) -- this TU is
    // still unmounted, and the leaves slice links against the singleton. The extern declaration in
    // RaceCarPhysics.h is unchanged; fold the definition back here when this TU mounts.
    namespace { PlayerParameters& MS = msPlayerParams; }   // short alias for the bodies below
    // -----------------------------------------------------------------------------------------
    // THE SEED CONSTANTS ARE READ (2026-08-03, constants wave). The banner that used to sit
    // here said "the exports do NOT contain their numeric values (verified: 0x82F2A2xx have no
    // function/data export)". That verification was sound but it answered the wrong question: the
    // IDA *function* exports carry no data, and the whole 0x82F2A2xx block is not un-homed .rdata
    // at all -- it is ORDINARY INITIALISED .data that has held its values in the image the entire
    // time. flt_82F2A2A0 literally contains 0x41A00000 = 20.0 in the file. There is no initialiser
    // to disassemble and nothing to recover: one image read settles every row below. Each value
    // here was decoded from the raw 16 bytes IDA reports at that address (big-endian f32).
    //
    // FOR THE NEXT PERSON: "absent from the exports" is not the same as "absent from the image".
    // Five sweeps re-derived this block's *addresses* and none of them read the *bytes*.
    static const f32 KF_TIMEUNTILPUSH_DELAY      = 0.4f;         // flt_82F2A2A4 (SetPlayerVehicleInShowtime seed)
    static const f32 KF_LAUNCH_PUSH_SPEED        = 20.0f;        // flt_82F2A2A0 (launch impulse scale, m/s)
    static const f32 KF_LAUNCH_AIRRAM_FACTOR     = 0.02f;        // flt_82F2A2AC
    static const f32 KF_LAUNCH_AIRRAM_ARG        = 0.1f;         // flt_82F2A2B0
    static const f32 KF_DAMAGE_BUDGET_SCALE      = 0.06f;        // flt_82F2A2C8 (PlayerParameters::Reset seed)
    static const f32 KF_BOUNCE_AIRRAM_FACTOR     = 0.004f;       // flt_82F2A2F4
    static const f32 KF_BOUNCE_AIRRAM_FACTOR_NB  = 0.001f;       // flt_82F2A2F0 (non-boosted bounce)
    static const f32 KF_BOUNCE_VELOCITY_SCALE    = 3.0f;         // flt_82F2A2EC (+1.0 added inline)
    static const f32 KF_PUSH_AIRRAM_FACTOR       = 0.01f;        // flt_82F2A29C
    static const f32 KF_PUSH_AIRRAM_ARG          = 0.2f;         // flt_82F2A298
    static const f32 KF_PUSH_SPIN_SCALE          = 500.0f;       // flt_82F2A2A8
    // RENAMED, NOT JUST FILLED. The five CapShowtimeVelocities caps were carried as a single
    // "speed then vertical" ladder, and the numbers made that reading impossible (an "uncapped"
    // vertical of 9 sitting BELOW the boosting vertical of 20). Re-deriving @0x825D7600 from the
    // asm shows the function clamps TWO DIFFERENT REGISTERS, not one:
    //   * mAngularVelocity (this+0x60) magnitude -> flt_82F2A2D8 / flt_82F2A2DC  (2 / 4 rad/s)
    //   * mLinearVelocity  (this+0x50) magnitude -> flt_82F2A2E0 / flt_82F2A2E4  (8 / 20 m/s)
    // and flt_82F2A2E8 (9.0) is not a third cap in either ladder: 0x825D781C does
    // `fdivs f0, flt_82F2A2E8, f30`, i.e. it forms the RATIO 9/linearCap and clamps the .y lane of
    // the UNIT DIRECTION to it. Multiplied back by the capped magnitude that is an absolute
    // vertical-speed ceiling of 9 m/s, which is why it is smaller than 20 and why it is not
    // ordered with the others. Values were always certain; the semantics were not, and they were
    // wrong. Old names kept in these comments so the previous spellings stay greppable.
    static const f32 KF_CAP_ANGULAR              = 2.0f;         // flt_82F2A2D8 (was KF_CAP_SPEED)
    static const f32 KF_CAP_ANGULAR_BOOST        = 4.0f;         // flt_82F2A2DC (was KF_CAP_SPEED_BOOST)
    static const f32 KF_CAP_LINEAR               = 8.0f;         // flt_82F2A2E0 (was KF_CAP_VERT)
    static const f32 KF_CAP_LINEAR_BOOST         = 20.0f;        // flt_82F2A2E4 (was KF_CAP_VERT_BOOST)
    static const f32 KF_CAP_VERTICAL_SPEED       = 9.0f;         // flt_82F2A2E8 (was KF_CAP_VERT_UNCAPPED)
    static const f32 KF_IDEAL_T_BASE             = 5.0f;         // flt_82F2A330 (ComputeIdealVelocity t base)
    static const f32 KF_AFTERTOUCH_LAT           = -2000.0f;     // flt_82F2A304 (lateral force scale)
    static const f32 KF_AFTERTOUCH_ROLL          = 1400.0f;      // flt_82F2A2FC (roll impulse scale)
    static const f32 KF_AFTERTOUCH_PITCH         = 1400.0f;      // flt_82F2A300 (pitch impulse scale)
    static const f32 KF_TARGET_SCORE_GATE        = 0.765999973f; // flt_82F2A328 (UpdateTargetAssist gate)
    static const f32 KF_GRAVITY                  = 9.8100004f;   // resolved inline literal (ballistic arc)

    // ---------------------------------------------------------------------------------------
    // PlayerParameters::Reset  @0x825B89B8 -- store-for-store from the asm (offsets confirmed).
    //   Zeroes the bounce report + latch block, the +0x10 direction vector, the +0x30 launch block,
    //   the +0x100 vector and the +0x110 sensor count; seeds mfDamageBudget(+0x38)=flt_82F2A2C8,
    //   sets mbBounceBoostPending(+0x09)=1 and miCurrentTargetId(+0xF4)=-1.
    // ---------------------------------------------------------------------------------------
    void PlayerParameters::Reset()
    {
        mbBounceBoosting     = false;   // +0x00 = 0
        mbJustBounced        = false;   // +0x01 = 0
        mbBouncedThisFrame   = false;   // +0x02 = 0
        mbCarBounce          = false;   // +0x03 = 0
        mbGoodImpact         = false;   // +0x04 = 0
        muBounceChainCount   = 0;       // +0x06 = 0  (sth)
        mbShouldBounceBoost  = false;   // +0x08 = 0
        mbBounceBoostPending = true;    // +0x09 = 1  (stb r9=1)
        mbSixaxisTiltApplied = false;   // +0x0A = 0
        mbGoodImpactReport   = false;   // +0x0B = 0
        miOtherEntityId      = 0;       // +0x0C = 0  (stw)
        mBounceDirection     = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // +0x10 stvx128 v0=0

        mbDisableShowtime    = false;   // +0x30 = 0
        mbBounceWasGood      = false;   // +0x31 = 0
        mbLaunchActive       = false;   // +0x32 = 0
        mbLaunchSpin         = false;   // +0x33 = 0
        mfDeformationScale   = 0.0f;    // +0x34 = 0.0
        mfDamageBudget       = KF_DAMAGE_BUDGET_SCALE;   // +0x38 = flt_82F2A2C8 (seed; FLAGGED)
        mfUncappedSpeedTimer = 0.0f;    // +0x3C = 0.0
        mfReserved40         = 0.0f;    // +0x40 = 0.0
        mfTimeUntilPush      = 0.0f;    // +0x44 = 0.0

        miCurrentTargetId    = -1;      // +0xF4 = -1  (stw r8=-1)
        mu8NumBounceSensors  = 0;       // +0x110 = 0
        // (the +0x100 vector store is part of the target/sensor scratch -- left zero by the {} init).
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::Update  @0x826415E8
    //   Run the AI-crash slow-motion timer OR maintain mfSlamSteering, flush prop impulses, chain to
    //   VehiclePhysics::Update, then tick mfTimeSinceTookDownPlayer / mfBeachedTime and latch
    //   mbUsingAftertouch.
    //
    // THE TOP-LEVEL BRANCH IN THIS BODY WAS THE WRONG FLAG -- CORRECTED 2026-08-03 (own-block
    //   recovery wave). It read `if (mbUsingAftertouch)` with the comment "byte710 in the asm". The
    //   asm is `lbz r11, 0x710(r31)` @0x82641624 and +0x710 is **mbCrashing** -- proven first-hand,
    //   not inferred: RaceCarPhysics::GetNormalCausingCrash @0x825B3944 loads the same +0x710 and
    //   asserts on it with the literal string "mbCrashing" and __FILE__ ".../RaceCarPhysics.h",
    //   __LINE__ 328. mbUsingAftertouch is +0x140D and is WRITTEN AT THE BOTTOM OF THIS FUNCTION, so
    //   the committed form branched on its own previous-frame output. Classic address-right /
    //   meaning-wrong, with a self-referential twist.
    //
    // AND THE MEMBER THE CRASHING BRANCH DRIVES IS mfCrashTimer, NOT A "SLAM-STEER ENVELOPE".
    //   +0x1430 is the AI crash timer (DWARF RaceCarPhysics.h:408); it is compared against
    //   KVF_AI_CRASH_PAUSE_TIME to clear mbAISlowMo @+0x1434, which is what gates the
    //   KVF_AI_CRASH_SLOWMO_FACTOR timestep scale. Full asm trace in RaceCarPhysics.h's own-block
    //   comment. Three call sites re-pointed here.
    //
    // THE THREE RODATA SEEDS ARE READ (2026-08-03, constants wave). All three were image
    //   reads, not guesses, and the elided AI-crash slow-mo block below is restored:
    //     flt_820037C8       @0x820037C8 .rdata = 0xBF800000 = -1.0   (mfCrashTimer "not crashing"
    //                        re-seed; the committed placeholder happened to already be -1.0, so the
    //                        VALUE does not move -- only its status, from PLACEHOLDER to attested.
    //                        Eight other committed sites in this tree agree it is -1.0.)
    //     unk_82FB83B0       .data splat, init@0x82C5B6A0 from unk_8208F9B4 @0x8208F9B4
    //                        = 0x3F400000 = 0.75  == KVF_AI_CRASH_PAUSE_TIME (seconds)
    //     unk_82FB8880       .data splat, init@0x82C5B6C0 from unk_8208F9B8 @0x8208F9B8
    //                        = 0x42C80000 = 100.0 == KVF_AI_CRASH_SLOWMO_FACTOR
    //
    // THE FACTOR IS A DIVISOR, and that is proved by the instruction SHAPE, not by taste.
    //   0x826416A8..0x826416D8 loads unk_82FB8880, then runs `vrefp` + TWO Newton-Raphson steps
    //   (`vnmsubfp`/`vmaddfp` pairs against a vcfsx-built 1.0) before the `vmulfp128`. A compiler
    //   emitting `v1 * K` needs none of that -- it would multiply by the loaded vector directly.
    //   The reciprocal refinement is only ever emitted for a DIVIDE, so the multiplier applied to
    //   the sim timestep is 1/100 = 0.01, i.e. a 100x slow-motion window, not a 100x speed-up.
    //   (Same idiom, same reading, at 0x82C5D7D0 and 0x82C5C1C8 elsewhere in the image.)
    //
    // lfTimeStep arrives in a VMX lane (the asm splats v2/v1). THE TWO VECTORS ARE NOT
    //   INTERCHANGEABLE: mfCrashTimer integrates v2 (the REAL frame time, so the slow-mo window is
    //   bounded in real time) while mfTimeSinceTookDownPlayer and mfBeachedTime integrate v1 (the
    // SIM timestep, which the slow-mo path scales). THE SPLIT IS NOW APPLIED. It was deferred
    //   only because the scaling that makes the two differ was elided; with the factor read, leaving
    //   those integrations on v2 would be a NEWLY WRONG result rather than an inert one. The asm
    //   stores v127 (== v1) -- not v126 (== v2) -- into the scratch it adds at 0x826418CC
    //   (mfUncappedSpeedTimer), 0x826418E4 (mfTimeSinceTookDownPlayer) and 0x826419CC (+0x1408).
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::Update(VecFloat lvfSimTimeStep, VecFloat lvfRealTimeStep,
                                const rw::math::vpu::Matrix44Affine* lpCameraMatrix,
                                const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                                bool lbPlayerAftertouchForceAdditive, bool lbShowtimeAllowed,
                                CgsNumeric::Random& lrRandom)
    {
        // THE ZERO TIMESTEP IS GONE (2026-08-01, physics wave 1). This used to read
        //     static const f32 KF_DT = 0.0f;   // FLAG: frame dt ... un-homed here
        // -- a committed zero that made EVERY timer in this function a no-op, and a `0.0f`
        // "placeholder" is never inert: it means *immediately* / *never*. The dt is lane 0 of
        // the second incoming vector register; the asm is quoted in RaceCarPhysics.h above the
        // declaration (`stvx128 v126 ; lfs f13 ; fadds` on mfCrashTimer, at the top of the
        // CRASHING branch). NOT un-homed -- just never read off the asm.
        // TWO dt's, as the console has. lvfRealTimeStep is v2 and lvfSimTimeStep
        // is v1 (the SIM timestep, which the AI crash slow-mo branch below scales by 1/100). Only
        // mfCrashTimer integrates v2; every other timer in this function integrates v1, so v1 is
        // read at its use sites (AFTER the possible scaling) rather than snapshotted here.
        const f32 lfRealDT = lvfRealTimeStep.x;   // v126/v2 lane 0 -- unscaled frame time

        if (mbCrashing)   // asm @0x82641624: `lbz r11, 0x710(r31)` -- see the ⛔ note above
        {
            // Crashing: run the AI crash-slowmo timer. It integrates the REAL frame time (v2), and
            // once it passes KVF_AI_CRASH_PAUSE_TIME the slow-motion window closes.
            mfCrashTimer += lfRealDT;       // asm 0x82641640..0x82641664 (`fadds f0,f0,v2.x`)

            // both constants now read off the image (see the block comment).
            // asm 0x82641668..0x82641698: splat mfCrashTimer, `vcmpgtfp. v13, v0` against
            // unk_82FB83B0 (0.75), and on greater-than clear mbAISlowMo @+0x1434.
            static const f32 KVF_AI_CRASH_PAUSE_TIME   = 0.75f;    // unk_82FB83B0 <- unk_8208F9B4
            static const f32 KVF_AI_CRASH_SLOWMO_FACTOR = 100.0f;  // unk_82FB8880 <- unk_8208F9B8

            if (mfCrashTimer > KVF_AI_CRASH_PAUSE_TIME)
                mbAISlowMo = false;         // asm 0x82641698: `stb r30(0), 0x1434(r31)`

            // asm 0x8264169C..0x826416D8: while mbAISlowMo is set, the SIM timestep v1 is divided
            // by KVF_AI_CRASH_SLOWMO_FACTOR (vrefp + 2 Newton steps, then `vmulfp128 v127, v0, v1`).
            // v127 is v1, and every later use of v1 in this function reads the SCALED value.
            if (mbAISlowMo)
            {
                const f32 lfScale = 1.0f / KVF_AI_CRASH_SLOWMO_FACTOR;
                lvfSimTimeStep = VecFloat{
                    lvfSimTimeStep.x * lfScale, lvfSimTimeStep.y * lfScale,
                    lvfSimTimeStep.z * lfScale, lvfSimTimeStep.w * lfScale };
            }

            mfSlamSteering = 0.0f;          // this->float1404 = 0.0 (flt_82001CC0)
        }
        else
        {
            const f32 lfSteer = lpControls->GetSteer();

            // asm 0x826416F0..0x826416FC: clear mbAISlowMo, then re-seed mfCrashTimer from
            // flt_820037C8. That symbol is READ: 0x820037C8 .rdata = 0xBF800000 = -1.0f, the same
            // "invalid timer" sentinel it carries at eight other committed sites in this tree.
            mbAISlowMo = false;             // asm: `stb r30(0), 0x1434(r31)`
            mfCrashTimer = -1.0f;           // flt_820037C8 == -1.0f (image-attested)

            // RE-NAMED 2026-08-03. The asm is `lwz r11,0x44(r29); cmpwi 1; bne` then
            // `lbz r11,0x4E(r29)` (@0x82641700..0x82641714): the console checks the DRIVER TYPE and,
            // if this car is AI-driven, looks at the AI payload's mbSlamPlayer. Was two invented
            // accessors (GetMode()==1 / GetFlag78()); both are retired.
            if (lpControls->GetType() == E_DRIVER_TYPE_AI)
            {
                if (static_cast<const BrnAIDriverControls*>(lpControls)->mbSlamPlayer)
                    mfSlamSteering += lfSteer * 4.0f * lvfSimTimeStep.x;
            }
            else if (lfSteer * mfSlamSteering < -0.30000001f)
            {
                // 0x82641784..A4: multiply by -0.2, then clamp with the two fsel instructions.
                mfSlamSteering = std::clamp(mfSlamSteering * -0.2f, -0.2f, 0.2f);
            }

            if (std::fabs(lfSteer) >= 0.1f)
                mfSlamSteering += lfSteer * lvfSimTimeStep.x;
            else
                mfSlamSteering = mfSlamSteering * 0.94999999f;   // inside -> decay 0.95/frame

            if (miNumCollisions > 0)   // asm @0x82641814: `lwz r11,0x1354(r31)` -- a WORD at +0x1354, i.e.
                                       // miNumCollisions. The committed source named mi8SlammingRaceCarId
                                       // (a BYTE at +0x13E0) while its own comment cited int1354; RaceCarPhysics
                                       // never references +0x13E0. Corrected during the re-parenting audit.
                mfSlamSteering = 0.0f;

            // clamp +/-10 (fsel min then max)
            if (mfSlamSteering < -10.0f) mfSlamSteering = -10.0f;
            if (mfSlamSteering >  10.0f) mfSlamSteering =  10.0f;
        }

        ApplyPropCollisionImpulseSum();

        // The asm restores BOTH vectors verbatim before this call (`vmr128 v2,v126 ;
        // vmr128 v1,v127` @0x8264185C), i.e. they are pass-through arguments.
        VehiclePhysics::Update(lvfSimTimeStep, lvfRealTimeStep, lpCameraMatrix, lpControls,
                               lbImpactTime, lbPlayerAftertouchForceAdditive,
                               lbShowtimeAllowed, lrRandom);

        // RE-POINTED 2026-08-03 (VehiclePhysics own-block wave). The console gates this
        // follow-up steering pass on a byte at +0x70 (asm @0x82641884: `lbz r11, 0x70(r31) ;
        // cmplwi ; beq`), and the committed body read mbUsingAftertouch (+0x140D) instead -- a
        // different member, and one THIS FUNCTION WRITES twenty lines below, so the condition was
        // reading its own previous-frame output.
        //
        // THE PREVIOUS WAVE'S BLOCKER WAS FALSE. It left this line unfixed because "mbFrozen has
        // no declared member in this tree yet (only a comment in VehiclePhysics.h)". It has had one
        // all along: BrnPhysics::ExternallySimulatedBody::mbFrozen (ExternallySimulatedBody.h:112),
        // whose own banner already documents the leaf seat as +0x70, with the public IsFrozen()
        // accessor right there. Nothing needed declaring and nothing is invented here.
        // Independent attestation added this wave: SimpleVehiclePhysics::Construct @0x8262047C does
        // `stb r30(0), 0x70(r31)` with r31 == the leaf `this` -- Construct clearing mbFrozen -- and
        // the base chain closes on +0x130 from exactly that framing (see BrnSimpleVehiclePhysics.h).
        // RE-NAMED 2026-08-03: the byte at controls+0x41 is mbIsSteeringWheel, not a "car type"
        // (UpdateDriving @0x82638348 passes the same +0x41 byte to UpdateSteering, and
        // ModifyControlsForSteeringWheelInput is gated on it @0x826381A8).
        // RE-POINTED 2026-08-07 (orchestrator wave) to the real 4-arg signature. The asm
        // @0x82641890..0x826418A4: f1 = controls->mfSteering (+0x10), f2 = f31 which this
        // function loads from flt_82001CC0 == 0.0f (no gas on the frozen path), v1 = v127
        // (the sim timestep, slow-mo scaled), r6 = mbIsSteeringWheel.
        if (IsFrozen())   // asm @0x82641884: lbz r11, 0x70(r31) == ExternallySimulatedBody::mbFrozen
            VehiclePhysics::UpdateSteering(
                lpControls->GetSteer(), 0.0f,
                lvfSimTimeStep,
                lpControls->mbIsSteeringWheel);

        // Decay the uncapped-speed window timer while it is positive.
        // v1, NOT v2: asm 0x826418CC stores v127 (the sim timestep, slow-mo scaled) into the
        // scratch it then subtracts at 0x826418D4. Was lfDT (v2) while the scaling was elided.
        if (mbPlayerCarInShowtime && MS.mfUncappedSpeedTimer > 0.0f)
            MS.mfUncappedSpeedTimer -= lvfSimTimeStep.x;

        // RE-POINTED 2026-08-03. This store is at +0x1400 (asm 0x826418E0: `lfs f0,0x1400(r31)`
        // / 0x826418F8: `stfs f0,0x1400(r31)`), i.e. mfTimeSinceTookDownPlayer -- the post-takedown
        // invulnerability clock HasRecentlyTakendownPlayer reads. It was written into the +0x1430
        // member, which is a different timer entirely.
        // v1, NOT v2: asm 0x826418E4 stores v127 (the sim timestep) into the scratch it adds at
        // 0x826418F0. Was lfDT (v2) while the slow-mo scaling that makes the two differ was elided.
        mfTimeSinceTookDownPlayer += lvfSimTimeStep.x;

        // Latch aftertouch-active for this frame: needs the request flag (a5), an aftertouch-enable
        // input ( > 0 ) and the virtual "can use aftertouch" query (vtbl+20).
        bool lbUsing = true;
        // RE-BOUND at the signature conform: the latch reads the AFTERTOUCH request flag --
        // r7 == lbPlayerAftertouchForceAdditive under the recovered arg map (the old binding
        // read r6, which is the manager's mbImpactTime byte; this body's own comment already
        // said "the request flag (a5)").
        if (!lbPlayerAftertouchForceAdditive || lpControls->GetAftertouchEnable() <= 0.0f /* (a3+32) */
            || !IsPlayerVehicleActuallyInShowtime() /* the (*+20) virtual on this path */)
            lbUsing = false;
        mbUsingAftertouch = lbUsing;

        // 0x82641938..E0: the exact beached-time gate. The four byte loads at
        // +0x206/+0x2E6/+0x3C6/+0x4A6 are Wheel::mbHasTraction (+0xD6 within each wheel),
        // not RoadContact::mbIsOnGround.
        static const f32 KF_MPH_TO_MPS = 0.447039992f; // unk_83017FE0 <- flt_82F31928
        const bool lbEveryWheelHasTraction =
            maWheels[eFrontLeftWheel].mbHasTraction
            && maWheels[eFrontRightWheel].mbHasTraction
            && maWheels[eRearLeftWheel].mbHasTraction
            && maWheels[eRearRightWheel].mbHasTraction;

        if (GetSpeedMPH().x * KF_MPH_TO_MPS < 1.0f
            && !mbHasAir
            && !lbEveryWheelHasTraction
            && mbContactingWall
            && !mbCrashing)
        {
            mfBeachedTime += lvfSimTimeStep.x;
        }
        else
        {
            mfBeachedTime = 0.0f;
        }
    }

    // DecFIGS @0x6EE02C is a one-branch thunk. There is no separate Race-only post-simulation
    // behavior to reconstruct: the Showtime boundary remains the virtual predicate inside the
    // VehiclePhysics base implementation.
    void RaceCarPhysics::UpdatePostSimulation(VecFloat lvfTimeStep)
    {
        VehiclePhysics::UpdatePostSimulation(lvfTimeStep);
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::IsPlayerVehicleInShowtime  @0x825D7B68
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::IsPlayerVehicleInShowtime() const
    {
        if (!mbPlayerCarInShowtime)
            return false;
        if (MS.mbDisableShowtime)            // byte_82FB84B0
            return false;
        if (MS.mfTimeUntilPush > 0.0f)       // still in the launch-push delay
            return false;
        return true;
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::IsPlayerVehicleWithUncappedShowtimeSpeed  @0x825B8C18  (asserts showtime)
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::IsPlayerVehicleWithUncappedShowtimeSpeed() const
    {
        // assert(mbPlayerCarInShowtime) -- debug-only, elided.
        return MS.mfUncappedSpeedTimer > 0.0f;   // flt_82FB84BC > 0
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::GetShowtimePlayerCarStrength  @0x825B8BC0  (asserts showtime)
    // ---------------------------------------------------------------------------------------
    f32 RaceCarPhysics::GetShowtimePlayerCarStrength() const
    {
        // assert(mbPlayerCarInShowtime) -- elided.
        return MS.mfPlayerCarStrength;   // lfShowtimePlayerCarStrength
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::IsBounceBoosting  @0x825B8C90  (asserts showtime)
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::IsBounceBoosting() const
    {
        // assert(mbPlayerCarInShowtime) -- elided.
        return MS.mbBounceBoosting;   // lbBounceBoosting
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::ShouldBounceBoostNextImpact  @0x825B8CE0  (asserts showtime)
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::ShouldBounceBoostNextImpact() const
    {
        // assert(mbPlayerCarInShowtime) -- elided.
        return MS.mbBounceBoostPending;   // byte_82FB8489
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::SetShowtimeAimDirection  @0x825B8AF0
    //   Stash the aim direction (one VMX register) into the singleton @ +0x10.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::SetShowtimeAimDirection(const Vector3& lvAimDirection)
    {
        MS.mBounceDirection = lvAimDirection;   // stvx128 v1, (lbBounceBoosting + 0x10)
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::SetJustBounced  @0x825B8D38  (asserts showtime)
    //   Arm the bounce latch: mbJustBounced=1, store the bounce direction @ +0x10, and record the
    //   car-bounce / good-impact flags + the other entity id.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::SetJustBounced(Vector3 lvBounceDirection, bool lbFirstBounce,
                                        bool lbOverMinStress, EntityId lOtherEntityId)
    {
        // assert(mbPlayerCarInShowtime) -- elided.
        MS.mbJustBounced     = true;                  // byte_82FB8481 = 1
        MS.mBounceDirection  = lvBounceDirection;     // stvx128 v127 @ +0x10
        MS.mbCarBounce       = lbFirstBounce;         // byte_82FB8483 = a2
        MS.mbGoodImpactReport = lbOverMinStress;      // byte_82FB848B = a3
        MS.miOtherEntityId   = (s32)lOtherEntityId.muValue;   // dword_82FB848C = a4
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::GetRecentBounce  @0x825B8B08  (asserts showtime)
    //   Copy out the recent-bounce report and CONSUME the per-frame latches.
    // ---------------------------------------------------------------------------------------
    bool RaceCarPhysics::GetRecentBounce(s32* lpChainCount, bool* lpOverMinStress, bool* lpCarBounce,
                                         bool* lpGoodImpact, bool* lpExtraFlag, s32* lpOtherEntityId,
                                         Vector3* lpBounceDirection)
    {
        // assert(mbPlayerCarInShowtime) -- elided.
        *lpChainCount     = MS.muBounceChainCount;    // *a2 = word_82FB8486
        *lpOverMinStress  = MS.mbBounceWasGood;       // *a3 = byte_82FB84B1
        *lpCarBounce      = MS.mbCarBounce;            // *a4 = byte_82FB8483
        *lpGoodImpact     = MS.mbGoodImpact;           // *a5 = byte_82FB8484
        *lpExtraFlag      = MS.mbGoodImpactReport;     // *a6 = byte_82FB848B
        *lpOtherEntityId  = MS.miOtherEntityId;        // *a7 = dword_82FB848C
        *lpBounceDirection = MS.mBounceDirection;       // stvx128 (+0x10) -> *a8

        const bool lbBounced = MS.mbBouncedThisFrame;  // result = byte_82FB8482
        MS.mbBouncedThisFrame = false;                 // byte_82FB8482 = 0
        MS.mbGoodImpact       = false;                 // byte_82FB8484 = 0
        MS.mbCarBounce        = false;                 // byte_82FB8483 = 0
        return lbBounced;
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::GetNormalCausingCrash  @0x825B3928  (asserts the crash-active latch)
    //   Return the stored crash normal (mCrashNormal @ this+0x1440) through the compiler's hidden
    //   result buffer. The asm asserts the crash-active flag at this+0x710 first
    //   (`lbz r11,0x710(r30); cmplwi; bne` -> assert 'mbCrashing'). That +0x710 byte lives in the
    //   base VehiclePhysics (mbIsCrashing) and is read via the base IsCrashing() accessor -- it is
    //   NOT a fresh RaceCarPhysics member (declaring one at +0x710 would collide with the base).
    // ---------------------------------------------------------------------------------------
    Vector3 RaceCarPhysics::GetNormalCausingCrash()
    {
        CGS_ASSERT(IsCrashing(), "mbCrashing");   // lbz this+0x710 (base mbIsCrashing)
        return mCrashNormal;   // lvx128 this+0x1440 -> hidden-sret stvx128
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::AddTractionPoint  @0x825FFAE8  (49 insns)
    // RE-SIGNATURED 2026-08-07 (orchestrator wave). The committed 2-arg form and its
    //   "chains to a 2-argument VehiclePhysics entry" reading were a slice artefact: the asm
    //   at 0x825FFB04 does `bl SimpleVehiclePhysics::AddTractionPoint` with EVERY incoming
    //   register untouched (r4 = wheel, r5 = tag, v1 = position, v2 = normal), and the PS3
    //   DecFIGS mangles this override as the same 4-arg
    //   (EVehicleDrivenWheel, Vector3, Vector3, u32) as the base (@0x6E77E4 -> @0x6E742C).
    //   The base body is now real (BrnSimpleVehiclePhysics.cpp), so the chain is closed
    //   end to end, and the previously ELIDED tail is modelled BY NAME:
    //     0x825FFB0C  if (mfBeachedTime (+0x1408) >= 0.5 [unk_82FB9180 <- flt_82001DA0,
    //                 static-init splat @0x82C5D030])
    //     0x825FFB54    snapshot maWheels[leWheel]'s 48-byte record from +0x130 rel (i.e. the
    //                   whole RoadContact through +0x2F) -- the copy exists only to read ONE
    //                   byte out of it:
    //     0x825FFB80    if (byte +0x2A of the copy -- RoadContact::mbIsCloseToGround)
    //     0x825FFB90      maWheels[leWheel].mRoadContact.mbIsOnGround (+0x158 rel this) = 1
    //   i.e. after the beached/launch window, a merely CLOSE wheel is promoted to grounded.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::AddTractionPoint(EVehicleDrivenWheel leWheel, Vector3 lvPosition,
                                          Vector3 lvNormal, u32 lu32CollisionTag)
    {
        static const f32 KF_TRACTION_PUSH_THRESHOLD = 0.5f;   // unk_82FB9180 (attested)

        SimpleVehiclePhysics::AddTractionPoint(leWheel, lvPosition, lvNormal, lu32CollisionTag);

        // asm: lfs f0, 0x1408(r31) ; vcmpgefp. against the 0.5 splat.
        if (mfBeachedTime >= KF_TRACTION_PUSH_THRESHOLD)
        {
            // the 6x8-byte stack snapshot reads RoadContact::mbIsCloseToGround (+0x2A of the
            // copied record) and promotes the on-ground flag.
            Wheel& lrWheel = maWheels[leWheel];
            if (lrWheel.mRoadContact.mbIsCloseToGround)
                lrWheel.mRoadContact.mbIsOnGround = true;   // stb 1, 0x158(r8)
        }
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::ApplyPropCollisionImpulseSum  @0x82600780
    //   Soft-clamp the accumulated prop-collision impulse against the car's mass/speed so props can't
    //   catapult the car, apply it, then zero the accumulator.
    //   Structure (de-SIMD'd): scale the impulse, project out its positive component along world-up
    //   (this+0x20), component-clamp it to 20% of the car momentum when speed^2 > 0.5, fade it to
    //   zero from 30 to 85 MPH, gate it on mbAllWheelsHaveTraction (+0x135B), apply it, and clear
    //   the accumulator. All numeric values are read from Breaker's static-init/data chain.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::ApplyPropCollisionImpulseSum()
    {
        static const f32 KF_PROP_IMPULSE_MODIFIER = 0.05f; // unk_82FB91C0
        static const f32 KF_MAX_SLOWDOWN          = 0.2f;  // unk_82FB8430
        static const f32 KF_MIN_SPEED_SQUARED     = 0.5f;  // flt_82001DA0
        static const f32 KF_SPEED_LOWER_LIMIT_MPH = 30.0f; // unk_82FB9C00
        static const f32 KF_SPEED_RANGE_MPH       = 55.0f; // unk_82FB8B80 = 85 - 30

        Vector3 lImpulse = vpu::Mult(mPropCollisionImpulseSum, KF_PROP_IMPULSE_MODIFIER);

        // Remove only the upward component. A downward component is retained exactly as the
        // `vmax(dot(up, impulse), 0)` / subtract pair at 0x826007D4..F4 does.
        const Vector3 lWorldUp = GetWorldUpRow();
        const f32 lfUpComponent = std::max(vpu::Dot(lWorldUp, lImpulse), 0.0f);
        lImpulse = vpu::Subtract(lImpulse, vpu::Mult(lWorldUp, lfUpComponent));

        const Vector3 lLinearVelocity = GetLinearVelocity();
        if (vpu::MagnitudeSquared(lLinearVelocity) > KF_MIN_SPEED_SQUARED)
        {
            const Vector3 lMomentum = vpu::Mult(lLinearVelocity, GetMass().x);
            const f32 lfComponentLimit = vpu::Magnitude(lMomentum) * KF_MAX_SLOWDOWN;
            lImpulse.x = std::clamp(lImpulse.x, -lfComponentLimit, lfComponentLimit);
            lImpulse.y = std::clamp(lImpulse.y, -lfComponentLimit, lfComponentLimit);
            lImpulse.z = std::clamp(lImpulse.z, -lfComponentLimit, lfComponentLimit);
            lImpulse.w = std::clamp(lImpulse.w, -lfComponentLimit, lfComponentLimit);
        }
        else
        {
            lImpulse.SetZero();
        }

        const f32 lfSpeedFactor = std::clamp(
            (GetSpeedMPH().x - KF_SPEED_LOWER_LIMIT_MPH) / KF_SPEED_RANGE_MPH,
            0.0f, 1.0f);
        lImpulse = vpu::Mult(lImpulse, 1.0f - lfSpeedFactor);

        if (!mbAllWheelsHaveTraction)
            lImpulse.SetZero();

        AddWorldSpaceImpulse(lImpulse);
        mPropCollisionImpulseSum.SetZero();
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::ComputeIdealVelocity  @0x82600558
    //   Solve a projectile arc to the target. dir = Flatten(target - bodyPos@+0x40); if its 2D
    //   distance >= 1.0, t = dist / (KF_IDEAL_T_BASE - lfInputSpeed); horizontal = unit2D * dist/(2t),
    //   vertical = t^2 * 9.81; combined into the result. Else return the target's own velocity
    //   (bodyPos@+0x50). lfInputSpeed is the car's 2D speed.
    //   FLAG: KF_IDEAL_T_BASE (flt_82F2A330) is un-homed; the arc math is exact (9.81 is an inline
    //   literal). a2 is the target body (read at +0x40 position, +0x50 velocity).
    // ---------------------------------------------------------------------------------------
    Vector3* RaceCarPhysics::ComputeIdealVelocity(Vector3* lpResult, f32 lfInputSpeed) const
    {
        // The target body's position/velocity are read at +0x40 / +0x50 of a2 in the asm. In this
        // minimal slice the "target body" is the singleton's recorded aim/intercept; faithful read is
        // mBounceDirection as the relative direction. FLAG: target-body pointer un-modelled.
        const Vector3 lvTargetRel = MS.mBounceDirection;   // (target - bodyPos) before Flatten

        // Flatten: drop the vertical (y) component -> a ground-plane direction (BrnMath::Flatten).
        Vector3 lvFlat = lvTargetRel;
        lvFlat.y = 0.0f;

        const f32 lfDistSq = lvFlat.x * lvFlat.x + lvFlat.z * lvFlat.z;   // vmsum of the 2 lanes
        const f32 lfDist   = (lfDistSq > 0.0f) ? std::sqrt(lfDistSq) : 0.0f;

        if (lfDist >= 1.0f)
        {
            const f32 lfDenom = KF_IDEAL_T_BASE - lfInputSpeed;   // flt_82F2A330 - a3 (FLAGGED base)
            const f32 lfT     = (lfDenom != 0.0f) ? (lfDist / lfDenom) : 0.0f;

            // horizontal component: unit2D * (1 / (t * 2)) ; then scaled by the original dir
            const f32 lfHorizScale = (lfT != 0.0f) ? (1.0f / (lfT * 2.0f)) : 0.0f;
            Vector3 lvHoriz = vpu::Mult(vpu::Normalize(lvFlat), lfDist * lfHorizScale);

            // vertical component: t^2 * gravity
            const f32 lfVert = (lfT * lfT) * KF_GRAVITY;
            lvHoriz.y = lfVert;

            *lpResult = lvHoriz;
        }
        else
        {
            // horizontal distance below 1.0 -> just take the target's own velocity (a2 + 0x50).
            *lpResult = GetLinearVelocity();   // FLAG: target-body velocity; here the car's own
        }
        return lpResult;
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateShowtimeBounceModifiers @0x825D7940 -- MOVED 2026-08-06 (PhysicsModule
    // ::Update leaves wave) to the mounted slice RaceCarPhysics_ShowtimeBounce.cpp, together with
    // the msPlayerParams definition above: VehicleManager::ProcessDeformationStates (mounted this
    // wave) calls it, and this TU must stay unmounted while its un-read rodata stands. The body's
    // own FLAG (the elided per-sensor scatter) moved with it, unchanged. Fold back on mount.
    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::SetPlayerVehicleInShowtime  @0x826000F8
    //   On entry (lbInShowtime): reset the singleton; if the car is not already in showtime and not
    //   on the downforce/crash path, give it a DOUBLE-IMPULSE launch -- overwrite mLinearVelocity with
    //   a scaled push (KF_LAUNCH_PUSH_SPEED * unit-velocity) AND fire an AddAirRam (input-space 5130
    //   if rising / 3082 if falling) -- then seed mfTimeUntilPush + mark mbLaunchActive. Always store
    //   the strength + scale the damage budget. (Asserts on velocity validity + damage-limit range.)
    //   FLAG: the push speed / airram factors / budget scale are un-homed rodata (placeholders).
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::SetPlayerVehicleInShowtime(bool lbInShowtime, f32 lfPlayerCarStrength,
                                                    f32 lfPlayerCarDamageLimit)
    {
        if (lbInShowtime)
        {
            MS.Reset();   // BrnPhysics::Vehicle::PlayerParameters::Reset(&lbBounceBoosting)

            if (!mbPlayerCarInShowtime)   // !_R30[5132]
            {
                f32 lfTimeUntilPush;
                if (mbUsingAftertouch /* _R30[4944] (downforce) || _R30[1808] (crashing) */)
                {
                    lfTimeUntilPush = 0.001f;   // already airborne/crashing -> tiny delay, no relaunch
                }
                else
                {
                    // assert(IsValid(GetLinearVelocity())) -- elided.
                    // launch impulse: overwrite velocity with KF_LAUNCH_PUSH_SPEED along the (guarded)
                    // unit velocity direction. The asm normalises mLinearVelocity (+0x50) with a small
                    // floor (flt_82FB7E28) then scales by flt_82F2A2A0.
                    Vector3 lvUnitVel = vpu::Normalize(GetLinearVelocity());
                    Vector3 lvPush    = vpu::Mult(lvUnitVel, KF_LAUNCH_PUSH_SPEED);   // FLAGGED speed
                    SetLinearVelocity(lvPush);   // stvx128 v0 -> +0x50 (overwrite, the "double impulse")

                    // AddAirRam: input-space 5130 if the vertical velocity (vel.y) is rising, else 3082
                    // (the input-space dir bits the asm passes). 6-arg DWARF form (C08-reconciled):
                    // flags, factor, decay, customImpulse, customPosition, timerTillFire.
                    const bool lbRising = !(GetLinearVelocity().y < 0.0f);   // vcmpgefp vel.y >= 0
                    AddAirRam(lbRising ? 5130u : 3082u, KF_LAUNCH_AIRRAM_FACTOR, KF_LAUNCH_AIRRAM_ARG,
                              Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f);

                    lfTimeUntilPush = KF_TIMEUNTILPUSH_DELAY;   // flt_82F2A2A4 (FLAGGED)
                    MS.mbLaunchActive = true;                   // byte_82FB84B2 = 1
                }
                MS.mfTimeUntilPush = lfTimeUntilPush;   // flt_82FB84C4 = v68
            }
        }

        mbPlayerCarInShowtime = lbInShowtime;        // _R30[5132] = v7
        MS.mbDisableShowtime  = false;               // byte_82FB84B0 = 0
        MS.mfPlayerCarStrength = lfPlayerCarStrength; // lfShowtimePlayerCarStrength = param_1

        // assert(0 < lfPlayerCarDamageLimit < 100) -- elided.
        MS.mfDamageBudget = KF_DAMAGE_BUDGET_SCALE * lfPlayerCarDamageLimit;   // flt_82F2A2C8 * limit
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::CapShowtimeVelocities  @0x825D7600  (gated on mbLaunchActive)
    //
    // REWRITTEN 2026-08-03. The committed body clamped ONE vector (mLinearVelocity) with both
    //   cap pairs. The asm clamps TWO SEPARATE REGISTERS, and the second one it never touched:
    //     0x825D7634  `addi r29, r30, 0x60`  -> mAngularVelocity, clamped first
    //     0x825D7730  `addi r28, r30, 0x50`  -> mLinearVelocity,  clamped second
    //   ExternallySimulatedBody.h pins mLinearVelocity @+0x50 and mAngularVelocity @+0x60, so the
    //   register identities are not in doubt. Both cap pairs are chosen up front (0x825D7668/766C
    //   default, 0x825D76E8/76EC when lbBounceBoosting), then used one per register.
    //
    //   ANGULAR (0x825D76F0..0x825D7724): if |omega| > cap, rescale to the cap along its unit
    //   direction. Nothing else -- no vertical term, no uncapped-window exemption.
    //
    //   LINEAR (0x825D7760..0x825D78xx), in the asm's own order:
    //     1. take |v| and its unit direction; if |v| is within FLT_EPSILON of zero (the
    //        `vandc`+`vcmpgtfp` against stru_8208F620 @0x825D77E8) skip straight to step 3.
    //     2. unless IsPlayerVehicleWithUncappedShowtimeSpeed(), clamp the .y lane OF THE UNIT
    //        DIRECTION to KF_CAP_VERTICAL_SPEED / linearCap (`fdivs f0,f0,f30` @0x825D7820,
    //        `vspltw v0,v13,1` then `vcmpgtfp.` @0x825D7844-50).
    //     3. clamp the MAGNITUDE to linearCap (`fcmpu f31,f30 ; ble ; fmr` @0x825D7898).
    //     4. only if step 2 or step 3 actually changed something (r29), rebuild
    //        v = unit' * magnitude' and store back to +0x50.
    //   Clamping the direction's .y and then the magnitude is what makes 9.0 an absolute vertical
    //   speed limit: (9/cap) * cap = 9 m/s regardless of which cap is in force.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::CapShowtimeVelocities()
    {
        if (!MS.mbLaunchActive)   // byte_82FB84B2
            return;

        // assert(mbPlayerCarInShowtime) -- elided.
        f32 lfAngularCap = KF_CAP_ANGULAR;   // flt_82F2A2D8
        f32 lfLinearCap  = KF_CAP_LINEAR;    // flt_82F2A2E0
        if (MS.mbBounceBoosting)             // lbBounceBoosting
        {
            lfAngularCap = KF_CAP_ANGULAR_BOOST;   // flt_82F2A2DC
            lfLinearCap  = KF_CAP_LINEAR_BOOST;    // flt_82F2A2E4
        }

        // ---- 1. angular velocity (this+0x60): plain magnitude clamp ----
        Vector3 lvAngular = GetAngularVelocity();
        const f32 lfAngSq  = vpu::MagnitudeSquared(lvAngular);
        const f32 lfAngMag = (lfAngSq > 0.0f) ? std::sqrt(lfAngSq) : 0.0f;
        if (lfAngMag > lfAngularCap)
        {
            lvAngular = vpu::Mult(vpu::Normalize(lvAngular), lfAngularCap);
            SetAngularVelocity(lvAngular);   // stvx128 back to +0x60
        }

        // ---- 2. linear velocity (this+0x50): direction .y clamp, then magnitude clamp ----
        Vector3 lvLinear = GetLinearVelocity();
        const f32 lfLinSq  = vpu::MagnitudeSquared(lvLinear);
        f32 lfLinMag = (lfLinSq > 0.0f) ? std::sqrt(lfLinSq) : 0.0f;
        Vector3 lvUnit = vpu::Normalize(lvLinear);
        bool lbChanged = false;

        // The asm's zero-length guard: |magnitude| must exceed FLT_EPSILON (stru_8208F620) before
        // the direction is worth touching -- a zero-length velocity has no meaningful unit vector.
        static const f32 KF_LENGTH_EPSILON = 1.1920929e-07f;   // stru_8208F620 (== FLT_EPSILON)
        if (std::fabs(lfLinMag) > KF_LENGTH_EPSILON && !IsPlayerVehicleWithUncappedShowtimeSpeed())
        {
            const f32 lfUnitYLimit = KF_CAP_VERTICAL_SPEED / lfLinearCap;   // 9 / (8 or 20)
            if (lvUnit.y > lfUnitYLimit)
            {
                lvUnit.y  = lfUnitYLimit;
                lbChanged = true;
            }
        }

        if (lfLinMag > lfLinearCap)
        {
            lfLinMag  = lfLinearCap;
            lbChanged = true;
        }

        if (lbChanged)
            SetLinearVelocity(vpu::Mult(lvUnit, lfLinMag));   // stvx128 back to +0x50
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateShowtimePhysics  @0x825FFBD8  (asserts IsPlayerVehicleActuallyInShowtime)
    //   The bounce-boost state machine. Determine "bouncing" (airborne OR slow enough: speed below a
    //   threshold). Run the latch: when mbJustBounced is armed, settle mbBounceBoosting from the
    //   pending flag + the launch-push timer, raise the bounced-this-frame + boost flags, and bump the
    //   chain counter when chaining. A separate condition (button-63 + airborne/contact) force-sets
    //   bounce-boost. If bouncing this frame and in showtime: when boosting, apply a yaw/spin
    //   AddWorldSpaceAngularImpulse along (mBounceDirection + worldUp) + an AddAirRam boost; else a
    //   weaker AddAirRam. When mfTimeUntilPush expires, fire the launch pop (AddAirRam + extra spin)
    //   and set mbBounceBoosting. Finally CapShowtimeVelocities.
    //   FLAG: every flt_82F2A2xx magnitude here is un-homed rodata (placeholder).
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::UpdateShowtimePhysics(const Vector3& lvLaunchSpin, const Vector3& lvAimSpin,
                                               f32 lfTimeStep)
    {
        // assert(IsPlayerVehicleActuallyInShowtime()) -- elided.

        // "bouncing" = airborne (timeStep >= speed guard) OR current speed below KF_BOUNCE_VELOCITY_SCALE.
        bool lbBouncing = false;
        if (lfTimeStep >= 0.0f /* the v37=0 >= v127 guard: airborne lane test */)
        {
            lbBouncing = true;
        }
        else
        {
            const f32 lfSpeed = std::sqrt(vpu::MagnitudeSquared(GetLinearVelocity()));   // +0x50 |v|
            // flt_82F2A2EC is the bounce speed threshold (FLAGGED); the asm adds nothing here.
            if (KF_BOUNCE_VELOCITY_SCALE > lfSpeed)
                lbBouncing = true;
        }

        bool lbDidBounce = false;
        MS.mbBouncedThisFrame = false;   // byte_82FB8482 = 0
        MS.mbBounceWasGood    = false;   // byte_82FB84B1 = 0
        MS.mbDisableShowtime  = lbBouncing;   // byte_82FB84B0 = v15 (the bouncing flag)

        if (MS.mbJustBounced)   // byte_82FB8481
        {
            // settle mbBounceBoosting from the pending flag + the launch-push timer
            bool lbBoost = false;
            if (MS.mbBounceBoostPending /* byte_82FB8489 */
                || (mbPlayerCarInShowtime && MS.mfTimeUntilPush > 0.0f))
                lbBoost = true;
            MS.mbBounceBoosting    = lbBoost;   // lbBounceBoosting = v20
            MS.mbJustBounced       = false;     // byte_82FB8481 = 0
            MS.mbBouncedThisFrame  = true;      // byte_82FB8482 = 1
            MS.mbShouldBounceBoost = false;     // byte_82FB8488 = 0
            MS.mbLaunchActive      = true;      // byte_82FB84B2 = 1
            MS.mbLaunchSpin        = true;      // byte_82FB84B3 = 1

            if (lbBouncing || !IsBounceBoosting())
                MS.muBounceChainCount = 0;      // word_82FB8486 = 0
            else
            {
                lbDidBounce = true;
                MS.mbGoodImpact = true;         // byte_82FB8484 = 1
                ++MS.muBounceChainCount;        // ++word_82FB8486
            }
        }

        // force-set bounce-boost when the bounce button is held while airborne/near-ground.
        if (!mbUsingAftertouch /* !gap711[3135] */
            && /* *(v2+63): the button-63 request */ true
            && !lbDidBounce
            && /* airborne (gap0[1432] && height<2.5) OR per-frame crash count gate */ false)
        {
            MS.mbBounceBoosting = true;
            lbDidBounce = true;
            MS.mbGoodImpact = true;
            MS.mbBounceWasGood = true;
            MS.mbBouncedThisFrame = true;
            MS.mbShouldBounceBoost = false;
        }
        // mbShouldBounceBoost |= (button-63 == 0); mbBounceBoostPending = button-63.
        // (the button source is the per-frame control byte; kept as the pending-latch update.)
        MS.mbBounceBoostPending = MS.mbBounceBoostPending;   // byte_82FB8489 = *(v2+63)

        if (lbDidBounce)
        {
            // assert(mbPlayerCarInShowtime) -- elided.
            f32 lfAirRamFactor = KF_BOUNCE_AIRRAM_FACTOR_NB;   // flt_82F2A2F0 (non-boosted default)
            if (MS.mbBounceBoosting)
            {
                if (MS.mbBounceWasGood)   // byte_82FB84B1
                {
                    // spin impulse along normalize(mBounceDirection + worldUp), scaled by lvAimSpin.
                    Vector3 lvDir = vpu::Normalize(vpu::Add(MS.mBounceDirection, GetWorldUpRow()));
                    AddWorldSpaceAngularImpulse(vpu::Mult(lvDir, 1.0f /* lvAimSpin lane */));
                    (void)lvAimSpin;
                    lfAirRamFactor = KF_BOUNCE_AIRRAM_FACTOR;   // flt_82F2A2F4
                }
            }
            if (lfAirRamFactor > 0.0f)
                AddAirRam(1u, lfAirRamFactor, /*flt_82FB9140*/ 0.0f,
                          Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f);
        }

        // launch-push timer expiry: fire the launch pop + extra spin, then set bounce-boost.
        if (MS.mfTimeUntilPush > 0.0f)
        {
            MS.mfTimeUntilPush -= lfTimeStep;
            if (MS.mfTimeUntilPush <= 0.0f)
            {
                if (MS.mbLaunchActive)   // byte_82FB84B2
                {
                    // pop: AddAirRam along normalize(mBounceDirection + worldUp@+0x20-region) + a spin.
                    Vector3 lvDir = vpu::Normalize(vpu::Add(MS.mBounceDirection, GetWorldUpRow()));
                    (void)lvDir;
                    AddAirRam(1u, KF_PUSH_AIRRAM_FACTOR, KF_PUSH_AIRRAM_ARG,
                              Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f);
                    AddWorldSpaceAngularImpulse(vpu::Mult(lvLaunchSpin, KF_PUSH_SPIN_SCALE));   // flt_82F2A2A8
                }
                MS.mbBounceBoosting = true;   // lbBounceBoosting = 1
                MS.mbShouldBounceBoost = false;
            }
        }

        CapShowtimeVelocities();
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateTargetAssist  @0x8261FF50  (asserts showtime; Hex-Rays degenerate ->
    //   reconstructed from the legible argmin loop in the pseudocode)
    //   Showtime auto-aim. Only while moving upward (vel.y > KF_..). Over the global candidate list
    //   (msNumTargets / target positions at +0x50 stride 16, ids at maTargetIds): score each candidate
    //   weight = (2 - alignmentDot) * (1/distance) with a x KF_TARGET_SCORE_GATE stickiness bonus when
    //   it is last frame's target (miCurrentTargetId). Pick the argmin; record it; lerp the aim
    //   direction toward it (different blend if mbJustBounced); when aligned (score gate passed),
    //   ComputeIdealVelocity and AddWorldSpaceForce toward the intercept.
    //   FLAG: the score gate / blend rodata are un-homed; the argmin + (2-dot)/dist scoring is exact.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::UpdateTargetAssist(const BrnPlayerDriverControls* lpControls)
    {
        (void)lpControls;
        // assert(mbPlayerCarInShowtime) -- elided.
        s32 liBest = -1;
        f32 lfBestWeight = 3.4028235e38f;   // FLT_MAX seed

        if (MS.miNumTargets <= 0)
            return;

        // only while moving upward: the asm gates on vel.y (this+2383 region) > KF_.. (flt_82F2A2F8).
        const Vector3 lvVel = GetLinearVelocity();
        if (!(lvVel.y > 0.0f))   // FLAG: the upward gate constant (flt_82F2A2F8) is un-homed -> use >0
            return;

        for (s32 liT = 0; liT < MS.miNumTargets; ++liT)
        {
            // target position lives in the +0x50 scratch (unk_82FB84D0) at stride 16, parallel to
            // maTargetIds[liT]. With that parallel array inside maReserved4C and not separately named,
            // the per-candidate position read is taken from the recorded aim slot. FLAG: the position
            // array is un-modelled by name; the scoring math below is faithful.
            const Vector3 lvToTarget = vpu::Subtract(MS.mBounceDirection, GetLinearVelocity());
            const f32 lfDistSq = vpu::MagnitudeSquared(lvToTarget);
            const f32 lfDist   = (lfDistSq > 0.0f) ? std::sqrt(lfDistSq) : 0.0f;
            if (lfDist <= KF_TARGET_SCORE_GATE)   // flt_82F2A328 gate (FLAGGED)
                continue;

            // weight = (2 - alignmentDot) * (1/dist)  -- here alignmentDot folded into the (2 - dist)
            // form the asm uses: v24 = (2.0 - v62) * dir.x ; with a stickiness x flt_82F2A32C when the
            // candidate id matches the current target.
            const f32 lfAlign = vpu::Dot(vpu::Normalize(lvToTarget), vpu::Normalize(lvVel));
            f32 lfWeight = (2.0f - lfAlign) * ((lfDist > 0.0f) ? (1.0f / lfDist) : 0.0f);
            // maTargetIds re-typed s32 -> EntityId (the type
            // GetTargetAssistParams writes into it); the console compares the raw dwords.
            if (static_cast<s32>(MS.maTargetIds[liT].muValue) == MS.miCurrentTargetId)
                lfWeight *= 1.0f;   // FLAG: stickiness bonus flt_82F2A32C (un-homed) -> identity

            if (lfWeight <= lfBestWeight)
            {
                lfBestWeight = lfWeight;
                liBest = liT;
            }
        }

        MS.miCurrentTargetId =
            (liBest >= 0) ? static_cast<s32>(MS.maTargetIds[liBest].muValue) : 0;  // dword_82FB8574

        if (liBest >= 0)
        {
            // when aligned, pull velocity toward the ballistic intercept.
            const f32 lfInputSpeed = std::sqrt(vpu::MagnitudeSquared(GetLinearVelocity()));   // 2D speed
            Vector3 lvIdeal;
            ComputeIdealVelocity(&lvIdeal, lfInputSpeed);
            // force toward (idealVel - currentVel.y component) scaled by an alignment term; the asm
            // gates the AddWorldSpaceForce on the alignment passing flt_82F2A324.
            const Vector3 lvDelta = vpu::Subtract(lvIdeal, GetLinearVelocity());
            AddWorldSpaceForce(vpu::Mult(lvDelta, KF_AFTERTOUCH_LAT));   // FLAG scale (flt_82F2A31C/320)
        }
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateAftertouch  @0x8262EBE8
    //   Camera-relative air-steer. Gated on the car being airborne (this+1808 in-air/crash) and the
    //   aftertouch-enable input. Normalises the camera matrix X and Z axes (asserting each has
    //   |.|^2 > 0), reads the stick deflection via GetAftertouchValues (+ optional SIXAXIS pitch when
    //   the downforce flag is set and |tilt| >= a threshold), then applies three force channels:
    //     (1) lateral world-space force along camera-X scaled by yaw (KF_AFTERTOUCH_LAT)
    //     (2) world-space roll angular impulse from camera axes (KF_AFTERTOUCH_ROLL)
    //     (3) local pitch impulses (KF_AFTERTOUCH_PITCH), with a +/-2.0 wheelie split by pitch sign.
    //   Showtime magnitudes differ from normal flight, with an extra IsBounceBoosting multiplier
    //   (unk_82FB8830). From showtime it chains UpdateTargetAssist (post lateral channel) then
    //   UpdateShowtimePhysics. FLAG: all magnitude rodata are un-homed placeholders; the camera-axis
    //   normalisation, the yaw/pitch channels and the showtime chaining are faithful.
    // ---------------------------------------------------------------------------------------
    // WIDENED 2026-08-09 (crash/shunt wave) to the 5-arg DWARF virtual form
    // (VehiclePhysics.h:1514) so it OVERRIDES the base slot +0x28 that UpdateCrashing
    // dispatches -- the committed 4-arg form was the dropped-VecFloat trap (the @0x8262EBE8
    // prologue saves v1, `vmr128 v121, v1` @0x8262EC08, and restores it before the showtime
    // chain calls). The dt lane is consumed only as that pass-through on this build; the
    // magnitudes this minimal slice models do not read it -> carried, documented, unused here.
    void RaceCarPhysics::UpdateAftertouch(const BrnPlayerDriverControls* lpControls,
                                          const Matrix44Affine* lpCameraMatrix,
                                          VecFloat lvfTimeStep,
                                          bool lbDoForceAdditiveAftertouch, bool lbUseSixaxis)
    {
        (void)lbUseSixaxis;
        (void)lvfTimeStep;   // v1 pass-through (see the widening banner)
        // gate: airborne/crash (this+1808). The minimal slice models this via the aftertouch latch.
        if (!mbUsingAftertouch /* *(this+1808): in-air/crash */)
            return;
        // RE-NAMED 2026-08-03. `lbz r11, 0x40(r25)` @0x8262EC28, must be ZERO to proceed --
        // decimal 64 IS 0x40, and 0x40 is mbIsOnStartLine, not the +0x44 driver type the invented
        // GetMode() accessor read. Aftertouch is disabled while the car sits on the start line.
        if (lpControls->mbIsOnStartLine)          // asm lbz +0x40, bne -> bail
            return;

        // virtual "can use aftertouch" query (vtbl+20). On this path: IsPlayerVehicleActuallyInShowtime.
        if (!IsPlayerVehicleActuallyInShowtime())
        {
            // (the non-showtime aftertouch path still runs; the asm only branches the magnitudes.)
        }

        // normalise the camera X and Z axes (assert |.|^2 > 0 -- elided).
        const Vector3 lvCameraX = vpu::Normalize(lpCameraMatrix->xAxis);
        const Vector3 lvCameraZ = vpu::Normalize(lpCameraMatrix->zAxis);

        f32 lfEnable = lpControls->GetAftertouchEnable();   // v29 = *(v8+32)
        const bool lbShowtime = mbPlayerCarInShowtime;      // *(v6+5132)

        if (!lbShowtime)
        {
            // normal flight: scale the enable by a boost-dependent factor (flt_82F2A2D0 / D4) gated on
            // the vehicle's vertical speed (+4192). FLAGGED scalars -> identity here.
            if (!IsBounceBoosting())
                lfEnable = /* flt_82F2A2D0 or D4 */ lfEnable;   // FLAG: un-homed factor
        }

        if (lbDoForceAdditiveAftertouch && lfEnable > 0.0f)
        {
            f32 lfYaw = 0.0f, lfPitch = 0.0f, lfScalar = 0.0f;
            // FORK RESOLVED 2026-08-06 (UpdateVehiclePhysics wave): the console leaf
            // @0x825B2E88 is the 4-arg reference form; THIS call site passes the bool as a
            // literal FALSE (`li r7, 0` @0x8262EE64, bl @0x8262EE78 -- raw image bytes).
            // The 3-pointer declaration is deleted (BrnVehicleDriverControls.h).
            lpControls->GetAftertouchValues(lfYaw, lfPitch, lfScalar, false);

            // optional SIXAXIS pitch contribution when the downforce flag is set and |tilt| >= thresh.
            if (mbUsingAftertouch /* *(v6+4944) downforce flag */)
            {
                const f32 lfTilt = lpControls->GetSixaxisTilt();   // *(v8+24)
                if (std::fabs(lfTilt) >= /*flt_82F2A314*/ 0.0f)
                {
                    const f32 lfSign = (lfTilt > 0.0f) ? 1.0f : ((lfTilt < 0.0f) ? -1.0f : 0.0f);
                    lfYaw += lfSign * /*flt_82F2A318*/ 0.0f;   // FLAG tilt gain
                    MS.mbSixaxisTiltApplied = true;            // byte_82FB848A = 1
                }
            }

            // (1) lateral world-space force along camera-X scaled by yaw * enable.
            Vector3 lvLateral = vpu::Mult(lvCameraX, lfYaw * lfEnable);
            if (lbShowtime && IsBounceBoosting())
                lvLateral = vpu::Mult(lvLateral, 1.0f);   // FLAG x unk_82FB8830 bounce-boost multiplier
            // (2) roll: a world-space angular impulse from the camera Z axis scaled by pitch * enable.
            Vector3 lvRoll = vpu::Mult(lvCameraZ, lfPitch * lfEnable);
            if (lbShowtime && IsBounceBoosting())
                lvRoll = vpu::Mult(lvRoll, 1.0f);   // FLAG x unk_82FB8830

            if (vpu::MagnitudeSquared(lvLateral) != 0.0f)
                AddWorldSpaceForce(vpu::Mult(lvLateral, KF_AFTERTOUCH_LAT));   // flt_82F2A304

            if (lbShowtime)
                UpdateTargetAssist(lpControls);

            // roll angular impulse (flt_82F2A2FC) -- gated on a small alignment test in the asm.
            if (vpu::MagnitudeSquared(lvRoll) != 0.0f)
                AddWorldSpaceAngularImpulse(vpu::Mult(lvRoll, KF_AFTERTOUCH_ROLL));

            // (3) local pitch impulses: a +/-2.0 wheelie split by pitch sign, scaled by KF_AFTERTOUCH_PITCH.
            const f32 lfWheelie = 4.0f;   // resolved inline literal (v74 = 4.0)
            Vector3 lvPitchImpulse = vpu::Mult(lvCameraX, lfYaw * lfEnable);
            if (lfYaw <= 0.0f)
                lvPitchImpulse = vpu::Subtract(Vector3{ 0.0f, lfWheelie, 0.0f, 0.0f }, lvPitchImpulse);
            else
                lvPitchImpulse = vpu::Add(Vector3{ 0.0f, lfWheelie, 0.0f, 0.0f }, lvPitchImpulse);
            // InputSpace tags recovered from UpdateAftertouch's own asm. The X360 issues this call
            // TWICE (@0x8262F49C and @0x8262F5D0) and both sites set `li r5,0 ; li r4,0` -- BOTH
            // vectors WORLD_SPACE. This is the only one of the five AddLocal* call sites in the
            // vehicle tree whose POSITION is world-space rather than body-space.
            // FLAG (position argument, pre-existing): the zero vector below is a STAND-IN and the
            // asm contradicts it -- v2 at the call is `vsubfp v2, v0, v12` (@0x8262F458), a live
            // difference of two vectors, not a literal zero. With a WORLD_SPACE tag a zero position
            // means "the world origin", so AddLocalImpulse would use r = -mTransform.wAxis (the car's
            // whole world position) as the lever arm. The tags here are console-exact; the position
            // is NOT, and must be recovered before this TU is mounted. (This TU is unmounted and also
            // still carries KF_DT = 0.0f, so nothing runs today.)
            AddLocalImpulse(vpu::Mult(lvPitchImpulse, KF_AFTERTOUCH_PITCH), rw::physics::WORLD_SPACE,
                            Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, rw::physics::WORLD_SPACE);
        }

        if (lbShowtime)
            UpdateShowtimePhysics(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },
                                  Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, lfEnable);
    }

    // never-called layout pin: makes the per-TU compile gate enforce the PlayerParameters singleton
    // offsets (the offsets the asm reads). If a member moves, this fails to compile.
    static void _AssertPlayerParamsLayout()
    {
        static_assert(offsetof(PlayerParameters, mbBounceBoosting)     == 0x00, "lbBounceBoosting");
        static_assert(offsetof(PlayerParameters, mbJustBounced)        == 0x01, "byte_82FB8481");
        static_assert(offsetof(PlayerParameters, mbBouncedThisFrame)   == 0x02, "byte_82FB8482");
        static_assert(offsetof(PlayerParameters, muBounceChainCount)   == 0x06, "word_82FB8486");
        static_assert(offsetof(PlayerParameters, mbBounceBoostPending) == 0x09, "byte_82FB8489");
        static_assert(offsetof(PlayerParameters, miOtherEntityId)      == 0x0C, "dword_82FB848C");
        static_assert(offsetof(PlayerParameters, mBounceDirection)     == 0x10, "+0x10 vector");
        static_assert(offsetof(PlayerParameters, mbDisableShowtime)    == 0x30, "byte_82FB84B0");
        static_assert(offsetof(PlayerParameters, mbLaunchActive)       == 0x32, "byte_82FB84B2");
        static_assert(offsetof(PlayerParameters, mfDeformationScale)   == 0x34, "flt_82FB84B4");
        static_assert(offsetof(PlayerParameters, mfDamageBudget)       == 0x38, "flt_82FB84B8");
        static_assert(offsetof(PlayerParameters, mfUncappedSpeedTimer) == 0x3C, "flt_82FB84BC");
        static_assert(offsetof(PlayerParameters, mfTimeUntilPush)      == 0x44, "flt_82FB84C4");
        static_assert(offsetof(PlayerParameters, mfPlayerCarStrength)  == 0x48, "lfShowtimePlayerCarStrength");
        static_assert(offsetof(PlayerParameters, maTargetPositions)    == 0x50, "unk_82FB84D0 (UpdateDrivers' GetTargetAssistParams arg 1)");
        static_assert(offsetof(PlayerParameters, maTargetIds)          == 0xD0, "dword_82FB8550");
        static_assert(offsetof(PlayerParameters, miNumTargets)         == 0xF0, "dword_82FB8570");
        static_assert(offsetof(PlayerParameters, miCurrentTargetId)    == 0xF4, "dword_82FB8574");
        static_assert(offsetof(PlayerParameters, mu8NumBounceSensors)  == 0x110, "byte_82FB8590");
    }
}
}
