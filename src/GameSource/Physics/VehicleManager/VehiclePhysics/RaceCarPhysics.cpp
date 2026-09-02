#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::{Dot, Add, Subtract, Mult, Normalize, MagnitudeSquared}
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Math/BrnMathUtils.h"    // BrnMath::Magnitude2D / MagnitudeSquared2D (XZ plane)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [showtime-probe] CgsDev::Log::gpDebugPrint
#include <cstdlib>   // [showtime-probe] getenv
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
        // [DIAG] BRN_STRENGTH_STAT_OVERRIDE=<0..10>. NOT IN THE X360 BINARY. Opt-in stand-in for
        // driving a strength-9/10 car: the console reads this byte from the vehicle list
        // (VehicleListEntry+155) and the harness can only spawn the junkyard car (strength 5).
        // OnChecked @0x8261E3F4 gates the checked car's crash on `> 8`. DELETE-WHEN the harness
        // can pick a car.
        {
            const char* lpcOverride = getenv("BRN_STRENGTH_STAT_OVERRIDE");
            if (lpcOverride != 0)
            {
                mu8StrengthStat = static_cast<u8>(atoi(lpcOverride));
            }
        }

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
    // car at a time. The DEFINITION lives in the slice TU RaceCarPhysics_ShowtimeBounce.cpp (along
    // with UpdateShowtimeBounceModifiers) since the PhysicsModule::Update leaves wave. STALE-BANNER
    // FIX 2026-08-24: this TU is MOUNTED (build_game_exe.bat mounts both it and the slice); the
    // "fold the definition back here when this TU mounts" note aged out -- the split stands because
    // both TUs link today and folding would be churn, not because of any mount gap.
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
    // ---- showtime/aftertouch constants, ALL image-attested (2026-08-24 showtime wave: every
    // value below was either already in rodata_bulk from the physics11 audit or freshly read
    // with idat off the ARTIST i64 copy; the .data splats cite their static-init writer). ----
    static const f32 KF_BOUNCE_SPIN_SCALE        = 10000.0f;     // flt_82F2A2CC (good-bounce camX spin)
    static const f32 KF_BOUNCE_AIRRAM_DECAY      = 0.2f;         // flt_82FB9140 <- flt_82F2A298 (init
                                                                 // @0x82C5CFA0; the static image byte
                                                                 // reads 0.0 -- literal-scan trap)
    static const f32 KF_BOUNCE_AIRRAM_FACTOR_DEV = 0.0f;         // flt_82FB7E2C: NO static-init writer
                                                                 // (dev-watch block, ships 0) -- the
                                                                 // not-boosting bounce AirRam factor
    static const f32 KF_IDEAL_SPEED_MIN          = 5.0f;         // flt_82F2A330 (was KF_IDEAL_T_BASE --
                                                                 // it is a SPEED CLAMP low bound, not a
                                                                 // "t base"; old name kept greppable)
    static const f32 KF_IDEAL_SPEED_MAX          = 20.0f;        // flt_82F2A334 (speed clamp high bound)
    static const f32 KF_AT_FORCE_YAW             = 12000.0f;     // flt_82F2A308 (negated at use)
    static const f32 KF_AT_FORCE_PITCH_UP        = 8000.0f;      // flt_82F2A30C (pitch >= 0; negated)
    static const f32 KF_AT_FORCE_PITCH_DOWN      = 20000.0f;     // flt_82F2A310 (pitch < 0; negated)
    static const f32 KF_AT_NONSHOWTIME_SCALE     = 1.5f;         // flt_82F2A2B4
    static const f32 KF_AT_ROLL_SCALAR           = -2000.0f;     // flt_82F2A304. RENAMED: this was
                                                                 // KF_AFTERTOUCH_LAT "lateral force
                                                                 // scale" -- MISBOUND. The asm consumes
                                                                 // it @0x8262F2E4 in the ANGULAR yaw-
                                                                 // impulse channel (x the aftertouch
                                                                 // scalar), never the lateral force.
    static const f32 KF_AT_LEVER_IMPULSE_YAW     = 1400.0f;      // flt_82F2A2FC (|yaw| lever impulse)
    static const f32 KF_AT_LEVER_IMPULSE_PITCH   = 1400.0f;      // flt_82F2A300 (|pitch| lever impulse)
    static const f32 KF_AT_LEVER_ARM             = 4.0f;         // flt_8208FA0C (+/-4 m lever offset)
    static const f32 KF_AT_BOOST_MULTIPLIER      = 2.5f;         // unk_82FB8830 <- flt_82005548 (init
                                                                 // @0x82C5CF78; bounce-boost force x)
    static const f32 KF_AT_ENABLE_AIR_FACTOR     = 0.4f;         // flt_82F2A2D0 (time-in-air > gate)
    static const f32 KF_AT_ENABLE_GROUND_FACTOR  = 0.1f;         // flt_82F2A2D4
    static const f32 KF_AT_TILT_GATE             = 0.2f;         // flt_82F2A314 (|SIXAXIS tilt| >=)
    static const f32 KF_AT_TILT_GAIN             = 0.2f;         // flt_82F2A318 (yaw += sign * gain)
    static const f32 KF_AIRTIME_GATE             = 0.5f;         // flt_82F2A2F8 (+0x1060.z compare, the
                                                                 // aftertouch/assist "really airborne")
    static const f32 KF_TARGET_SCORE_GATE        = 0.765999973f; // flt_82F2A328 (dot(aim,toTarget) >)
    static const f32 KF_TARGET_STICKINESS        = 0.5f;         // flt_82F2A32C (x weight when same id)
    static const f32 KF_TARGET_MIN_DISTANCE      = 2.0f;         // flt_82F2A324 (assist-force gate)
    static const f32 KF_ASSIST_FORCE_PER_METRE   = 4000.0f;      // flt_82F2A31C
    static const f32 KF_ASSIST_FORCE_CAP         = 14000.0f;     // flt_82F2A320 (fsel min clamp)
    static const f32 KF_ASSIST_SEED              = 0.015f;       // unk_82FB92B0 <- flt_82004C74 (@0x82C5CFB8)
    static const f32 KF_ASSIST_DECAY_PER_S       = 0.01f;        // unk_82FB9320 <- flt_82002138 (@0x82C5D008)
    static const f32 KF_ASSIST_FLOOR             = 0.001f;       // unk_82FB8AF0 <- flt_82013F90 (@0x82C5CFE0)
    static const f32 KF_FORCESET_HEIGHT          = 2.5f;         // flt_82005548 (force-set bounce height gate)
    static const f32 KF_PITCH_NEAR_GROUND        = 2.0f;         // flt_82001D9C (pitch-up impulse height gate)
    static const f32 KF_LAUNCH_MIN_SPEED2D       = 0.0099999998f;// inline 0.01 (launch direction fallback)
    static const f32 KF_GRAVITY                  = 9.8100004f;   // flt_8208F83C (ballistic arc)
    // The world up vector both showtime spin/AirRam sites read. TWO console homes, one value:
    // unk_82181510 (.rdata, {0,1,0,0} raw-byte read) and unk_82FB9050 (.data, init @0x82C5C4B0
    // building {0.0, 1.0, 0.0, 0} from flt_82001CC0/flt_82001C98).
    static const Vector3 KV_WORLD_UP             = { 0.0f, 1.0f, 0.0f, 0.0f };

    // ---------------------------------------------------------------------------------------
    // PlayerParameters::Reset  @0x825B89B8 -- store-for-store from the asm (offsets confirmed).
    //   Zeroes the bounce report + latch block, the +0x10 direction vector, the +0x30 launch block
    //   and the +0x110 sensor count; SEEDS the +0x100 assist envelope = splat(flt_82002138 = 0.01)
    //   and mfDamageBudget(+0x38)=flt_82F2A2C8; sets mbBounceBoostPending(+0x09)=1 and
    //   miCurrentTargetId(+0xF4)=-1.
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

        // THE DROPPED +0x100 STORE, RESTORED (2026-08-24 showtime wave). The console splats
        // flt_82002138 = 0.01 into the assist envelope; the committed body left it to the {}
        // zero-init with a comment calling it "target/sensor scratch". It is neither scratch nor
        // reader-less: UpdateTargetAssist maintains it every frame and multiplies its .y into
        // the assist force's vertical correction. A 0.0 seed is NOT the identity here -- it
        // kills the vertical intercept pull until the first bounce re-seeds the envelope.
        mAssistStrength = Vector3{ 0.01f, 0.01f, 0.01f, 0.01f };   // +0x100 splat(flt_82002138)
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
    // =======================================================================================
    // =======================================================================================
    // [showtime-watch] -- NOT an X360 function. A per-frame WITNESS on the REAL showtime state.
    //
    // ⛔⛔ IT REPLACES `RunShowtimeBringUpProbe`, AND THAT PROBE'S OWN DELETE-WHEN IS WHY.
    // The block that stood here was armed by BRN_SHOWTIME_TEST and, once the car passed 25 mph,
    // CALLED `SetPlayerVehicleInShowtime(true, 5.0, 50.0)` and `SetCrashing(true)` itself, then
    // drove `UpdateAftertouch` by hand every frame. Its banner said exactly why:
    //     "NOTHING in this build can reach it through gameplay: the sole console caller of
    //      VehicleManager::SetPlayerCarToShowtimeMode (the game-mode/action chain) is not
    //      reconstructed ... ⛔ DELETE-WHEN: the game-mode chain lands and a real showtime run
    //      replaces this witness."
    // ⭐⭐⭐ THAT CONDITION IS MET AS OF 2026-08-27. `PhysicsModule::HandleGameActions @0x825A72F0`
    // is real (BrnPhysicsModuleGameActions.cpp) and `GameStateModule::StartCrashMode @0x8236B580`
    // is real (GameStateModule_Showtime.cpp); a run that holds both bumpers now reaches
    // SetPlayerCarToShowtimeMode through the console's own chain, and the game's own assert
    // callstack printed it:
    //     RaceCarPhysics::SetPlayerVehicleInShowtime <- VehicleManager::SetPlayerCarToShowtimeMode
    //       <- PhysicsModule::HandleGameActions <- PhysicsModule::Update
    // So the FABRICATED ENTRY IS DELETED. Keeping it would leave a second, invented road into
    // showtime alongside the real one -- and the two would diverge silently.
    //
    // WHAT SURVIVES is the half that was never fabricated: the state line. It is now gated on the
    // REAL `mbPlayerCarInShowtime` instead of on an injected one, so it can only print when the
    // console's own chain has put the car in showtime. It fires on the transition and then every
    // 30 frames while showtime lasts, and it carries the BOUNCE counters as well as the launch
    // latches -- because "did the P6 bounce chain execute" is a question about
    // mbJustBounced / muBounceChainCount, not about whether showtime was entered.
    // Opt-in via BRN_SHOWTIME_WATCH so a default run is untouched.
    //
    // ⭐⭐⭐ HOW TO READ THE LINE IT PRINTS, and what its FIRST measurement already said.
    // On 2026-08-27 a real showtime run produced 164 of these. The car entered showtime, took the
    // launch impulse, flew a genuine arc (velY +5.2 -> 0.1 -> -5.1 with hasAir going 1 -> 0) and
    // tumbled (angMag to 5.6). And across every one of those lines:
    //     justBounced=0  bouncedThisFrame=0  chain=0  boosting=0  usingAftertouch=0
    //     pushT=0.400000
    // ⚠️ `pushT` IS THE TELL, AND IT IS THE ONE FIELD THAT CANNOT LIE HERE. mfTimeUntilPush is
    // decremented by the timestep on exactly ONE line in the whole tree -- UpdateShowtimePhysics,
    // ~:1228 below -- and SetPlayerVehicleInShowtime had just seeded it to 0.4. A timer that is
    // seeded and then never ticks proves its owning body never ran, which is a far stronger claim
    // than "the bounce flags are zero" (those could be zero because nothing hit anything).
    // The gate is `VehiclePhysics::UpdateCrashing`'s `if (lbPlayerAftertouchForceAdditive)` --
    // console-faithful, asm 0x82638E30 `beq` -- fed by VehicleManager::mbAftertouchIsForceAdditive,
    // whose only writer is game action 42, which nothing in the tree posts. The full five-link
    // diagnosis is written out at that arm in BrnPhysicsModuleGameActions.cpp.
    // ⇒ ENTERING showtime and RUNNING showtime are two different milestones. This witness is what
    // keeps the first from being reported as the second.
    // =======================================================================================
    void RaceCarPhysics::Update(VecFloat lvfSimTimeStep, VecFloat lvfRealTimeStep,
                                const rw::math::vpu::Matrix44Affine* lpCameraMatrix,
                                const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                                bool lbPlayerAftertouchForceAdditive, bool lbShowtimeAllowed,
                                CgsNumeric::Random& lrRandom)
    {
        {
            static const bool sbWatch = (std::getenv("BRN_SHOWTIME_WATCH") != 0);
            static bool       sbWasInShowtime = false;
            static s32        siFrameMod      = 0;

            if (sbWatch && CgsDev::Log::gpDebugPrint != 0)
            {
                const bool lbIn = mbPlayerCarInShowtime;
                if (lbIn != sbWasInShowtime)
                {
                    sbWasInShowtime = lbIn;
                    siFrameMod = 0;
                    *CgsDev::Log::gpDebugPrint
                        << "[showtime-watch] mbPlayerCarInShowtime -> " << (lbIn ? 1 : 0)
                        << " (reached through the console chain, not injected)\n";
                }
                // ⭐⭐⭐ [bounce wave] ASK WHAT THIS PROBE CANNOT SEE. The 30-frame period is
                // HALF A SECOND at 60 fps, and mfTimeUntilPush is seeded to 0.4 -- so the push
                // window is SHORTER THAN ONE SAMPLE. At period 30 this witness can only ever
                // print the seed and then a zero, and it is structurally incapable of showing the
                // decrement in between. That is exactly how a body that ran ONCE with a huge
                // timestep and a body that ticked properly at 60 Hz would look identical here.
                // So while the timer is live, sample EVERY frame. Costs a handful of lines (the
                // window is ~24 frames) and turns "pushT changed" into "pushT decremented, at
                // this rate, over this many frames" -- a VALUE instead of a fact.
                const bool lbPushWindow = (msPlayerParams.mfTimeUntilPush > 0.0f);
                if (lbIn && (lbPushWindow || (siFrameMod++ % 30) == 0))
                {
                    const Vector3 lvVel = GetLinearVelocity();
                    const Vector3 lvAng = GetAngularVelocity();
                    *CgsDev::Log::gpDebugPrint
                        << "[showtime-watch] crashing=" << (IsCrashing() ? 1 : 0)
                        << " hasAir=" << (mbHasAir ? 1 : 0)
                        << " usingAftertouch=" << (mbUsingAftertouch ? 1 : 0)
                        << " | disable=" << (msPlayerParams.mbDisableShowtime ? 1 : 0)
                        << " launch=" << (msPlayerParams.mbLaunchActive ? 1 : 0)
                        << " pushT=" << msPlayerParams.mfTimeUntilPush
                        // THE BOUNCE CHAIN, which is the question this witness exists to answer:
                        << " | justBounced=" << (msPlayerParams.mbJustBounced ? 1 : 0)
                        << " bouncedThisFrame=" << (msPlayerParams.mbBouncedThisFrame ? 1 : 0)
                        << " chain=" << static_cast<s32>(msPlayerParams.muBounceChainCount)
                        << " boosting=" << (msPlayerParams.mbBounceBoosting ? 1 : 0)
                        << " boostPending=" << (msPlayerParams.mbBounceBoostPending ? 1 : 0)
                        << " | speed=" << std::sqrt(vpu::MagnitudeSquared(lvVel))
                        << " velY=" << lvVel.y
                        << " angMag=" << std::sqrt(vpu::MagnitudeSquared(lvAng)) << "\n";
                }
            }
        }
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
    //   Stash the aim direction (one VMX register) into the singleton @ +0x20.
    // FIXED 2026-08-24 (showtime wave): the committed store went to +0x10 (mBounceDirection),
    //   clobbering SetJustBounced's report vector while the real aim slot never received a
    //   write. The asm is unambiguous: `li r10, 0x20 ; stvx128 v1, r11, r10` -- re-read by two
    //   audit agents and again this wave. The +0x20 slot is now the named mAimDirection, and
    //   its console reader (the UpdateShowtimePhysics launch pop @0x82600030) reads it below.
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::SetShowtimeAimDirection(const Vector3& lvAimDirection)
    {
        MS.mAimDirection = lvAimDirection;   // stvx128 v1, (lbBounceBoosting + 0x20)
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
    // REWRITTEN 2026-08-24 (showtime wave) from the full raw asm (0x82600558..0x8260077C).
    //   The committed form was wrong in all three terms and in the signature:
    //     * a2/r4 is THIS (the caller passes `mr r4, r28` = the car), not a "target body";
    //       v1 is the chosen TARGET POSITION -- the old body read MS.mBounceDirection instead;
    //     * the divisor is clamp(speed2D, 5.0, 20.0) (two scalar fsels @0x826005C0/0x826005E0:
    //       max(speed, 5.0) then min(., 20.0)) -- not "5.0 - speed";
    //     * horizontal = unit2D * clampedSpeed (`vmulfp128 v0, v0, v9` @0x82600750, v9 =
    //       splat(the CLAMPED SPEED stored to var_40 @0x82600660)) -- not dist/(2t);
    //     * vertical = (2*delta.y + 9.81*t^2) * (1/(2t)) (@0x82600744/54: vmaddfp
    //       v13 = delta.y * splat(2.0) + splat(t^2 * 9.81), then * splat(1/(2t))) -- the exact
    //       ballistic solution vy = h/t + g*t/2 with the ORIGINAL vertical offset, which the old
    //       body dropped entirely.
    //   The < 1.0 m arm returns [a2+0x50] -- and since a2 == this, that is the car's own
    //   velocity: the previous FLAG on that arm is retired as accidentally-correct.
    // ---------------------------------------------------------------------------------------
    Vector3* RaceCarPhysics::ComputeIdealVelocity(Vector3* lpResult, Vector3 lvTargetPosition,
                                                  f32 lfSpeed2D) const
    {
        // delta = targetPos - position (+0x40), kept unflattened for the vertical term.
        const Vector3 lvDelta = vpu::Subtract(lvTargetPosition, mTransform.Pos());

        // 2D (XZ) distance -- the asm calls BrnMath::Flatten then sums the two packed lanes'
        // squares; MagnitudeSquared2D is that same x*x + z*z.
        const f32 lfDistSq = BrnMath::MagnitudeSquared2D(lvDelta);
        const f32 lfDist   = (lfDistSq > 0.0f) ? std::sqrt(lfDistSq) : 0.0f;   // vrsqrte + guard

        // clamp(speed2D, 5.0, 20.0) -- the two fsels (flt_82F2A330 / flt_82F2A334).
        f32 lfClampedSpeed = lfSpeed2D;
        if (lfClampedSpeed < KF_IDEAL_SPEED_MIN) lfClampedSpeed = KF_IDEAL_SPEED_MIN;
        if (lfClampedSpeed > KF_IDEAL_SPEED_MAX) lfClampedSpeed = KF_IDEAL_SPEED_MAX;

        if (lfDist >= 1.0f)   // flt_82001C98 compare @0x8260061C
        {
            const f32 lfT = lfDist / lfClampedSpeed;   // fdivs @0x82600650

            // horizontal: unit2D(delta) * clampedSpeed. The asm zeroes the y lane (vrlimi mask 4
            // of splat(0)) and normalizes the 3-lane vector -- identical to a 2D normalize.
            Vector3 lvFlat = lvDelta;
            lvFlat.y = 0.0f;
            const f32 lfFlatMagSq = vpu::MagnitudeSquared(lvFlat);
            Vector3 lvUnit2D = (lfFlatMagSq > 0.0f)
                             ? vpu::Mult(lvFlat, 1.0f / std::sqrt(lfFlatMagSq))
                             : Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };

            Vector3 lvIdeal = vpu::Mult(lvUnit2D, lfClampedSpeed);

            // vertical: (delta.y * 2.0 + 9.81 * t^2) / (2t)  == h/t + g*t/2.
            lvIdeal.y = (lvDelta.y * 2.0f + KF_GRAVITY * lfT * lfT) / (lfT * 2.0f);

            *lpResult = lvIdeal;
        }
        else
        {
            // under 1.0 m of horizontal distance: the body's own velocity (a2 == this, +0x50).
            *lpResult = GetLinearVelocity();
        }
        return lpResult;
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateShowtimeBounceModifiers @0x825D7940 -- lives in the slice TU
    // RaceCarPhysics_ShowtimeBounce.cpp (moved 2026-08-06, PhysicsModule::Update leaves wave;
    // the "fold back on mount" note aged out -- both TUs are mounted). REWRITTEN there
    // 2026-08-24 (showtime wave): the elided per-sensor scatter is a real loop now.
    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::SetPlayerVehicleInShowtime  @0x826000F8
    //   On entry (lbInShowtime): reset the singleton; if the car is not already in showtime and not
    //   airborne/crashing, give it a DOUBLE-IMPULSE launch -- overwrite mLinearVelocity with a
    //   20 m/s push along the FLATTENED heading AND fire an AddAirRam (input-space 5130/3082) --
    //   then seed mfTimeUntilPush + mark mbLaunchActive. Always store the strength + scale the
    //   damage budget.
    // REWRITTEN 2026-08-24 (showtime wave) against the full asm (dossier @0x826000F8):
    //   * the launch-skip gate is (mbHasAir || IsCrashing()) -- `_R30[4944]` == +0x1350 mbHasAir,
    //     `_R30[1808]` == +0x710 mbIsCrashing (audit F2). The committed body read mbUsingAftertouch
    //     (+0x140D), a different member this class only writes at the bottom of Update.
    //   * the push direction is the velocity FLATTENED (y forced 0, vrlimi mask 4 splat(0)); when
    //     its 2D magnitude is <= 0.01 the direction source falls back to the transform's z row
    //     (`lvx128 v0, r30, 0x30` = mTransform.At(), the car's forward). After the normalize the
    //     .y lane is overwritten with flt_82FB7E28 -- a dev-watch slot with NO static-init writer,
    //     shipping 0.0 (a faithful 0, per the cluster-D read).
    //   * the rising/falling test for the AirRam input-space bits reads the ANGULAR velocity's .y
    //     (`lvx128 v0, r30, 0x60 ; vspltw v0,v0,1` -- +0x60 is mAngularVelocity), not the linear.
    //   All four IsValid asserts + the damage-limit range assert are the console's own.
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
                if (mbHasAir || IsCrashing())   // +0x1350 || +0x710 (asm; audit F2)
                {
                    lfTimeUntilPush = 0.001f;   // already airborne/crashing -> tiny delay, no relaunch
                    // [showtime] one-shot mode-entry diagnostic (branch witness; entry is a
                    // once-per-mode-change event, so this cannot spam).
                    if (CgsDev::Log::gpDebugPrint != 0)
                        *CgsDev::Log::gpDebugPrint
                            << "[showtime] enter: airborne/crash arm (hasAir=" << (mbHasAir ? 1 : 0)
                            << " crashing=" << (IsCrashing() ? 1 : 0) << ") pushT=0.001\n";
                }
                else
                {
                    CGS_ASSERT(vpu::IsValid(GetLinearVelocity()),
                               "IsValid(GetLinearVelocity())");             // :1298

                    // launch direction: the flattened heading, transform-forward fallback at a
                    // standstill, unit .y forced to flt_82FB7E28 (== 0.0, dev-watch slot).
                    Vector3 lvDir = GetLinearVelocity();
                    lvDir.y = 0.0f;                                    // vrlimi mask 4, splat(0)
                    const f32 lfMag2DSq = vpu::MagnitudeSquared(lvDir);
                    const f32 lfMag2D   = (lfMag2DSq > 0.0f) ? std::sqrt(lfMag2DSq) : 0.0f;
                    if (KF_LAUNCH_MIN_SPEED2D >= lfMag2D)              // vcmpgefp. 0.01 >= |v2D|
                        lvDir = mTransform.At();                       // lvx128 [this+0x30]

                    Vector3 lvPush = vpu::Normalize(lvDir);            // vrsqrte + 2 NR steps
                    lvPush.y = KF_BOUNCE_AIRRAM_FACTOR_DEV /* flt_82FB7E28 == 0.0 */;
                    CGS_ASSERT(vpu::IsValid(lvPush), "IsValid(lPush)"); // :1310

                    lvPush = vpu::Mult(lvPush, KF_LAUNCH_PUSH_SPEED);  // * flt_82F2A2A0 (20.0)
                    SetLinearVelocity(lvPush);   // stvx128 -> +0x50 (overwrite: the "double impulse")
                    CGS_ASSERT(vpu::IsValid(GetLinearVelocity()),
                               "IsValid(GetLinearVelocity())");         // :1314

                    // AddAirRam: input-space 5130 when the ANGULAR velocity's .y is >= 0, 3082
                    // otherwise. 6-arg DWARF form: flags, factor, decay, customImpulse,
                    // customPosition, timerTillFire.
                    const bool lbRising = !(GetAngularVelocity().y < 0.0f);   // +0x60 lane .y
                    AddAirRam(lbRising ? 5130u : 3082u, KF_LAUNCH_AIRRAM_FACTOR, KF_LAUNCH_AIRRAM_ARG,
                              Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f);

                    lfTimeUntilPush = KF_TIMEUNTILPUSH_DELAY;   // flt_82F2A2A4 (0.4, image-read)
                    MS.mbLaunchActive = true;                   // byte_82FB84B2 = 1

                    // [showtime] one-shot mode-entry diagnostic (branch witness).
                    if (CgsDev::Log::gpDebugPrint != 0)
                        *CgsDev::Log::gpDebugPrint
                            << "[showtime] enter: LAUNCH arm, push vel=(" << lvPush.x << ", "
                            << lvPush.y << ", " << lvPush.z << ") rising=" << (lbRising ? 1 : 0)
                            << " pushT=" << KF_TIMEUNTILPUSH_DELAY << "\n";
                }
                MS.mfTimeUntilPush = lfTimeUntilPush;   // flt_82FB84C4 = v68
            }
        }

        mbPlayerCarInShowtime = lbInShowtime;        // _R30[5132] = v7
        MS.mbDisableShowtime  = false;               // byte_82FB84B0 = 0
        MS.mfPlayerCarStrength = lfPlayerCarStrength; // lfShowtimePlayerCarStrength = param_1

        CGS_ASSERT(lfPlayerCarDamageLimit > 0.0f && lfPlayerCarDamageLimit < 100.0f,
                   "lfPlayerCarDamageLimit > 0.0f && lfPlayerCarDamageLimit < 100.0f");   // :1356
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
    // REWRITTEN 2026-08-24 (showtime wave) from the full raw asm. What moved (audit F3, all
    // re-verified instruction-by-instruction this wave):
    //   * SIGNATURE: the DWARF 6-arg form. The old 3-arg body received the ENABLE input in its
    //     "lfTimeStep" seat and tested `dt >= 0` as "bouncing" -- always true, so
    //     mbDisableShowtime latched every frame and showtime self-disabled. The console's
    //     "bouncing" is (0 >= enable) || (3.0 > |velocity|): input released, or too slow.
    //   * the push timer decrements by the TIMESTEP (v3/v124 @0x8260000C), not by the input.
    //   * the pending latch was a self-assignment no-op; the console reads controls+0x3F
    //     (mbBoostBounce): mbShouldBounceBoost |= !button (cntlzw bit trick @0x8262FE7C-90),
    //     mbBounceBoostPending = button.
    //   * the force-set block was `&& false`; the console gate (0x825FFE00-58) is
    //     !mbHasAir && button && !didBounce && ((aboveGroundValid && verticalDistance < 2.5)
    //     || miNumCollisions > mi8NumWorldCollisions).
    //   * the good-bounce arm ALSO floors the velocity's .y at (3.0 + 1.0) -- the fsel max
    //     @0x825FFF70 + vrlimi/stvx write-back to +0x50 the old body dropped entirely -- and the
    //     spin impulse is cameraX * splat(flt_82F2A2CC = 10000), not "dir * 1.0".
    //   * the bounce AirRam direction is normalize(cameraZ + worldUp) when boosting-and-good
    //     (v127 default = unk_82181510 = worldUp otherwise), its decay arg is flt_82FB9140 --
    //     which the STATIC image reads as 0.0 but the init writer @0x82C5CFA0 sets to 0.2 -- and
    //     the not-boosting factor is flt_82FB7E2C (dev-watch, ships 0.0 => no ram), NOT the
    //     0.001 the old body used (0.001/0.004 are the boosting-not-good / boosting-good pair).
    //   * the launch pop reads the +0x20 AIM slot (`li r11,0x20 ; lvx128 v13,r31,r11`
    //     @0x82600030-54): dir = normalize(worldUp + mAimDirection); its spin is
    //     cameraX * splat(500).
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::UpdateShowtimePhysics(const BrnPlayerDriverControls* lpControls,
                                               const Vector3& lvCameraX, const Vector3& lvCameraZ,
                                               VecFloat lvfTimeStep, VecFloat lvfEnable,
                                               bool lbUnused)
    {
        (void)lbUnused;   // r5: saved by the prologue, never read (register census @0x825FFBD8)
        CGS_ASSERT(IsPlayerVehicleActuallyInShowtime(),
                   "IsPlayerVehicleActuallyInShowtime()");   // :754 (vcall vtbl+0x14)

        // "bouncing" = the aftertouch enable released (0 >= enable) OR speed below 3.0 m/s.
        bool lbBouncing = false;
        if (0.0f >= lvfEnable.x)   // vcmpgefp128. splat(0) >= v4 @0x825FFC7C
        {
            lbBouncing = true;
        }
        else
        {
            const f32 lfSpeed = std::sqrt(vpu::MagnitudeSquared(GetLinearVelocity()));   // +0x50 |v|
            if (KF_BOUNCE_VELOCITY_SCALE > lfSpeed)   // flt_82F2A2EC (3.0) > |v|
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

        // force-set bounce-boost: bounce button held, not airborne-latched, no bounce yet this
        // frame, and either close to the ground or in fresh car-car contact (asm 0x825FFE00-74).
        if (!mbHasAir                                      // lbz +0x1350 == 0
            && lpControls->GetButton63()                   // lbz controls+0x3F (mbBoostBounce)
            && !lbDidBounce
            && ((mAboveGroundTestResult.mbValid            // lbz +0x598
                 && mAboveGroundTestResult.mfVerticalDistance < KF_FORCESET_HEIGHT)  // +0x590 < 2.5
                || miNumCollisions > static_cast<s32>(mi8NumWorldCollisions)))       // +0x1354 > sext(+0x1353)
        {
            MS.mbBounceBoosting = true;
            lbDidBounce = true;
            MS.mbGoodImpact = true;
            MS.mbBounceWasGood = true;
            MS.mbBouncedThisFrame = true;
            MS.mbShouldBounceBoost = false;
        }

        // the pending latch (asm 0x825FFE74-98): should-boost sticks while the button is UP;
        // pending mirrors the button.
        MS.mbShouldBounceBoost = MS.mbShouldBounceBoost || !lpControls->GetButton63();
        MS.mbBounceBoostPending = lpControls->GetButton63();   // byte_82FB8489 = *(controls+0x3F)

        if (lbDidBounce)
        {
            Vector3 lvAirRamDir = KV_WORLD_UP;   // v127 default = unk_82181510
            f32 lfAirRamFactor  = KF_BOUNCE_AIRRAM_FACTOR_DEV;   // flt_82FB7E2C (0.0 -> no ram)
            CGS_ASSERT(mbPlayerCarInShowtime, "mbPlayerCarInShowtime");   // :1499

            if (MS.mbBounceBoosting)   // lbBounceBoosting
            {
                if (MS.mbBounceWasGood)   // byte_82FB84B1 (set only by the force-set arm)
                {
                    lfAirRamFactor = KF_BOUNCE_AIRRAM_FACTOR;   // flt_82F2A2F4 (0.004)

                    // floor the vertical speed at 3.0 + 1.0 (fsel max @0x825FFF70, write-back
                    // to +0x50 with vrlimi mask 4).
                    Vector3 lvVel = GetLinearVelocity();
                    const f32 lfFloor = KF_BOUNCE_VELOCITY_SCALE + 1.0f;   // flt_82F2A2EC + flt_82001C98
                    if (lvVel.y < lfFloor)
                        lvVel.y = lfFloor;
                    SetLinearVelocity(lvVel);

                    // spin impulse: cameraX * splat(flt_82F2A2CC = 10000).
                    AddWorldSpaceAngularImpulse(vpu::Mult(lvCameraX, KF_BOUNCE_SPIN_SCALE));

                    // the boost AirRam fires along normalize(cameraZ + worldUp).
                    lvAirRamDir = vpu::Normalize(vpu::Add(lvCameraZ, KV_WORLD_UP));
                }
                else
                {
                    lfAirRamFactor = KF_BOUNCE_AIRRAM_FACTOR_NB;   // flt_82F2A2F0 (0.001)
                }
            }
            if (lfAirRamFactor > 0.0f)
                AddAirRam(1u, lfAirRamFactor, KF_BOUNCE_AIRRAM_DECAY /* flt_82FB9140 = 0.2 */,
                          lvAirRamDir, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f);
        }

        // launch-push timer expiry: fire the launch pop + extra spin, then set bounce-boost.
        if (MS.mfTimeUntilPush > 0.0f)
        {
            MS.mfTimeUntilPush -= lvfTimeStep.x;   // v3/v124 -- the TIMESTEP (asm 0x8260000C-18)
            if (MS.mfTimeUntilPush <= 0.0f)
            {
                if (MS.mbLaunchActive)   // byte_82FB84B2
                {
                    // pop: AddAirRam along normalize(worldUp + mAimDirection) -- the +0x20 slot
                    // SetShowtimeAimDirection stores (asm 0x82600030-94).
                    Vector3 lvDir = vpu::Normalize(vpu::Add(KV_WORLD_UP, MS.mAimDirection));
                    AddAirRam(1u, KF_PUSH_AIRRAM_FACTOR, KF_PUSH_AIRRAM_ARG,
                              lvDir, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, 0.0f);
                    // pop spin: cameraX * splat(flt_82F2A2A8 = 500).
                    AddWorldSpaceAngularImpulse(vpu::Mult(lvCameraX, KF_PUSH_SPIN_SCALE));

                    // [showtime] one-shot launch-pop diagnostic (fires once per launch window).
                    if (CgsDev::Log::gpDebugPrint != 0)
                        *CgsDev::Log::gpDebugPrint
                            << "[showtime] launch pop: airram dir=(" << lvDir.x << ", " << lvDir.y
                            << ", " << lvDir.z << ") aim=(" << MS.mAimDirection.x << ", "
                            << MS.mAimDirection.y << ", " << MS.mAimDirection.z << ")\n";
                }
                MS.mbBounceBoosting = true;   // lbBounceBoosting = 1
                MS.mbShouldBounceBoost = false;
            }
        }

        CapShowtimeVelocities();
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateTargetAssist  @0x8261FF50
    // REWRITTEN 2026-08-24 (showtime wave) from the full raw asm (audit F4 confirmed, and the
    // old body's shape was wrong well beyond the flagged constants):
    //   * signature: the DWARF 4-arg form (controls, dt, enable, aimDirection). The candidate
    //     loop scores dot(AIM DIRECTION, unit(targetPos - carPos)) -- the old body subtracted
    //     mBounceDirection from the VELOCITY for every candidate.
    //   * the entry gate is time-in-air (+0x1060.z, `vspltw v0,v0,2` @0x8261FFxx) > 0.5
    //     (flt_82F2A2F8) -- not "vel.y > 0".
    //   * flt_82F2A328 (0.766) gates the DOT, not the distance; the weight is
    //     (2 - dot) * DISTANCE (argmin prefers aligned-and-CLOSE; the old 1/dist inverted it);
    //     the stickiness for last frame's target MULTIPLIES by 0.5 (flt_82F2A32C), halving w.
    //   * the mAssistStrength envelope (+0x100) is maintained here (seed 0.015 on a fresh
    //     bounce, else decay 0.01/s to a 0.001 floor -- all three splats' init writers decoded)
    //     and its .y scales the vertical intercept correction.
    //   * the assist force: direction = unit(toTarget) with .y replaced by
    //     mAssistStrength.y * (ideal.y - vel.y); magnitude = min(dist * 4000, 14000)
    //     (flt_82F2A31C / flt_82F2A320, the fsel @0x826202C4); fired only past 2.0 m
    //     (flt_82F2A324 -- the constant the old comment called an "alignment" gate); then the
    //     queued air rams are cleared (`std r31(0), 0x1158` @0x826202F0 == mUsedAirRams).
    //   * the debug arm (mbDebugShowTargetAssist +0x1454 -> DebugRender::DrawCross per candidate,
    //     10.0/red for the chosen target, 8.0/green otherwise) is dev-render-only and stays
    //     elided; the byte is modelled (RaceCarPhysics.h:+0x1454).
    // ---------------------------------------------------------------------------------------
    void RaceCarPhysics::UpdateTargetAssist(const BrnPlayerDriverControls* lpControls,
                                            VecFloat lvfTimeStep, VecFloat lvfEnable,
                                            Vector3 lvAimDirection)
    {
        (void)lpControls;   // r4: in the DWARF signature, never read by the body
        (void)lvfEnable;    // v2: in the DWARF signature, never read by the body
        CGS_ASSERT(mbPlayerCarInShowtime, "mbPlayerCarInShowtime");   // :1036

        s32 liBest = -1;
        f32 lfBestWeight = 3.4028235e38f;   // FLT_MAX seed

        if (MS.miNumTargets <= 0)   // dword_82FB8570
            return;

        // gate: really airborne -- time-without-traction (+0x1060.z) above 0.5 s.
        if (!(mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z > KF_AIRTIME_GATE))
            return;

        const Vector3 lvPosition = mTransform.Pos();   // lvx128 [this+0x40]

        for (s32 liT = 0; liT < MS.miNumTargets; ++liT)
        {
            const Vector3 lvToTarget = vpu::Subtract(MS.maTargetPositions[liT], lvPosition);
            const f32 lfDistSq = vpu::MagnitudeSquared(lvToTarget);
            const f32 lfDist   = (lfDistSq > 0.0f) ? std::sqrt(lfDistSq) : 0.0f;
            const Vector3 lvUnit = (lfDist > 0.0f)
                                 ? vpu::Mult(lvToTarget, 1.0f / lfDist)
                                 : Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // vsel zero guard

            const f32 lfDot = vpu::Dot(lvAimDirection, lvUnit);   // vmsum3fp128 v127, v11
            if (lfDot > KF_TARGET_SCORE_GATE)   // flt_82F2A328 (0.766) -- the DOT gate
            {
                f32 lfWeight = (2.0f - lfDot) * lfDist;   // aligned-and-close argmin
                // the console compares the raw dwords (maTargetIds is EntityId-typed here).
                if (static_cast<s32>(MS.maTargetIds[liT].muValue) == MS.miCurrentTargetId)
                    lfWeight *= KF_TARGET_STICKINESS;     // x 0.5 (flt_82F2A32C)

                if (lfWeight <= lfBestWeight)
                {
                    lfBestWeight = lfWeight;
                    liBest = liT;
                }
            }
        }

        MS.miCurrentTargetId =
            (liBest >= 0) ? static_cast<s32>(MS.maTargetIds[liBest].muValue) : 0;  // dword_82FB8574

        // maintain the assist-strength envelope (+0x100): fresh seed on a bounce, else decay
        // toward the floor (asm 0x82620144-98; all three vectors' static-init writers decoded).
        if (MS.mbJustBounced)   // byte_82FB8481
        {
            MS.mAssistStrength =
                Vector3{ KF_ASSIST_SEED, KF_ASSIST_SEED, KF_ASSIST_SEED, KF_ASSIST_SEED };
        }
        else
        {
            const f32 lfDecay = lvfTimeStep.x * KF_ASSIST_DECAY_PER_S;
            Vector3 lvEnv = MS.mAssistStrength;
            lvEnv.x = std::max(lvEnv.x - lfDecay, KF_ASSIST_FLOOR);   // vmaxfp lane-wise
            lvEnv.y = std::max(lvEnv.y - lfDecay, KF_ASSIST_FLOOR);
            lvEnv.z = std::max(lvEnv.z - lfDecay, KF_ASSIST_FLOOR);
            lvEnv.w = std::max(lvEnv.w - lfDecay, KF_ASSIST_FLOOR);
            MS.mAssistStrength = lvEnv;
        }

        if (liBest >= 0)
        {
            const Vector3 lvToTarget = vpu::Subtract(MS.maTargetPositions[liBest], lvPosition);
            const f32 lfDistSq = vpu::MagnitudeSquared(lvToTarget);
            const f32 lfDist   = (lfDistSq > 0.0f) ? std::sqrt(lfDistSq) : 0.0f;

            if (lfDist > KF_TARGET_MIN_DISTANCE)   // flt_82F2A324 (2.0)
            {
                Vector3 lvUnit = (lfDist > 0.0f)
                               ? vpu::Mult(lvToTarget, 1.0f / lfDist)
                               : Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };

                // the intercept: 2D speed via BrnMath::Magnitude2D(mLinearVelocity).
                const f32 lfSpeed2D = BrnMath::Magnitude2D(GetLinearVelocity());
                Vector3 lvIdeal;
                ComputeIdealVelocity(&lvIdeal, MS.maTargetPositions[liBest], lfSpeed2D);

                // vertical correction: envelope.y * (ideal.y - vel.y) into the direction's .y
                // (vrlimi128 v126, v0, 4, 3 @0x826202DC).
                lvUnit.y = MS.mAssistStrength.y * (lvIdeal.y - GetLinearVelocity().y);

                // magnitude: min(dist * 4000, 14000) (the fsel pair @0x826202BC-C4).
                f32 lfForce = lfDist * KF_ASSIST_FORCE_PER_METRE;
                if (lfForce > KF_ASSIST_FORCE_CAP)
                    lfForce = KF_ASSIST_FORCE_CAP;

                AddWorldSpaceForce(vpu::Mult(lvUnit, lfForce));

                mUsedAirRams.UnSetAll();   // std r31(0), 0x1158(r28) -- drop the queued air rams
            }
        }
    }

    // ---------------------------------------------------------------------------------------
    // RaceCarPhysics::UpdateAftertouch  @0x8262EBE8
    // REWRITTEN 2026-08-24 (showtime wave) from a fresh full disassembly (show_asm.txt in the
    // wave scratchpad; 2616 bytes read end to end). The committed body's channel structure was
    // a slice sketch; the console's is:
    //   * ENTRY GATES (0x8262EC1C-EC4C): IsCrashing() (+0x710 -- the old body read the
    //     mbUsingAftertouch latch, whose own comment already admitted "this+1808" = 0x710),
    //     !mbIsOnStartLine, and the vtbl+0x14 virtual (IsPlayerVehicleActuallyInShowtime here);
    //     each one bails the WHOLE body.
    //   * camera X / Z normalize with the console's own MagnitudeSquared asserts (:0x207/:0x20F).
    //   * enable scale (0x8262ED98-EE4C): while IN showtime and NOT bounce-boosting,
    //     enable *= (time-in-air (+0x1060.z) > 0.5 ? 0.4 : 0.1) (flt_82F2A2D0 / flt_82F2A2D4).
    //     The old body put this arm on the NOT-showtime branch.
    //   * SIXAXIS tilt (0x8262EE7C-EEDC): gated on mbHasAir (+0x1350) -- not "the downforce
    //     flag" -- with |tilt| >= 0.2 (flt_82F2A314); yaw += sign(tilt) * 0.2 (flt_82F2A318),
    //     sign via fsel (0 when tilt == 0). The tilt channel was DEAD (both constants 0).
    //   * ONE combined world force (0x8262EEE4-F1FC): cameraX * yaw * -12000 (flt_82F2A308)
    //     * enable + cameraZ * pitch * -(pitch >= 0 ? 8000 : 20000) (flt_82F2A30C/310) * enable;
    //     x2.5 (unk_82FB8830) when showtime-and-boosting; NOT showtime: x1.5 (flt_82F2A2B4) and
    //     faded by (1 - dot(unit(force), unit(velocity))) when the dot is positive.
    //   * showtime chain: UpdateTargetAssist(controls, dt, splat(enable),
    //     normalize(-(cameraX*yaw + cameraZ*pitch))) -- the stick-derived aim direction.
    //   * yaw ANGULAR impulse (0x8262F2D0-F38C): worldUp * aftertouchScalar * -2000
    //     (flt_82F2A304 -- the constant the old body spent on its "lateral force") * dt * enable.
    //   * two world-up LEVER impulses (0x8262F394-F5D0), both AddLocalImpulse WORLD/WORLD:
    //     roll:    worldUp * |yaw| * 1400 (flt_82F2A2FC) * dt * enable at position
    //              carPos +/- cameraX * 4.0 (sign of yaw) -- the "live difference of two
    //              vectors" the old FLAG said must be recovered before mounting: recovered.
    //     wheelie: worldUp * |pitch| * 1400 (flt_82F2A300) * dt * enable at position
    //              carPos +/- cameraZ * 4.0 (sign of pitch), gated on
    //              ((aboveGroundValid && verticalDistance < 2.0) || pitch <= 0).
    //   * tail (0x8262F5D4-F604): in showtime, UpdateShowtimePhysics(controls, cameraX, cameraZ,
    //     dt, splat(enable), lbUseSixaxis).
    // WIDENED 2026-08-09 (crash/shunt wave) to the 5-arg DWARF virtual form
    // (VehiclePhysics.h:1514) so it OVERRIDES the base slot +0x28 that UpdateCrashing
    // dispatches -- v1 (`vmr128 v121, v1` @0x8262EC08) is the dt, now consumed for real by the
    // impulse channels and the showtime chain.
    void RaceCarPhysics::UpdateAftertouch(const BrnPlayerDriverControls* lpControls,
                                          const Matrix44Affine* lpCameraMatrix,
                                          VecFloat lvfTimeStep,
                                          bool lbDoForceAdditiveAftertouch, bool lbUseSixaxis)
    {
        if (!IsCrashing())                        // lbz +0x710 @0x8262EC1C, beq -> bail
            return;
        // RE-NAMED 2026-08-03. `lbz r11, 0x40(r25)` @0x8262EC28, must be ZERO to proceed --
        // 0x40 is mbIsOnStartLine. Aftertouch is disabled while the car sits on the start line.
        if (lpControls->mbIsOnStartLine)          // asm lbz +0x40, bne -> bail
            return;
        // the vtbl+0x14 virtual (image-attested slot; IsPlayerVehicleActuallyInShowtime on the
        // RaceCarPhysics vtable @0x820D1034). FALSE bails the whole body @0x8262EC4C.
        if (!IsPlayerVehicleActuallyInShowtime())
            return;

        // normalise the camera X and Z axes (the console's own asserts, :0x207 / :0x20F).
        CGS_ASSERT(vpu::MagnitudeSquared(lpCameraMatrix->xAxis) > 0.0f,
                   "RwMath::MagnitudeSquared(lCameraX) > 0.0f");
        const Vector3 lvCameraX = vpu::Normalize(lpCameraMatrix->xAxis);
        CGS_ASSERT(vpu::MagnitudeSquared(lpCameraMatrix->zAxis) > 0.0f,
                   "RwMath::MagnitudeSquared(lCameraZ) > 0.0f");
        const Vector3 lvCameraZ = vpu::Normalize(lpCameraMatrix->zAxis);

        f32 lfEnable = lpControls->GetAftertouchEnable();   // f29 = *(controls+0x20)
        const bool lbShowtime = mbPlayerCarInShowtime;      // lbz +0x140C

        // showtime-and-not-boosting: scale the enable by the air-time factor (0x8262EDDC-EE4C).
        if (lbShowtime && !IsBounceBoosting())
        {
            lfEnable *= (mvTimeStandingStill_CoolDown_TimeWithoutTraction_TimeWithTraction.z
                             > KF_AIRTIME_GATE)             // +0x1060.z > 0.5 (flt_82F2A2F8)
                            ? KF_AT_ENABLE_AIR_FACTOR       // flt_82F2A2D0 (0.4)
                            : KF_AT_ENABLE_GROUND_FACTOR;   // flt_82F2A2D4 (0.1)
        }

        if (lbDoForceAdditiveAftertouch && lfEnable > 0.0f)
        {
            f32 lfYaw = 0.0f, lfPitch = 0.0f, lfScalar = 0.0f;
            // FORK RESOLVED 2026-08-06 (UpdateVehiclePhysics wave): the console leaf
            // @0x825B2E88 is the 4-arg reference form; THIS call site passes the bool as a
            // literal FALSE (`li r7, 0` @0x8262EE64, bl @0x8262EE78 -- raw image bytes).
            lpControls->GetAftertouchValues(lfYaw, lfPitch, lfScalar, false);

            // SIXAXIS tilt contribution, gated on the car being airborne (0x8262EE7C-EEDC).
            if (mbHasAir)   // lbz +0x1350
            {
                const f32 lfTilt = lpControls->GetSixaxisTilt();   // lfs +0x18
                if (std::fabs(lfTilt) >= KF_AT_TILT_GATE)          // flt_82F2A314 (0.2)
                {
                    // fsel sign: exactly 0 for a zero tilt, else +/-1 (0x8262EEA4-BC).
                    const f32 lfSign = (lfTilt == 0.0f) ? 0.0f : ((lfTilt >= 0.0f) ? 1.0f : -1.0f);
                    lfYaw += lfSign * KF_AT_TILT_GAIN;             // flt_82F2A318 (0.2)
                    MS.mbSixaxisTiltApplied = true;                // byte_82FB848A = 1
                }
            }

            // ONE combined world-space force: the camera-X yaw channel + the camera-Z pitch
            // channel (0x8262EEE4-F010). Both scales enter NEGATED (fneg @0x8262EEF4/EF9C/EFB8).
            const f32 lfPitchScale = (lfPitch >= 0.0f) ? KF_AT_FORCE_PITCH_UP     // 8000
                                                       : KF_AT_FORCE_PITCH_DOWN;  // 20000
            Vector3 lvForce = vpu::Add(
                vpu::Mult(lvCameraX, lfYaw * -KF_AT_FORCE_YAW * lfEnable),
                vpu::Mult(lvCameraZ, lfPitch * -lfPitchScale * lfEnable));

            if (lbShowtime)
            {
                if (IsBounceBoosting())
                    lvForce = vpu::Mult(lvForce, KF_AT_BOOST_MULTIPLIER);   // x unk_82FB8830 (2.5)
            }
            else
            {
                // not showtime: x1.5, then fade by alignment with the velocity (0x8262F048-F1A4).
                lvForce = vpu::Mult(lvForce, KF_AT_NONSHOWTIME_SCALE);      // flt_82F2A2B4
                const f32 lfForceMagSq = vpu::MagnitudeSquared(lvForce);
                if (lfForceMagSq > 1.1920929e-07f)   // FLT_EPSILON (stru_8208F620 lvlx)
                {
                    const Vector3 lvVel = GetLinearVelocity();
                    const f32 lfVelMagSq = vpu::MagnitudeSquared(lvVel);
                    const f32 lfDot = (lfVelMagSq > 0.0f)
                                    ? vpu::Dot(vpu::Mult(lvForce, 1.0f / std::sqrt(lfForceMagSq)),
                                               vpu::Mult(lvVel, 1.0f / std::sqrt(lfVelMagSq)))
                                    : 0.0f;
                    // fsel @0x8262F174: scale 1.0 when the dot is negative, else (1 - dot).
                    lvForce = vpu::Mult(lvForce, (lfDot < 0.0f) ? 1.0f : (1.0f - lfDot));
                }
            }

            if (vpu::MagnitudeSquared(lvForce) > 1.1920929e-07f)   // the FLT_EPSILON lane test
                AddWorldSpaceForce(lvForce);                       // bl @0x8262F1FC

            if (lbShowtime)
            {
                // the stick-derived aim direction (0x8262F26C-F2C8):
                // normalize(-(cameraX*yaw + cameraZ*pitch)), zero when degenerate.
                Vector3 lvAim = vpu::Add(vpu::Mult(lvCameraX, lfYaw),
                                         vpu::Mult(lvCameraZ, lfPitch));
                lvAim = Vector3{ -lvAim.x, -lvAim.y, -lvAim.z, -lvAim.w };   // vxor sign flip
                const f32 lfAimMagSq = vpu::MagnitudeSquared(lvAim);
                lvAim = (lfAimMagSq > 0.0f)
                      ? vpu::Mult(lvAim, 1.0f / std::sqrt(lfAimMagSq))
                      : Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // vsel zero guard

                UpdateTargetAssist(lpControls, lvfTimeStep,
                                   VecFloat{ lfEnable, lfEnable, lfEnable, lfEnable }, lvAim);
            }

            // yaw ANGULAR impulse from the aftertouch scalar (0x8262F2D0-F38C):
            // worldUp * scalar * -2000 * dt * enable.
            Vector3 lvYawImpulse = vpu::Mult(
                KV_WORLD_UP, lfScalar * KF_AT_ROLL_SCALAR * lvfTimeStep.x * lfEnable);
            if (vpu::MagnitudeSquared(lvYawImpulse) > 1.1920929e-07f)
                AddWorldSpaceAngularImpulse(lvYawImpulse);

            // lever impulse 1 -- roll from yaw (0x8262F394-F49C): worldUp * |yaw| * 1400 * dt *
            // enable, applied at carPos +/- cameraX * 4.0 (sign of yaw). WORLD/WORLD tags
            // (`li r5,0 ; li r4,0` at both call sites -- console-exact).
            {
                Vector3 lvImpulse = vpu::Mult(
                    KV_WORLD_UP, std::fabs(lfYaw) * KF_AT_LEVER_IMPULSE_YAW
                                     * lvfTimeStep.x * lfEnable);
                const Vector3 lvArm = vpu::Mult(lvCameraX, KF_AT_LEVER_ARM);
                const Vector3 lvAt  = (lfYaw > 0.0f) ? vpu::Add(mTransform.Pos(), lvArm)
                                                     : vpu::Subtract(mTransform.Pos(), lvArm);
                if (vpu::MagnitudeSquared(lvImpulse) > 1.1920929e-07f)
                    AddLocalImpulse(lvImpulse, rw::physics::WORLD_SPACE,
                                    lvAt, rw::physics::WORLD_SPACE);
            }

            // lever impulse 2 -- wheelie/pitch (0x8262F4A8-F5D0): worldUp * |pitch| * 1400 * dt
            // * enable at carPos +/- cameraZ * 4.0 (sign of pitch); pitch-UP only near the
            // ground: ((valid && verticalDistance < 2.0) || pitch <= 0).
            if ((mAboveGroundTestResult.mbValid                                  // lbz +0x598
                 && mAboveGroundTestResult.mfVerticalDistance < KF_PITCH_NEAR_GROUND)  // +0x590 < 2.0
                || !(lfPitch > 0.0f))
            {
                Vector3 lvImpulse = vpu::Mult(
                    KV_WORLD_UP, std::fabs(lfPitch) * KF_AT_LEVER_IMPULSE_PITCH
                                     * lvfTimeStep.x * lfEnable);
                const Vector3 lvArm = vpu::Mult(lvCameraZ, KF_AT_LEVER_ARM);
                const Vector3 lvAt  = (lfPitch > 0.0f) ? vpu::Add(mTransform.Pos(), lvArm)
                                                       : vpu::Subtract(mTransform.Pos(), lvArm);
                if (vpu::MagnitudeSquared(lvImpulse) > 1.1920929e-07f)
                    AddLocalImpulse(lvImpulse, rw::physics::WORLD_SPACE,
                                    lvAt, rw::physics::WORLD_SPACE);
            }
        }

        if (lbShowtime)   // lbz +0x140C @0x8262F5D4
            UpdateShowtimePhysics(lpControls, lvCameraX, lvCameraZ, lvfTimeStep,
                                  VecFloat{ lfEnable, lfEnable, lfEnable, lfEnable },
                                  lbUseSixaxis);
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
        static_assert(offsetof(PlayerParameters, mAimDirection)        == 0x20, "+0x20 aim (SetShowtimeAimDirection stvx128)");
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
        static_assert(offsetof(PlayerParameters, mAssistStrength)      == 0x100, "unk_82FB8580 (assist envelope)");
        static_assert(offsetof(PlayerParameters, mu8NumBounceSensors)  == 0x110, "byte_82FB8590");
        static_assert(offsetof(PlayerParameters, maBounceSensors)      == 0x120, "unk_82FB85A0 (32B-stride sensor scratch)");
        static_assert(sizeof(PlayerParameters::BounceSensor)           == 0x20, "slwi r31, 5 -- 32-byte stride");
        static_assert(offsetof(PlayerParameters::BounceSensor, mfSpecScalar)  == 0x10, "flt_82FB85B0 - unk_82FB85A0");
        static_assert(offsetof(PlayerParameters::BounceSensor, mfCrushFactor) == 0x14, "flt_82FB85B4 - unk_82FB85A0");
    }
}
}
