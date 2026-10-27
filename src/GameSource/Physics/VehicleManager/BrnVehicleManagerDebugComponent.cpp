// ============================================================================================
// BrnPhysics::Vehicle::VehicleManagerDebugComponent - Construct + debug render callbacks.
//
// Layout/shape authority: references/DecFIGS/dwarfdump/GameSource/Physics/VehicleManager/
// BrnVehicleManagerDebugComponent.h (complete member sequence) + the X360 ARTIST asm.
// Every offset quoted in the comments is asm-literal; see the header banner for the three
// independent closures that pin the class to 1296 bytes.
// ============================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h"

#include <cstddef>                                          // offsetof (layout asserts)
#include <cstring>                                          // std::memcpy for the manager cache sphere
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"                   // VehicleManager (RecordCrashContact's two bare reads; friend)
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"            // PhysicalTrafficVehicle lookup
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h" // traffic body snapshot
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"       // RaceCarPhysics (GetTransform / GetDeformableAABB / GetLinearVelocity)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"        // shared debug-menu tunables
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"         // CgsGeometric::AxisAlignedBox
#include "GameSource/World/BrnEntityTypes.h"                                        // BrnWorld::E_ENTITYTYPE_*
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                             // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"                         // CgsDev::SimpleStrStream
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"       // DebugUI::GetMetrics
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"    // Draw2DTextJustified
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "rw/math/vpu/vector3_operation.h"                                           // vpu::Add / Subtract / Mult / Normalize

int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

namespace BrnPhysics
{
namespace Vehicle
{
    // X360 data variables exposed by VehicleManagerDebugComponent::OnActivate.
    f32 gfGroundContactCullHeight = 0.4f;          // 0x82F2A148; read by both VehicleManager world-contact arms
    f32 gfLastShuntClosingSpeed = 0.0f;             // 0x82FB7E18; no other ARTIST data reference found
    f32 gfLastShuntAggressorSpeed = 0.0f;           // 0x82FB7E1C; no other ARTIST data reference found

    // ----------------------------------------------------------------------------------------
    // The "invalid volume instance" sentinel the two embedded PotentialContacts are seeded with.
    //
    // X360: `ld rN, qword_82F2A3C8` -> stored to both PotentialContact::muVolumeInstanceId{A,B}.
    // The slot is INITIALISED IN THE IMAGE -- read directly out of BURNOUT_X360_ARTIST.XEX.id1
    // with the self-calibrating reader (delta -1594, 9/9 witnesses):
    //     0x82F2A3C8 = FF FF FF FF FF FF FF FF
    // so this is NOT one of the static-init-filled slots that read zero in the image. (Its
    // neighbour qword_82F2A3A8, the invalid RigidBodyId that VehicleManager::Construct seeds
    // maRaceCarHandlingBodyIDs with, reads the same all-ones value.)
    // ----------------------------------------------------------------------------------------
    static const u64 KU64_INVALID_VOLUME_INSTANCE_ID = 0xFFFFFFFFFFFFFFFFull;   // qword_82F2A3C8

    // ========================================================================================
    // VehicleManagerDebugComponent::Construct   @ 0x825B5A78   (194 instructions)
    // DWARF: BrnVehicleManagerDebugComponent.cpp:87  `void Construct(VehicleManager *)`
    //
    // The X360 scheduler interleaves this function's stores almost completely (the six 16-byte
    // stack scratch slots at sp+0x50..sp+0xAC are re-written between every `lvx128`), so the
    // stores are re-ordered here into the DWARF member sequence. Every store targets a distinct
    // member, so the ordering is behaviourally inert; the SET of stores is what is faithful, and
    // it is exact -- including the four members Construct deliberately leaves alone.
    //
    // NOT written by the X360 body (deliberate, verified by scanning every store in the range):
    //   miNumWorldContactSpies, mbDisplayContact, mfOtherCarContactAngleRad,
    //   mPlayerCarContactPosition, mOtherCarContactPosition, mClosingVelocityPlayerCarSpace,
    //   mClosingVelocityOtherCarSpace, mPlayerCarVelocity, mOtherCarVelocity,
    //   mLastTrafficContact_RaceCarVelocity, mLastTrafficContact_TrafficVelocity.
    // ========================================================================================
    void VehicleManagerDebugComponent::Construct(VehicleManager* lpVehicleManager)
    {
        // 0x825B5A8C  bl <0x8284CB38>. That target is a single `blr` -- the ICF representative
        // for every empty function in the image (IDA names it
        // CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct). Called with r3 ==
        // this at the very top of a Construct, it is the base component's own Construct.

        // 0x825B5A90..0x825B5AB4, BrnVehicleManagerDebugComponent.cpp:92
        CGS_ASSERT(lpVehicleManager != 0, "lpVehicleManager != NULL");

        mbDisplayWorldContactSpies      = false;             // stb 0, 0x1D4(r31)
        mpVehicleManager                = lpVehicleManager;  // stw r29, 0x1D8(r31)
        mbSlamDebugRenderEnabled        = false;             // stb 0, 0x1DC(r31)
        mbRenderResetLines              = false;             // stb 0, 0x1DD(r31)
        mbGrindDebugRenderEnabled       = false;             // stb 0, 0x1DE(r31)

        // The eight consecutive render gates at 0x250..0x257.
        mbDrawLastRaceCarTrafficContact  = false;            // stb 0, 0x250(r31)
        mbDrawLastSlamInfo               = false;            // stb 0, 0x251(r31)
        mbDrawLastShuntInfo              = false;            // stb 0, 0x252(r31)
        mbDrawLastCrashContact           = false;            // stb 0, 0x253(r31)
        mbDrawCatchupTargets             = false;            // stb 0, 0x254(r31)
        mbRenderWallContacts             = false;            // stb 0, 0x255(r31)
        mbRenderGroundContacts           = false;            // stb 0, 0x256(r31)
        mbDrawPlayerStuckInCollisionTests = false;           // stb 0, 0x257(r31)

        // ---- last race-car-vs-traffic contact -----------------------------------------------
        // Traced store-by-store out of the interleaved stack scratch: both matrices resolve to
        // {1,0,0,0}/{0,1,0,0}/{0,0,1,0}/{0,0,0,0}, i.e. Matrix44Affine::SetIdentity (wAxis zero,
        // not (0,0,0,1)) -- the same identity Camera::Clear @0x8223CE70 already documents.
        mLastTrafficContact_RaceCarTransform.SetIdentity();  // 4 x stvx128 @ r9 = this + 0x260
        mLastTrafficContact_TrafficTransform.SetIdentity();  // 4 x stvx128 @ r8 = this + 0x2A0
        mLastTrafficContact_RaceCarHalfExt.SetZero();        // stvx128 v0, r31, r26 (0x2E0)
        mLastTrafficContact_TrafficHalfExt.SetZero();        // stvx128 v0, r31, r26 (0x2F0)

        // PotentialContact::Construct() INLINED at r10 = this + 0x320. There is no out-of-line
        // CgsSceneManager::SceneManagerIO::PotentialContact::Construct symbol anywhere in the
        // ARTIST export set, so (per the standing convention for inlined helpers with no
        // out-of-line body) the expansion is transcribed store-for-store rather than invented as
        // a body in the scene-manager group's header. The nine stores map field-for-field onto
        // the committed CgsPotentialContact.h member sequence.
        mLastTrafficContact_Contact.mPointOnA.SetZero();                                  // stvx128 v0, r0,  r10
        mLastTrafficContact_Contact.mPointOnB.SetZero();                                  // stvx128 v0, r10, 0x10
        mLastTrafficContact_Contact.mNormal.SetZero();                                    // stvx128 v0, r10, 0x20
        mLastTrafficContact_Contact.muVolumeInstanceIdA.muId = KU64_INVALID_VOLUME_INSTANCE_ID; // std, 0x30(r10)
        mLastTrafficContact_Contact.muVolumeInstanceIdB.muId = KU64_INVALID_VOLUME_INSTANCE_ID; // std, 0x38(r10)
        mLastTrafficContact_Contact.muPolyTagA = 0;                                       // stw 0, 0x40(r10)
        mLastTrafficContact_Contact.muPolyTagB = 0;                                       // stw 0, 0x44(r10)
        mLastTrafficContact_Contact.mu16PrimitiveIndexA = 0;                              // sth 0, 0x48(r10)
        mLastTrafficContact_Contact.mu16PrimitiveIndexB = 0;                              // sth 0, 0x4A(r10)

        // ---- last crash contact --------------------------------------------------------------
        mLastCrashContact_RaceCarTransform.SetIdentity();    // 4 x stvx128 @ r10 = this + 0x370
        mLastCrashContact_RaceCarHalfExt.SetZero();          // stvx128 v0, r31, r28 (0x3B0)
        mLastCrashContact_RaceCarVelocity.SetZero();         // stvx128 v0, r31, r27 (0x3C0)

        // The second inlined PotentialContact::Construct(), at r10 = this + 0x3D0 -- byte for byte
        // the same nine stores as the first, which is what attests the expansion.
        mLastCrashContact_Contact.mPointOnA.SetZero();
        mLastCrashContact_Contact.mPointOnB.SetZero();
        mLastCrashContact_Contact.mNormal.SetZero();
        mLastCrashContact_Contact.muVolumeInstanceIdA.muId = KU64_INVALID_VOLUME_INSTANCE_ID;
        mLastCrashContact_Contact.muVolumeInstanceIdB.muId = KU64_INVALID_VOLUME_INSTANCE_ID;
        mLastCrashContact_Contact.muPolyTagA = 0;
        mLastCrashContact_Contact.muPolyTagB = 0;
        mLastCrashContact_Contact.mu16PrimitiveIndexA = 0;
        mLastCrashContact_Contact.mu16PrimitiveIndexB = 0;

        // ---- last slam / last shunt ------------------------------------------------------------
        // The two `li r30, -1` stores are E_IMPACT_SITUATION_INVALID (BrnVehicleConstants.h).
        meLastSlamImpactSituation  = E_IMPACT_SITUATION_INVALID;  // stw r30(-1), 0x420(r31)
        miLastSlamNumber           = 0;                           // stw 0,       0x424(r31)
        mfLastSlamDuration         = 0.0f;                        // stfs f0,     0x428(r31)
        mfLastSlamBaseDuration     = 0.0f;                        // stfs f0,     0x42C(r31)
        mfLastSlamMassFactor       = 0.0f;                        // stfs f0,     0x430(r31)
        meLastShuntImpactSituation = E_IMPACT_SITUATION_INVALID;  // stw r30(-1), 0x434(r31)
        mfLastShuntMagnitude       = 0.0f;                        // stfs f0,     0x438(r31)
        mfLastShuntClosingSpeed    = 0.0f;                        // stfs f0,     0x43C(r31)

        // ---- the stuck-in-collision line tests ------------------------------------------------
        // The tail loop @0x825B5D58: r9 starts at this + 0x480 (== &maStuckInCollisionLineTestEnd[0])
        // and each iteration stores a zero vector at [r9-64] (Start), [r9] (End) and [r9+64] (Point)
        // then `stbx` a zero byte at this + 0x500 + i, advancing r9 by 16. Four iterations.
        for (s32 liTest = 0; liTest < KI_MAX_WORLD_CONTACT_SPIES; ++liTest)
        {
            maStuckInCollisionLineTestStart[liTest].SetZero();   // stvx128 v0, r9, -0x40
            maStuckInCollisionLineTestEnd[liTest].SetZero();     // stvx128 v0, r0,  r9
            maStuckInCollisionLineTestPoint[liTest].SetZero();   // stvx128 v0, r9,  0x40
            mabStuckInCollisionIntersection[liTest] = false;     // stbx    r11, r27, r3
        }
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::GetName   @ 0x825B5D80
    //   lis r11, aVehicleManager@ha ; addi r3, r11, aVehicleManager@l ; blr
    // ========================================================================================
    const char* VehicleManagerDebugComponent::GetName() const
    {
        return "Vehicle Manager";
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::OnActivate @ 0x825B5D90
    // Register the X360 menu entries in their original order. The duplicate vulnerability and
    // slam-speed registrations deliberately retain the same underlying manager fields.
    // ========================================================================================
    void VehicleManagerDebugComponent::OnActivate()
    {
        RegisterVariable(&mbDrawPlayerStuckInCollisionTests, "Collision", "Draw player stuck tests");
        RegisterVariable(&mpVehicleManager->maRaceCarVehicles[0].mfTimeUntilStuckInCollisionTest,
                         "Collision", "Time until stuck test");
        RegisterVariable(&mbDrawCatchupTargets, "Catchup", "Draw catchup targets");

        RegisterVariable(&gfFullDampThreshold, "Jumps", "Full damp threshold");
        RegisterVariable(&gfNoDampThreshold, "Jumps", "No damp threshold");
        RegisterVariable(&gfMinRollFactor, "Jumps", "Min. roll factor");
        SetRange(&gfFullDampThreshold, 0.0f, 1.0f);
        SetRange(&gfNoDampThreshold, 0.0f, 1.0f);
        SetRange(&gfMinRollFactor, 0.0f, 1.0f);
        SetStep(&gfFullDampThreshold, 0.0099999998f);
        SetStep(&gfNoDampThreshold, 0.0099999998f);
        SetStep(&gfMinRollFactor, 0.0099999998f);
        RegisterVariable(&gfRollDampingUsed, "Jumps", "Roll damping used");
        SetReadOnly(&gfRollDampingUsed, true);

        RegisterVariable(&mpVehicleManager->miNumSPUTractionLineTests, "Performance", "Num traction tests");
        SetReadOnly(&mpVehicleManager->miNumSPUTractionLineTests, true);

        RegisterVariable(&mpVehicleManager->mbStopPlayerCrashing, "Crashes", "Stop player crashing");
        RegisterVariable(&mpVehicleManager->mbStopAICrashing, "Crashes", "Stop AI crashing");
        RegisterVariable(&mpVehicleManager->mbCrashOnHandbrakeTurn, "Crashes", "Handbrake turns cause crashes");
        RegisterVariable(&mpVehicleManager->DEBUG_mbAlwaysCrashRaceCarToRaceCar,
                         "Crashes", "Always crash race car on race car");
        RegisterVariable(&gfGroundContactCullHeight, "Crashes", "Ground Contact Cull Height");
        SetRange(&gfGroundContactCullHeight, 0.0f, 1.0f);
        SetStep(&gfGroundContactCullHeight, 0.0099999998f);
        RegisterVariable(&mpVehicleManager->mfTrafficCollisionCheckThresholdMPH,
                         "Crashes", "Traffic check min speed");
        SetRange(&mpVehicleManager->mfTrafficCollisionCheckThresholdMPH, 0.0f, 100.0f);
        SetStep(&mpVehicleManager->mfTrafficCollisionCheckThresholdMPH, 2.5f);
        RegisterVariable(&mpVehicleManager->mfMinRCTrafficTranslateSpeedMPH,
                         "Crashes", "'Translating bollox' min speed");
        RegisterVariable(&mpVehicleManager->mfHeadOnWorldCrashThreshold,
                         "Crashes", "Head-on world crash factor");
        SetStep(&mpVehicleManager->mfHeadOnWorldCrashThreshold, 0.5f);
        SetRange(&mpVehicleManager->mfHeadOnWorldCrashThreshold, 0.0f, 100.0f);
        RegisterVariable(&mpVehicleManager->mfSideOnWorldCrashThreshold,
                         "Crashes", "Side-on world crash factor");
        SetStep(&mpVehicleManager->mfSideOnWorldCrashThreshold, 0.5f);
        SetRange(&mpVehicleManager->mfSideOnWorldCrashThreshold, 0.0f, 100.0f);
        RegisterVariable(&mpVehicleManager->mbDebugModifyTrafficContacts,
                         "Crashes", "Modify traffic contacts");
        RegisterVariable(&mbDrawLastRaceCarTrafficContact,
                         "Crashes", "Draw last RC-traffic contact");
        RegisterVariable(&mbDrawLastCrashContact, "Crashes", "Draw last crash contact");
        RegisterVariable(&mbRenderWallContacts, "Crashes", "Draw wall contacts");
        RegisterVariable(&mbRenderGroundContacts, "Crashes", "Draw ground contacts");

        RegisterVariable(&gvMaxCrashAngularVelocity.x, "Crashes", "Max X Angular Velocity");
        SetStep(&gvMaxCrashAngularVelocity.x, 0.1f);
        SetRange(&gvMaxCrashAngularVelocity.x, 0.0f, 100.0f);
        RegisterVariable(&gvMaxCrashAngularVelocity.y, "Crashes", "Max Y Angular Velocity");
        SetStep(&gvMaxCrashAngularVelocity.y, 0.1f);
        SetRange(&gvMaxCrashAngularVelocity.y, 0.0f, 100.0f);
        RegisterVariable(&gvMaxCrashAngularVelocity.z, "Crashes", "Max Z Angular Velocity");
        SetStep(&gvMaxCrashAngularVelocity.z, 0.1f);
        SetRange(&gvMaxCrashAngularVelocity.z, 0.0f, 100.0f);

        RegisterVariable(&mpVehicleManager->mfVerticalTakedownAngleDeg,
                         "Takedowns", "Max vertical TD angle degrees");
        SetRange(&mpVehicleManager->mfVerticalTakedownAngleDeg, 0.0f, 90.0f);
        RegisterVariable(&mpVehicleManager->DEBUG_mbHornTakedownEnabled, "Takedowns", "Killer Horn");

        RegisterVariable(&mpVehicleManager->mbSlamsAndShuntsOn, "Slams", "Slams And Shunts On");
        RegisterVariable(&gfLastShuntClosingSpeed, "Slams", "Last Shunt Closing Speed");
        SetStep(&gfLastShuntClosingSpeed, 1.0f);
        RegisterVariable(&gfLastShuntAggressorSpeed, "Slams", "Last Shunt Aggressor Speed");
        SetStep(&gfLastShuntAggressorSpeed, 1.0f);
        RegisterVariable(&gfForcedAcceleration, "Slams", "Forced Acceleration");
        SetStep(&gfForcedAcceleration, 0.1f);
        RegisterVariable(&mpVehicleManager->mfMinNudgeSpeed, "Slams", "Min Nudge Speed");
        SetStep(&mpVehicleManager->mfMinNudgeSpeed, 1.0f);
        RegisterVariable(&mpVehicleManager->mfMinShuntSpeed, "Slams", "Min Shunt Speed");
        SetStep(&mpVehicleManager->mfMinShuntSpeed, 1.0f);
        RegisterVariable(&mpVehicleManager->mfFatalShuntSpeed, "Slams", "Fatal Shunt Speed");
        SetStep(&mpVehicleManager->mfFatalShuntSpeed, 1.0f);
        RegisterVariable(&mpVehicleManager->mfMaxShuntAngle, "Slams", "Max Shunt Angle");
        SetStep(&mpVehicleManager->mfMaxShuntAngle, 1.0f);
        RegisterVariable(&mpVehicleManager->mfMinShuntMagnitude, "Slams", "Min Shunt Magnitude");
        SetStep(&mpVehicleManager->mfMinShuntMagnitude, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfMaxShuntMagnitude, "Slams", "Max Shunt Magnitude");
        SetStep(&mpVehicleManager->mfMaxShuntMagnitude, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfMinTradingPaintSpeed, "Slams", "Min Trading Paint Speed");
        SetStep(&mpVehicleManager->mfMinTradingPaintSpeed, 1.0f);
        RegisterVariable(&mpVehicleManager->mfMaxSlamClosingXSpeed, "Slams", "Max Slam Speed");
        SetStep(&mpVehicleManager->mfMaxSlamClosingXSpeed, 1.0f);
        RegisterVariable(&mpVehicleManager->mfFatalSlamSpeed, "Slams", "Fatal Slam Speed");
        SetStep(&mpVehicleManager->mfFatalSlamSpeed, 1.0f);
        RegisterVariable(&mpVehicleManager->mfSlamEffectMinMagnitude, "Slams", "Slam Effect Min");
        SetStep(&mpVehicleManager->mfSlamEffectMinMagnitude, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfSlamEffectMaxMagnitude, "Slams", "Slam Effect Max");
        SetStep(&mpVehicleManager->mfSlamEffectMaxMagnitude, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfFatalHitCrashingCarSpeed,
                         "Slams", "Fatal Hit Crashing Car Speed");
        SetStep(&mpVehicleManager->mfFatalHitCrashingCarSpeed, 1.0f);
        RegisterVariable(&mpVehicleManager->mfMaxHeadToHeadAngle, "Slams", "Head To Head Max Angle");
        SetStep(&mpVehicleManager->mfMaxHeadToHeadAngle, 1.0f);
        RegisterVariable(&mpVehicleManager->mfMinHeadToHeadSpeed, "Slams", "Head To Head Min Closing Speed");
        SetStep(&mpVehicleManager->mfMinHeadToHeadSpeed, 1.0f);
        RegisterVariable(&mpVehicleManager->mfMinHeadToHeadIndividualSpeed,
                         "Slams", "Head To Head Min Fatal Speed");
        SetStep(&mpVehicleManager->mfMinHeadToHeadIndividualSpeed, 1.0f);
        RegisterVariable(&mpVehicleManager->mfTBoneTakedownMaxAngle, "Slams", "T-Bone Takedown Max Angle");
        SetStep(&mpVehicleManager->mfTBoneTakedownMaxAngle, 1.0f);
        RegisterVariable(&mpVehicleManager->mfTBoneTakedownSpeed, "Slams", "T-Bone Takedown Min Speed");
        SetStep(&mpVehicleManager->mfTBoneTakedownSpeed, 1.0f);

        RegisterVariable(&mbDrawLastSlamInfo, "Slam Details", "Draw last slam info");
        RegisterVariable(&mbDrawLastShuntInfo, "Slam Details", "Draw last shunt info");
        RegisterVariable(&mbSlamDebugRenderEnabled, "Slam Details", "Slam Debug");
        RegisterVariable(&mbGrindDebugRenderEnabled, "Slam Details", "Grinding Debug");
        RegisterVariable(&mpVehicleManager->mfVulnerabilityFactorMax,
                         "Slam Details", "Slam vulnerability max");
        RegisterVariable(&mpVehicleManager->mfAIVulnerabilityDurationSeconds,
                         "Slam Details", "Slam vulnerability seconds on Player");
        SetStep(&mpVehicleManager->mfAIVulnerabilityDurationSeconds, 0.1f);
        RegisterVariable(&mpVehicleManager->mfAIVulnerabilityDurationSeconds,
                         "Slam Details", "Slam vulnerability seconds on AI");
        SetStep(&mpVehicleManager->mfAIVulnerabilityDurationSeconds, 0.1f);
        RegisterVariable(&mpVehicleManager->mfMinSteeringOverrideTimeShunt,
                         "Slam Details", "Min shunt steering override time");
        SetStep(&mpVehicleManager->mfMinSteeringOverrideTimeShunt, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfMinSteeringOverrideTimeSlam,
                         "Slam Details", "Min slam steering override time");
        SetStep(&mpVehicleManager->mfMinSteeringOverrideTimeSlam, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfPlayerMaxSteeringOverrideTimeSlam,
                         "Slam Details", "Player Slam Steering override max time");
        SetStep(&mpVehicleManager->mfPlayerMaxSteeringOverrideTimeSlam, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfPlayerMaxSteeringOverrideTimeShunt,
                         "Slam Details", "Player Shunt Steering override max time");
        SetStep(&mpVehicleManager->mfPlayerMaxSteeringOverrideTimeShunt, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfMinAmountOfSlamForce,
                         "Slam Details", "Min Amount of player Slam force");
        SetStep(&mpVehicleManager->mfMinAmountOfSlamForce, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfMinAmountOfShuntForce,
                         "Slam Details", "Min Amount of player Shunt force");
        SetStep(&mpVehicleManager->mfMinAmountOfShuntForce, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfAIMaxSteeringOverrideTimeSlam,
                         "Slam Details", "AI Slam Steering override max time");
        SetStep(&mpVehicleManager->mfAIMaxSteeringOverrideTimeSlam, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfAIMaxSteeringOverrideTimeShunt,
                         "Slam Details", "AI Shunt Steering override max time");
        SetStep(&mpVehicleManager->mfAIMaxSteeringOverrideTimeShunt, 0.050000001f);
        RegisterVariable(&mpVehicleManager->mfPlayerSlamForceScale,
                         "Slam Details", "Player Slam force scale");
        SetStep(&mpVehicleManager->mfPlayerSlamForceScale, 0.0099999998f);
        RegisterVariable(&mpVehicleManager->mfAISlamForceScale,
                         "Slam Details", "AI Slam force scale");
        SetStep(&mpVehicleManager->mfAISlamForceScale, 0.0099999998f);
        RegisterVariable(&mpVehicleManager->mfMaxSlamClosingXSpeed,
                         "Slam Details", "Max slam closing X speed");
        SetStep(&mpVehicleManager->mfMaxSlamClosingXSpeed, 0.1f);
        RegisterVariable(&mpVehicleManager->mfMinSecondsBetweenImpacts,
                         "Slam Details", "Min seconds between impacts");
        SetStep(&mpVehicleManager->mfMinSecondsBetweenImpacts, 0.050000001f);
        SetRange(&mpVehicleManager->mfMinSecondsBetweenImpacts, 0.0f, 3.0f);
        RegisterVariable(&mpVehicleManager->mfBaseSlamMagnitude, "Slam Details", "SLAM MAGNITUDE");
        SetStep(&mpVehicleManager->mfBaseSlamMagnitude, 0.2f);
        RegisterVariable(&mpVehicleManager->mfSlamDecayRate, "Slam Details", "SLAM DECAY RATE");
        SetStep(&mpVehicleManager->mfSlamDecayRate, 0.0099999998f);
        RegisterVariable(&mpVehicleManager->mfBaseShuntMagnitude, "Slam Details", "SHUNT MAGNITUDE");
        SetStep(&mpVehicleManager->mfBaseShuntMagnitude, 0.2f);
        RegisterVariable(&mpVehicleManager->mfShuntDecay, "Slam Details", "SHUNT DECAY RATE");
        SetStep(&mpVehicleManager->mfShuntDecay, 0.0099999998f);

        RegisterVariable(&mbRenderResetLines, "Reset Rays...", "Render reset rays");
        RegisterVariable(&mpVehicleManager->mfFrontRayLength, "Reset Rays...", "Front Ray Length");
        SetStep(&mpVehicleManager->mfFrontRayLength, 0.0099999998f);
        RegisterVariable(&mpVehicleManager->mfRearRayLength, "Reset Rays...", "Rear Ray Length");
        SetStep(&mpVehicleManager->mfRearRayLength, 0.0099999998f);
        RegisterVariable(&mbDisplayWorldContactSpies, "", "Draw World Contact Spies");
        RegisterVariable(&mpVehicleManager->mbCrashRaceCarWhenFatal, "", "Crash race cars on fatal");

        RegisterVariable(&gbEnableInAirRotation, "In-air", "Enable in-air rotation");
        RegisterVariable(&gfRollCorrectionFactor, "In-air", "Roll correction factor");
        SetStep(&gfRollCorrectionFactor, 0.0099999998f);
        SetRange(&gfRollCorrectionFactor, 0.0f, 10.0f);
        RegisterVariable(&gfLandingAssistDamping, "In-air", "Landing assist damping");
        SetStep(&gfLandingAssistDamping, 0.0099999998f);
        SetRange(&gfLandingAssistDamping, 0.0f, 1.0f);
        RegisterVariable(&gfIncreaseRollFactor, "In-air", "Increase roll factor");
        SetStep(&gfIncreaseRollFactor, 0.0099999998f);
        SetRange(&gfIncreaseRollFactor, 0.0f, 10.0f);
        RegisterVariable(&gfMinRollToAllowCorrection, "In-air", "Min roll to allow correction");
        SetStep(&gfMinRollToAllowCorrection, 0.0099999998f);
        SetRange(&gfMinRollToAllowCorrection, 0.0f, 1.0f);
        RegisterVariable(&gfMaxWheelieAngle, "Wheelies", "Max wheelie angle");
        SetStep(&gfMaxWheelieAngle, 0.5f);
        SetRange(&gfMaxWheelieAngle, 0.0f, 90.0f);
        RegisterVariable(&gfWheelieLimitDamping, "Wheelies", "Wheelie limit damping");
        SetStep(&gfWheelieLimitDamping, 0.0099999998f);
        SetRange(&gfWheelieLimitDamping, 0.0f, 1.0f);
    }

    namespace
    {
        // The impact-situation names and the slam-duration situation scales the HUD prints. The
        // scale table holds the same image values VehicleManager's slam path multiplies the
        // duration by (0.75 / 0.25 / 1.0 / 2.0).
        const char* const KAPC_IMPACT_SITUATION_NAMES[E_IMPACT_SITUATION_COUNT] =
        {
            "Player on AI", "AI on Player", "AI on AI", "Network",
        };

        const f32 KAF_SLAM_SITUATION_SCALE[E_IMPACT_SITUATION_COUNT] = { 0.75f, 0.25f, 1.0f, 2.0f };

        const f32 KF_RADIANS_TO_DEGREES = 57.2957763671875f;

        rw::math::vpu::Vector2 MakeVector2(f32 lfX, f32 lfY)
        {
            rw::math::vpu::Vector2 lv2Result;
            lv2Result.x = lfX;
            lv2Result.y = lfY;
            lv2Result.z = 0.0f;
            lv2Result.w = 0.0f;
            return lv2Result;
        }
    }

    void VehicleManagerDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        static const f32 KF_X          = 100.0f;
        static const f32 KF_TEXT_SCALE = 20.0f;
        static const u32 KU_COLOUR     = 0xFFFFFFFFu;

        if (mbSlamDebugRenderEnabled)
        {
            RenderContact(lpRender);
        }

        if (mbDrawLastSlamInfo)
        {
            CgsDev::SimpleStrStream lStream;

            if (meLastSlamImpactSituation == E_IMPACT_SITUATION_INVALID)
            {
                lStream << "Invalid";
            }
            else
            {
                lStream << "Slam " << KAPC_IMPACT_SITUATION_NAMES[meLastSlamImpactSituation] << ", ";
                lStream << "Duration: " << mfLastSlamDuration << "s, ";
                lStream << "(Base duration: " << mfLastSlamBaseDuration << ", ";
                lStream << "Mass factor " << mfLastSlamMassFactor << ", ";
                lStream << "Situation scale: " << KAF_SLAM_SITUATION_SCALE[meLastSlamImpactSituation] << ") ";
            }

            MaybeDrawText(lpRender, lStream.GetBuffer(), KF_X, 50.0f, KF_TEXT_SCALE, KU_COLOUR, false);
        }

        if (mbDrawLastShuntInfo)
        {
            CgsDev::SimpleStrStream lStream;

            if (meLastShuntImpactSituation == E_IMPACT_SITUATION_INVALID)
            {
                lStream << "Invalid";
            }
            else
            {
                lStream << "Shunt " << KAPC_IMPACT_SITUATION_NAMES[meLastShuntImpactSituation] << ", ";
                lStream << "Magnitude: " << mfLastShuntMagnitude;
                lStream << "(Closing speed: " << mfLastShuntClosingSpeed << ")";
            }

            MaybeDrawText(lpRender, lStream.GetBuffer(), KF_X, 80.0f, KF_TEXT_SCALE, KU_COLOUR, false);
        }
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::RenderWorld @0x8261C2B0
    //
    // This is the active component's 3D dispatch. The world-contact-spy branch retains ARTIST's
    // user-controlled guard; its count producer/reset remains unresolved. The traffic-contact
    // renderer also depends on the separately proposed RecordRaceCarTrafficContact producer edge.
    // ========================================================================================
    void VehicleManagerDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpRender)
    {
        if (mbGrindDebugRenderEnabled)
            RenderGrind();

        // ARTIST @0x8261C2F0 checks mbDisplayWorldContactSpies and calls the original reader here.
        // The count producer/reset is unresolved; preserve the stored-count loop without a host-only clamp.
        if (mbDisplayWorldContactSpies)
            RenderWorldContactSpies(lpRender);

        if (mbDrawLastRaceCarTrafficContact)
            DrawLastRaceCarTrafficContact(lpRender);
        if (mbRenderResetLines)
            RenderResetRays(lpRender);
        if (mbDrawLastCrashContact)
            DrawLastCrashContact(lpRender);
        if (mbDrawCatchupTargets)
            DrawCatchupTargets(lpRender);
        if (mbDrawPlayerStuckInCollisionTests)
            DrawPlayerStuckInCollisionLineTests(lpRender);
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::RenderGrind @0x825B6EA8
    // The console keeps the most recent active race-car slot for each of the three grind overlays.
    // ========================================================================================
    void VehicleManagerDebugComponent::RenderGrind() const
    {
        CgsDev::DebugInterface lDebugInterface;
        CgsDev::DebugRender& lrRender = lDebugInterface.Get2dRender();
        const CgsDev::DebugUI::Metrics lMetrics = lDebugInterface.GetUI().GetMetrics();

        f32 lfGrindingDuration = 0.0f;
        f32 lfGroundingDuration = 0.0f;
        f32 lfRubbingDuration = 0.0f;
        u32 luGrindingCar = 0;
        u32 luGroundingCar = 0;
        u32 luRubbingCar = 0;
        const VehicleManager& lrManager = *mpVehicleManager;

        for (s32 liCar = 0; liCar < VehicleManager::KI_MAX_ACTIVE_RACE_CARS; ++liCar)
        {
            if (lrManager.mau8FramesSincePlayerGrindingOther[liCar] < 10)
            {
                lfGrindingDuration = lrManager.mafPlayerGrindingOtherDurationSeconds[liCar];
                luGrindingCar = static_cast<u32>(liCar);
            }
            if (lrManager.mau8FramesSinceOtherGrindingPlayer[liCar] < 10)
            {
                lfGroundingDuration = lrManager.mafOtherGrindingPlayerDurationSeconds[liCar];
                luGroundingCar = static_cast<u32>(liCar);
            }
            if (lrManager.mabRubbingThisUpdate[liCar])
            {
                lfRubbingDuration = lrManager.mafRubbingDurationSeconds[liCar];
                luRubbingCar = static_cast<u32>(liCar);
            }
        }

        char lacText[64];
        const rw::math::vpu::Vector2 lPosition = MakeVector2(lMetrics.mfScreenWidth - 100.0f, 360.0f);
        if (lfGrindingDuration > 0.0f)
        {
            CgsCore::SPrintf(lacText, sizeof(lacText), "Grinding %u: %.1f", luGrindingCar, lfGrindingDuration);
            lrRender.Draw2DTextJustified(lacText, lPosition, CgsDev::DebugRender::E_JUSTIFY_RIGHT,
                                         16.0f, 0xFF00FFFFu);
        }
        if (lfGroundingDuration > 0.0f)
        {
            CgsCore::SPrintf(lacText, sizeof(lacText), "Ground by %u! %.1f", luGroundingCar, lfGroundingDuration);
            lrRender.Draw2DTextJustified(lacText, MakeVector2(lPosition.x, 380.0f),
                                         CgsDev::DebugRender::E_JUSTIFY_RIGHT, 16.0f, 0xFF00FF80u);
        }
        if (lfRubbingDuration > 0.0f)
        {
            CgsCore::SPrintf(lacText, sizeof(lacText), "Rubbing %u: %.1f", luRubbingCar, lfRubbingDuration);
            lrRender.Draw2DTextJustified(lacText, MakeVector2(lPosition.x, 400.0f),
                                         CgsDev::DebugRender::E_JUSTIFY_RIGHT, 16.0f, 0xFF80FF00u);
        }
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::RenderWorldContactSpies @0x825B7170
    // ARTIST reader for the retained OutContactSpy records. The display toggle gates this call;
    // preserve the stored count while the producer/reset owner remains unresolved.
    // ========================================================================================
    namespace
    {
        rw::RGBA WorldDebugColour(u32 luPackedColour)
        {
            rw::RGBA lColour;
            lColour.m_rgba = luPackedColour;
            return lColour;
        }
    }

    void VehicleManagerDebugComponent::RenderWorldContactSpies(
        CgsDev::Debug3DImmediateRender* lpRender) const
    {
        for (s32 liSpy = 0; liSpy < miNumWorldContactSpies; ++liSpy)
        {
            const CgsPhysics::PhysicsSimulationIO::OutContactSpy& lrSpy = maWorldContactSpies[liSpy];
            lpRender->DrawLine(lrSpy.mPointOnA, lrSpy.mPointOnB, WorldDebugColour(0xFFFFFFFFu));
            lpRender->DrawLine(lrSpy.mPointOnA,
                               rw::math::vpu::Add(lrSpy.mPointOnA, lrSpy.mFrictionStress),
                               WorldDebugColour(0xFF00FF00u));
            lpRender->DrawLine(lrSpy.mPointOnA,
                               rw::math::vpu::Add(lrSpy.mPointOnA,
                                                  rw::math::vpu::Mult(lrSpy.mNormalStress, 0.001f)),
                               WorldDebugColour(0xFFFF0000u));
        }
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::RenderResetRays @0x825C9158 (raw ARTIST image words)
    // The player car's front/rear wheel-pair midpoints seed the two original reset rays.
    // ========================================================================================
    void VehicleManagerDebugComponent::RenderResetRays(CgsDev::Debug3DImmediateRender* lpRender) const
    {
        const VehicleManager& lrManager = *mpVehicleManager;
        const s32 liPlayerIndex = static_cast<s32>(lrManager.mePlayerActiveRaceCarIndex);
        const RaceCarPhysics& lrCar = lrManager.maRaceCarVehicles[liPlayerIndex];
        const Matrix44Affine& lrTransform = lrCar.GetTransform();

        const Vector3& lrFrontRight = lrCar.GetWheel(eFrontRightWheel).GetPosition();
        const Vector3& lrFrontLeft = lrCar.GetWheel(eFrontLeftWheel).GetPosition();
        const Vector3& lrRearRight = lrCar.GetWheel(eRearRightWheel).GetPosition();
        const Vector3& lrRearLeft = lrCar.GetWheel(eRearLeftWheel).GetPosition();
        CGS_ASSERT((lrFrontRight.x == lrFrontRight.x) && (lrFrontRight.y == lrFrontRight.y)
                       && (lrFrontRight.z == lrFrontRight.z),
                   "Invalid wheel position: , please tell Graham D."); // Wheel.h:412
        CGS_ASSERT((lrFrontLeft.x == lrFrontLeft.x) && (lrFrontLeft.y == lrFrontLeft.y)
                       && (lrFrontLeft.z == lrFrontLeft.z),
                   "Invalid wheel position: , please tell Graham D."); // Wheel.h:412
        CGS_ASSERT((lrRearRight.x == lrRearRight.x) && (lrRearRight.y == lrRearRight.y)
                       && (lrRearRight.z == lrRearRight.z),
                   "Invalid wheel position: , please tell Graham D."); // Wheel.h:412
        CGS_ASSERT((lrRearLeft.x == lrRearLeft.x) && (lrRearLeft.y == lrRearLeft.y)
                       && (lrRearLeft.z == lrRearLeft.z),
                   "Invalid wheel position: , please tell Graham D."); // Wheel.h:412

        const Vector3 lvFrontMidpointCarSpace{
            (lrFrontRight.x + lrFrontLeft.x) * 0.5f,
            (lrFrontRight.y + lrFrontLeft.y) * 0.5f,
            (lrFrontRight.z + lrFrontLeft.z) * 0.5f,
            (lrFrontRight.w + lrFrontLeft.w) * 0.5f };
        const Vector3 lvRearMidpointCarSpace{
            (lrRearRight.x + lrRearLeft.x) * 0.5f,
            (lrRearRight.y + lrRearLeft.y) * 0.5f,
            (lrRearRight.z + lrRearLeft.z) * 0.5f,
            (lrRearRight.w + lrRearLeft.w) * 0.5f };

        const Vector3 lvFrontStart{
            lrTransform.wAxis.x + lrTransform.xAxis.x * lvFrontMidpointCarSpace.x
                + lrTransform.yAxis.x * lvFrontMidpointCarSpace.y + lrTransform.zAxis.x * lvFrontMidpointCarSpace.z,
            lrTransform.wAxis.y + lrTransform.xAxis.y * lvFrontMidpointCarSpace.x
                + lrTransform.yAxis.y * lvFrontMidpointCarSpace.y + lrTransform.zAxis.y * lvFrontMidpointCarSpace.z,
            lrTransform.wAxis.z + lrTransform.xAxis.z * lvFrontMidpointCarSpace.x
                + lrTransform.yAxis.z * lvFrontMidpointCarSpace.y + lrTransform.zAxis.z * lvFrontMidpointCarSpace.z,
            lrTransform.wAxis.w + lrTransform.xAxis.w * lvFrontMidpointCarSpace.x
                + lrTransform.yAxis.w * lvFrontMidpointCarSpace.y + lrTransform.zAxis.w * lvFrontMidpointCarSpace.z };
        const Vector3 lvRearStart{
            lrTransform.wAxis.x + lrTransform.xAxis.x * lvRearMidpointCarSpace.x
                + lrTransform.yAxis.x * lvRearMidpointCarSpace.y + lrTransform.zAxis.x * lvRearMidpointCarSpace.z,
            lrTransform.wAxis.y + lrTransform.xAxis.y * lvRearMidpointCarSpace.x
                + lrTransform.yAxis.y * lvRearMidpointCarSpace.y + lrTransform.zAxis.y * lvRearMidpointCarSpace.z,
            lrTransform.wAxis.z + lrTransform.xAxis.z * lvRearMidpointCarSpace.x
                + lrTransform.yAxis.z * lvRearMidpointCarSpace.y + lrTransform.zAxis.z * lvRearMidpointCarSpace.z,
            lrTransform.wAxis.w + lrTransform.xAxis.w * lvRearMidpointCarSpace.x
                + lrTransform.yAxis.w * lvRearMidpointCarSpace.y + lrTransform.zAxis.w * lvRearMidpointCarSpace.z };

        const Vector3 lvFrontEnd = lvFrontStart + lrTransform.zAxis * lrManager.mfFrontRayLength;
        const Vector3 lvRearEnd = lvRearStart - lrTransform.zAxis * lrManager.mfRearRayLength;
        lpRender->DrawLine(lvFrontStart, lvFrontEnd, WorldDebugColour(0xFFFFFFFFu));
        lpRender->DrawLine(lvRearStart, lvRearEnd, WorldDebugColour(0xFFFFFFFFu));
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::DrawLastRaceCarTrafficContact @0x825B74A0
    // ========================================================================================
    void VehicleManagerDebugComponent::DrawLastRaceCarTrafficContact(
        CgsDev::Debug3DImmediateRender* lpRender)
    {
        CGS_ASSERT(lpRender != nullptr, "lpDisplay != NULL"); // :934

        const Vector3 lvRaceCarHalfExtent = mLastTrafficContact_RaceCarHalfExt;
        const Vector3 lvTrafficHalfExtent = mLastTrafficContact_TrafficHalfExt;
        lpRender->DrawBox(Vector3{ -lvRaceCarHalfExtent.x, -lvRaceCarHalfExtent.y,
                                   -lvRaceCarHalfExtent.z, -lvRaceCarHalfExtent.w },
                          lvRaceCarHalfExtent, mLastTrafficContact_RaceCarTransform, WorldDebugColour(0xFF0000FFu));
        lpRender->DrawBox(Vector3{ -lvTrafficHalfExtent.x, -lvTrafficHalfExtent.y,
                                   -lvTrafficHalfExtent.z, -lvTrafficHalfExtent.w },
                          lvTrafficHalfExtent, mLastTrafficContact_TrafficTransform, WorldDebugColour(0xFF00FF00u));

        const CgsSceneManager::SceneManagerIO::PotentialContact& lrContact = mLastTrafficContact_Contact;
        const Vector3 lvContactNormalEnd = lrContact.mPointOnB + lrContact.mNormal;
        lpRender->DrawArrow(lrContact.mPointOnB, lvContactNormalEnd, WorldDebugColour(0xFF00FFFFu));

        const Vector3 lvRaceCarVelocityOffset =
            rw::math::vpu::Mult(mLastTrafficContact_RaceCarVelocity, 0.1f);
        lpRender->DrawArrow(mLastTrafficContact_RaceCarTransform.wAxis,
                            mLastTrafficContact_RaceCarTransform.wAxis + lvRaceCarVelocityOffset,
                            WorldDebugColour(0xFF0000FFu));
        const Vector3 lvTrafficVelocityOffset =
            rw::math::vpu::Mult(mLastTrafficContact_TrafficVelocity, 0.1f);
        lpRender->DrawArrow(mLastTrafficContact_TrafficTransform.wAxis,
                            mLastTrafficContact_TrafficTransform.wAxis + lvTrafficVelocityOffset,
                            WorldDebugColour(0xFF00FF00u));

        const Vector3 lvWorldUp{ 0.0f, 1.0f, 0.0f, 0.0f };
        const Vector3 lvActualTextPosition = lvContactNormalEnd - lvWorldUp * 0.1f;
        lpRender->DrawText(lvActualTextPosition, "actual", 17.0f);
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::DrawLastCrashContact @0x825B7758
    // ========================================================================================
    void VehicleManagerDebugComponent::DrawLastCrashContact(CgsDev::Debug3DImmediateRender* lpRender)
    {
        CGS_ASSERT(lpRender != nullptr, "lpDisplay != NULL"); // :1006

        const Vector3 lvHalfExtent = mLastCrashContact_RaceCarHalfExt;
        lpRender->DrawBox(Vector3{ -lvHalfExtent.x, -lvHalfExtent.y,
                                   -lvHalfExtent.z, -lvHalfExtent.w },
                          lvHalfExtent, mLastCrashContact_RaceCarTransform, WorldDebugColour(0xFF0000FFu));

        const CgsSceneManager::SceneManagerIO::PotentialContact& lrContact = mLastCrashContact_Contact;
        lpRender->DrawArrow(lrContact.mPointOnB, lrContact.mPointOnB + lrContact.mNormal, WorldDebugColour(0xFF00FFFFu));

        const Vector3 lvVelocityOffset = rw::math::vpu::Mult(mLastCrashContact_RaceCarVelocity, 0.1f);
        lpRender->DrawArrow(mLastCrashContact_RaceCarTransform.wAxis,
                            mLastCrashContact_RaceCarTransform.wAxis + lvVelocityOffset,
                            WorldDebugColour(0xFF0000FFu));
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::DrawCatchupTargets @0x825EBF30
    // ========================================================================================
    void VehicleManagerDebugComponent::DrawCatchupTargets(CgsDev::Debug3DImmediateRender* lpRender)
    {
        CGS_ASSERT(lpRender != nullptr, "lpDisplay != NULL"); // :1093

        const VehicleManager& lrManager = *mpVehicleManager;
        for (s32 liCar = lrManager.mUsedRaceCars.GetFirstNonZeroBit(); liCar >= 0;
             liCar = lrManager.mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            const VehicleDriver& lrDriver = lrManager.maRaceCarDrivers[liCar];
            if (lrDriver.GetNumInterpSteps() <= 0)
                continue;

            const RaceCarPhysics& lrCar = lrManager.maRaceCarVehicles[liCar];
            Matrix44Affine lTargetTransform = lrDriver.GetCatchupTargetTransform();
            const Vector3 lvCOMOffset = lrCar.GetSimpleAttribs()->mCOMOffset;
            const Vector3 lvTargetCOMOffset =
                lTargetTransform.zAxis * lvCOMOffset.z
                + lTargetTransform.xAxis * lvCOMOffset.x
                + lTargetTransform.yAxis * lvCOMOffset.y;
            lTargetTransform.wAxis = lTargetTransform.wAxis - lvTargetCOMOffset;

            const Vector3 lvHalfExtent = lrCar.GetHalfExtent();
            const Vector3 lvBoxMinimum{ -lvHalfExtent.x, -lvHalfExtent.y,
                                        -lvHalfExtent.z, -lvHalfExtent.w };
            const Vector3 lvBoxMaximum = lvHalfExtent;
            lpRender->DrawBox(lvBoxMinimum, lvBoxMaximum, lTargetTransform, WorldDebugColour(0xFFFFFFFFu));

            const Vector3 lvCurrentPosition = lrCar.GetGraphicsVehicleTransform().wAxis;
            lpRender->DrawArrow(lvCurrentPosition, lTargetTransform.wAxis, WorldDebugColour(0xFFFFFFFFu));

            const Vector3 lvWorldUp{ 0.0f, 1.0f, 0.0f, 0.0f };
            const Vector3 lvTextPosition = lTargetTransform.wAxis + lvWorldUp * 1.5f;
            lpRender->DrawText(lvTextPosition, lrCar.IsFrozen() ? "Frozen" : "Not frozen", 20.0f);
        }
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::DrawPlayerStuckInCollisionLineTests @0x825C9C78
    // ========================================================================================
    void VehicleManagerDebugComponent::DrawPlayerStuckInCollisionLineTests(
        CgsDev::Debug3DImmediateRender* lpRender)
    {
        CGS_ASSERT(lpRender != nullptr, "lpDisplay != NULL"); // :782

        const VehicleManager& lrManager = *mpVehicleManager;
        const s32 liPlayerIndex = static_cast<s32>(lrManager.mePlayerActiveRaceCarIndex);
        if (liPlayerIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
            return;

        const u32 luPlayerIndex = static_cast<u32>(liPlayerIndex);
        CGS_ASSERT(luPlayerIndex < VehicleManager::KI_MAX_ACTIVE_RACE_CARS, "invalid index : "); // CgsBitArray.h:203
        if (!lrManager.mUsedRaceCars.IsBitSet(luPlayerIndex))
            return;

        Vector3 lvCacheSphere;
        std::memcpy(&lvCacheSphere, lrManager.mStuckInCollisionTestCacheSphere, sizeof(lvCacheSphere));
        const Vector3 lvSphereCentre{ lvCacheSphere.x, lvCacheSphere.y, lvCacheSphere.z, 0.0f };
        lpRender->DrawHollowSphere(lvSphereCentre, lvCacheSphere.w, WorldDebugColour(0xFFFFFFFFu));

        for (s32 liLine = 0; liLine < KI_MAX_WORLD_CONTACT_SPIES; ++liLine)
        {
            lpRender->DrawLine(maStuckInCollisionLineTestStart[liLine],
                               maStuckInCollisionLineTestEnd[liLine], WorldDebugColour(0xFFFFFFFFu));
            if (mabStuckInCollisionIntersection[liLine])
            {
                lpRender->DrawSphere(maStuckInCollisionLineTestPoint[liLine], 0.15f, WorldDebugColour(0xFFFFFFFFu));
            }
        }

        const RaceCarPhysics& lrPlayerCar = lrManager.maRaceCarVehicles[liPlayerIndex];
        lpRender->DrawAxis(lrPlayerCar.GetGraphicsVehicleTransform());
    }

    void VehicleManagerDebugComponent::RenderContact(CgsDev::Debug2DImmediateRender* lpRender) const
    {
        CgsDev::DebugInterface lDebugInterface;
        const CgsDev::DebugUI::Metrics lMetrics = lDebugInterface.GetUI().GetMetrics();
        const f32 lfScreenWidth = lMetrics.mfScreenWidth;

        rw::math::vpu::Vector2 lv2Position = MakeVector2(lfScreenWidth - 80.0f, 60.0f);
        RenderCarDiagram(lpRender, lv2Position, 0xFFFF0000u,
                         mbDisplayContact ? &mPlayerCarContactPosition : nullptr,
                         mbDisplayContact ? &mClosingVelocityPlayerCarSpace : nullptr,
                         mbDisplayContact ? &mPlayerCarVelocity : nullptr);

        lv2Position.y += 120.0f;
        RenderCarDiagram(lpRender, lv2Position, 0xFF00FF00u,
                         mbDisplayContact ? &mOtherCarContactPosition : nullptr,
                         mbDisplayContact ? &mClosingVelocityOtherCarSpace : nullptr,
                         mbDisplayContact ? &mOtherCarVelocity : nullptr);

        if (mbDisplayContact)
        {
            char lacAngleText[64];
            CgsCore::SPrintf(lacAngleText, sizeof(lacAngleText), "%d deg",
                             static_cast<s32>(mfOtherCarContactAngleRad * KF_RADIANS_TO_DEGREES));
            lpRender->DrawText(lacAngleText, MakeVector2(lfScreenWidth - 100.0f, 330.0f), 16.0f,
                               0xFF0080FFu, false);
        }
    }

    // DrawBox takes (position, size): the body is a 40x80 box at lrPosition and the wheels are
    // 5x20 boxes just outside its corners. The contact point maps the car-space x / -z in
    // [-0.5, 0.5] across the wheel-to-wheel span (45 x 40); each velocity line runs from
    // centre - (v.x, v.z) to the centre.
    void VehicleManagerDebugComponent::RenderCarDiagram(CgsDev::Debug2DImmediateRender* lpRender,
                                                        const rw::math::vpu::Vector2& lrPosition,
                                                        CgsDev::RGBA lColour,
                                                        const rw::math::vpu::Vector3* lpContactPosition,
                                                        const rw::math::vpu::Vector3* lpClosingVelocity,
                                                        const rw::math::vpu::Vector3* lpCarVelocity) const
    {
        static const f32 KF_CAR_WIDTH      = 40.0f;
        static const f32 KF_CAR_LENGTH     = 80.0f;
        static const f32 KF_WHEEL_WIDTH    = 5.0f;
        static const f32 KF_WHEEL_LENGTH   = 20.0f;
        static const f32 KF_WHEEL_FRONT    = 10.0f;
        static const f32 KF_WHEEL_REAR     = 30.0f;
        static const f32 KF_VELOCITY_SCALE = 1.0f;
        static const u32 KU_WHEEL_COLOUR   = 0xFF000000u;

        const rw::math::vpu::Vector2 lv2WheelSize = MakeVector2(KF_WHEEL_WIDTH, KF_WHEEL_LENGTH);

        const f32 lfLeftWheelX  = lrPosition.x - KF_WHEEL_WIDTH;
        const f32 lfFrontWheelY = lrPosition.y + KF_WHEEL_FRONT;
        const f32 lfRightWheelX = lfLeftWheelX + KF_CAR_WIDTH + KF_WHEEL_WIDTH;
        const f32 lfRearWheelY  = lrPosition.y + KF_CAR_LENGTH - KF_WHEEL_REAR;

        lpRender->DrawBox(lrPosition, MakeVector2(KF_CAR_WIDTH, KF_CAR_LENGTH), lColour);
        lpRender->DrawBox(MakeVector2(lfLeftWheelX, lfFrontWheelY), lv2WheelSize, KU_WHEEL_COLOUR);
        lpRender->DrawBox(MakeVector2(lfRightWheelX, lfFrontWheelY), lv2WheelSize, KU_WHEEL_COLOUR);
        lpRender->DrawBox(MakeVector2(lfLeftWheelX, lfRearWheelY), lv2WheelSize, KU_WHEEL_COLOUR);
        lpRender->DrawBox(MakeVector2(lfRightWheelX, lfRearWheelY), lv2WheelSize, KU_WHEEL_COLOUR);

        if (lpContactPosition != nullptr)
        {
            const f32 lfContactX = (lpContactPosition->x + 0.5f) * (lfRightWheelX - lfLeftWheelX) + lfLeftWheelX;
            const f32 lfContactY = (-lpContactPosition->z + 0.5f) * (lfRearWheelY - lfFrontWheelY) + lfFrontWheelY;

            lpRender->DrawBox(MakeVector2(lfContactX, lfContactY), lv2WheelSize, 0xFF0000FFu);
        }

        if (lpClosingVelocity != nullptr)
        {
            CGS_ASSERT(lpCarVelocity != NULL, "lpCarVelocity != NULL");

            const rw::math::vpu::Vector2 lv2Centre =
                MakeVector2(lrPosition.x + KF_CAR_WIDTH * 0.5f, lrPosition.y + KF_CAR_LENGTH * 0.5f);

            lpRender->DrawLine(MakeVector2(lv2Centre.x - lpClosingVelocity->x * KF_VELOCITY_SCALE,
                                           lv2Centre.y - lpClosingVelocity->z * KF_VELOCITY_SCALE),
                               lv2Centre, 0xFFFFFF00u);
            lpRender->DrawLine(MakeVector2(lv2Centre.x - lpCarVelocity->x * KF_VELOCITY_SCALE,
                                           lv2Centre.y - lpCarVelocity->z * KF_VELOCITY_SCALE),
                               lv2Centre, 0xFF00FFFFu);
        }
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::RecordRaceCarTrafficContact @ 0x825B7270 (140 instructions)
    //
    // The X360 caller is HandleRaceCarTrafficCarPotentialContact @0x8263FA50, call site
    // 0x826401E4. It passes the current PotentialContact, race-car slot, and physical traffic slot.
    // Identical ordered pairs are not recorded twice; a reversed duplicate fires the original
    // assert and then still refreshes the snapshot. This is debug state only.
    // ========================================================================================
    void VehicleManagerDebugComponent::RecordRaceCarTrafficContact(
        const CgsSceneManager::SceneManagerIO::PotentialContact* lpPotentialContact,
        s32 liRaceCarIndex, s32 liTrafficIndex)
    {

        const u32 luEntityIdA = static_cast<u32>(lpPotentialContact->muVolumeInstanceIdA.muId >> 32);
        const u32 luEntityIdB = static_cast<u32>(lpPotentialContact->muVolumeInstanceIdB.muId >> 32);
        CGS_ASSERT((luEntityIdA >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
                   "lEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"); // :845
        CGS_ASSERT((luEntityIdB >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE),
                   "lEntityIdB.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE"); // :846

        const u32 luLastEntityIdA =
            static_cast<u32>(mLastTrafficContact_Contact.muVolumeInstanceIdA.muId >> 32);
        const u32 luLastEntityIdB =
            static_cast<u32>(mLastTrafficContact_Contact.muVolumeInstanceIdB.muId >> 32);
        if (luEntityIdA != luLastEntityIdA || luEntityIdB != luLastEntityIdB)
        {
            CGS_ASSERT(luEntityIdB != luLastEntityIdA || luEntityIdA != luLastEntityIdB,
                       "lEntityIdB != mLastTrafficContact_Contact.muVolumeInstanceIdA.GetEntityId() || "
                       "lEntityIdA != mLastTrafficContact_Contact.muVolumeInstanceIdB.GetEntityId()"); // :854

            const RaceCarPhysics& lrRaceCar = mpVehicleManager->maRaceCarVehicles[liRaceCarIndex];
            const PhysicalTrafficVehicle* const lpTrafficVehicle =
                mpVehicleManager->mPhysicalTrafficManager.GetTrafficVehicle(liTrafficIndex);
            const SimpleVehiclePhysics* const lpTrafficBody = lpTrafficVehicle->mpVehicleBody;

            mLastTrafficContact_RaceCarTransform = lrRaceCar.GetTransform();
            mLastTrafficContact_TrafficTransform = lpTrafficBody->GetTransform();

            const CgsGeometric::AxisAlignedBox& lrRaceCarAABB = lrRaceCar.GetDeformableAABB();
            mLastTrafficContact_RaceCarHalfExt = Vector3{
                (lrRaceCarAABB.mMax.x - lrRaceCarAABB.mMin.x) * 0.5f,
                (lrRaceCarAABB.mMax.y - lrRaceCarAABB.mMin.y) * 0.5f,
                (lrRaceCarAABB.mMax.z - lrRaceCarAABB.mMin.z) * 0.5f,
                (lrRaceCarAABB.mMax.w - lrRaceCarAABB.mMin.w) * 0.5f };
            const CgsGeometric::AxisAlignedBox& lrTrafficAABB = lpTrafficBody->GetDeformableAABB();
            mLastTrafficContact_TrafficHalfExt = Vector3{
                (lrTrafficAABB.mMax.x - lrTrafficAABB.mMin.x) * 0.5f,
                (lrTrafficAABB.mMax.y - lrTrafficAABB.mMin.y) * 0.5f,
                (lrTrafficAABB.mMax.z - lrTrafficAABB.mMin.z) * 0.5f,
                (lrTrafficAABB.mMax.w - lrTrafficAABB.mMin.w) * 0.5f };

            mLastTrafficContact_RaceCarVelocity = lrRaceCar.GetLinearVelocity();
            mLastTrafficContact_TrafficVelocity = lpTrafficBody->GetLinearVelocity();
            mLastTrafficContact_Contact = *lpPotentialContact;
        }
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::RecordStuckInCollisionLineTestResult @0x825B7A48 (34 instructions)
    // ========================================================================================
    void VehicleManagerDebugComponent::RecordStuckInCollisionLineTestResult(
        const CgsSceneManager::CgsCollision::LineWithTriangleListStreamJobDesc::StreamResult* lpResult)
    {
        CGS_ASSERT(lpResult != nullptr, "lpResult != NULL"); // :1197
        for (s32 liLine = 0; liLine < 4; ++liLine) // X360 loop bound 4; the StreamResult has five slots
        {
            if (lpResult->mabHit[liLine] != 0)
            {
                mabStuckInCollisionIntersection[liLine] = true;
                const Vector4& lrHit = lpResult->maHitPosition[liLine];
                Vector3& lrPoint = maStuckInCollisionLineTestPoint[liLine];
                lrPoint.x = lrHit.x;
                lrPoint.y = lrHit.y;
                lrPoint.z = lrHit.z;
                lrPoint.w = lrHit.w;
            }
        }
    }

    // ========================================================================================
    // VehicleManagerDebugComponent::RecordCrashContact   @ 0x825B7880   (114 instructions)
    //
    // AN EXPORT HOLE -- decoded from the raw image words (tools/re/x360rd.py 825B7880 456); the
    // next export starts at 0x825B7A48 and the body's `b __restgprlr_26` sits at 0x825B7A44.
    // Word-by-word:
    //   825B788C  mr r29, r4 ; mr r31, r3            lpPotentialContact / this
    //   825B7898  cmplwi r29, 0 -> assert :1045       "lpPotentialContact != NULL"
    //   825B78C0  ld 0x30 / ld 0x38 ; srdi 32          the two entity words (r28 = A, r27 = B)
    //   825B78D8  srwi r28, 24 ; cmplwi 1 -> :1053     "lEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"
    //   825B7900  srwi r27, 24 ; cmplwi 0 ; bne RET    B not WORLD -> silent return (a test, not an assert)
    //   825B7908  rlwinm r10, r28, 22,18,31            idx = (A >> 10) & 0x3FFF
    //   825B7914  lwz r11, 0x1D8(this)                 mpVehicleManager
    //   825B791C  lwzx r9, r11, 0x2A0AC ; cmpw ; bne   idx != mePlayerActiveRaceCarIndex -> return
    //   825B7928  mulli r30, r10, 0x1460 ; lbz 0xE50   maRaceCarVehicles[idx].mbCrashing -> return if set
    //   825B793C  ld 0x400 / ld 0x408 (this)           mLastCrashContact_Contact.muVolumeInstanceId{A,B}
    //             cmplw B, last.A ; cmplw A, last.B -> assert :1065 (fires only if BOTH match)
    //   825B79A8  addi r10, rec, 0x10 ; 4 x lvx128/stvx128 -> +0x370  (mTransform -> _RaceCarTransform)
    //   825B79F4  lvx128 rec+0x6D0 / rec+0x6E0 ; vsubfp ; vrefp(2.0) + 2 NR ; vmulfp128 ; stvx128 +0x3B0
    //             (max - min) / 2  -> _RaceCarHalfExt   (vcfsx v0,2 / vcfsx v13,1 are the 2.0 and 1.0)
    //   825B7A18  mtctr 10 ; ld/std x10                 80-byte contact -> +0x3D0 (_Contact)
    //   825B7A38  lvx128 rec+0x50 ; stvx128 +0x3C0      mLinearVelocity -> _RaceCarVelocity
    // The friend grant in BrnVehicleManager.h carries the two bare manager reads.
    // ========================================================================================
    void VehicleManagerDebugComponent::RecordCrashContact(
        const CgsSceneManager::SceneManagerIO::PotentialContact* lpPotentialContact)
    {
        CGS_ASSERT(lpPotentialContact != 0, "lpPotentialContact != NULL");   // :1045

        const u32 luEntityIdA = static_cast<u32>(lpPotentialContact->muVolumeInstanceIdA.muId >> 32);   // r28
        const u32 luEntityIdB = static_cast<u32>(lpPotentialContact->muVolumeInstanceIdB.muId >> 32);   // r27

        CGS_ASSERT((luEntityIdA >> 24) == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
                   "lEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");   // :1053

        // 0x825B7904: a race-car-vs-non-world contact is simply not recorded here.
        if ((luEntityIdB >> 24) != static_cast<u32>(BrnWorld::E_ENTITYTYPE_WORLD))
            return;

        // 0x825B7908..0x825B7938: the player's slot only, and only before it is crashing.
        const u32 luRaceCarIndex = (luEntityIdA >> 10) & 0x3FFFu;
        if (luRaceCarIndex != static_cast<u32>(mpVehicleManager->mePlayerActiveRaceCarIndex))
            return;

        const RaceCarPhysics& lrRaceCar = mpVehicleManager->maRaceCarVehicles[luRaceCarIndex];
        if (lrRaceCar.IsCrashing())
            return;

        // 0x825B793C..0x825B797C (:1065) -- the same pair, mirrored, must not already be the record.
        CGS_ASSERT(luEntityIdB != static_cast<u32>(mLastCrashContact_Contact.muVolumeInstanceIdA.muId >> 32)
                   || luEntityIdA != static_cast<u32>(mLastCrashContact_Contact.muVolumeInstanceIdB.muId >> 32),
                   "lEntityIdB != mLastCrashContact_Contact.muVolumeInstanceIdA.GetEntityId() || "
                   "lEntityIdA != mLastCrashContact_Contact.muVolumeInstanceIdB.GetEntityId()");

        mLastCrashContact_RaceCarTransform = lrRaceCar.GetTransform();                     // rec +0x10 -> +0x370

        const CgsGeometric::AxisAlignedBox& lrBox = lrRaceCar.GetDeformableAABB();         // rec +0x6D0 / +0x6E0
        mLastCrashContact_RaceCarHalfExt = Vector3{ (lrBox.mMax.x - lrBox.mMin.x) / 2.0f,   // -> +0x3B0
                                                    (lrBox.mMax.y - lrBox.mMin.y) / 2.0f,
                                                    (lrBox.mMax.z - lrBox.mMin.z) / 2.0f,
                                                    (lrBox.mMax.w - lrBox.mMin.w) / 2.0f };

        mLastCrashContact_Contact         = *lpPotentialContact;                           // 80 bytes -> +0x3D0
        mLastCrashContact_RaceCarVelocity = lrRaceCar.GetLinearVelocity();                 // rec +0x50 -> +0x3C0
    }

    // ========================================================================================
    // Layout pins. Never called.
    //
    // These are written as RELATIVE deltas, not absolute offsets, because two members widen
    // on x64 (the base's vtable pointer and mpVehicleManager) and shift everything after them.
    // Every delta below IS X360-identical, and every number is asm-literal:
    //   * the head deltas are the byte gaps between the asm's own store targets;
    //   * from mPlayerCarContactPosition onwards there is not a single pointer left in the
    //     class, so that whole tail's offsets are identical on both ABIs and are pinned against
    //     one anchor (KI_HEAD == offsetof(mPlayerCarContactPosition), 480 on X360).
    // ========================================================================================
    void VehicleManagerDebugComponent::_AssertLayout()
    {
        typedef VehicleManagerDebugComponent V;

        // ---- the spy array: 4 elements at the X360-attested 112-byte stride --------------------
        // (stride from BaseEventQueue<OutContactSpy>::AddEvent @0x825E44C8 / AddEventSafe
        // @0x828A1B78; 16 + 4*112 == 464 == miNumWorldContactSpies is the closure.)
        static_assert(sizeof(CgsPhysics::PhysicsSimulationIO::OutContactSpy) == 112,
                      "OutContactSpy stride (X360-attested 112)");
        static_assert(offsetof(V, miNumWorldContactSpies) - offsetof(V, maWorldContactSpies) == 4 * 112,
                      "maWorldContactSpies[4] fills 448 bytes and ends at miNumWorldContactSpies");
        static_assert(offsetof(V, mbDisplayWorldContactSpies) - offsetof(V, miNumWorldContactSpies) == 4,
                      "mbDisplayWorldContactSpies (asm stb 0x1D4) is one word past miNumWorldContactSpies");
        static_assert(offsetof(V, mpVehicleManager) - offsetof(V, mbDisplayWorldContactSpies) == 4,
                      "mpVehicleManager (asm stw r29, 0x1D8) is one word past the display flag");
        static_assert(offsetof(V, mbSlamDebugRenderEnabled) - offsetof(V, mpVehicleManager) == sizeof(void*),
                      "mbSlamDebugRenderEnabled (asm stb 0x1DC) sits immediately after the manager pointer");
        static_assert(offsetof(V, mbRenderResetLines)        - offsetof(V, mbSlamDebugRenderEnabled) == 1,
                      "mbRenderResetLines (asm stb 0x1DD)");
        static_assert(offsetof(V, mbGrindDebugRenderEnabled) - offsetof(V, mbSlamDebugRenderEnabled) == 2,
                      "mbGrindDebugRenderEnabled (asm stb 0x1DE)");

        // ---- the pointer-free tail, anchored at mPlayerCarContactPosition (X360 +480) ----------
        const size_t KI_HEAD = offsetof(V, mPlayerCarContactPosition);
        // The head->tail bridge is the ONE delta that is not ABI-invariant: on X360 it is
        // 480 - 476 == 4, but on x64 the widened base vptr + mpVehicleManager push the three
        // gates to a different residue and the 16-aligned Vector3 lands 16 bytes later. What IS
        // invariant -- and what the X360 +480 attests -- is that the contact readout opens a
        // fresh 16-byte lane immediately after the three gate bytes.
        static_assert(KI_HEAD % 16 == 0,
                      "mPlayerCarContactPosition opens a 16-byte lane (X360 +480)");
        static_assert(KI_HEAD - offsetof(V, mbGrindDebugRenderEnabled) <= 16,
                      "nothing sits between the third gate byte and the contact readout");

        static_assert(offsetof(V, mOtherCarContactPosition)       - KI_HEAD ==  16,  "X360 +496");
        static_assert(offsetof(V, mbDisplayContact)               - KI_HEAD ==  32,  "X360 +512");
        static_assert(offsetof(V, mfOtherCarContactAngleRad)      - KI_HEAD ==  36,  "X360 +516");
        static_assert(offsetof(V, mClosingVelocityPlayerCarSpace) - KI_HEAD ==  48,  "X360 +528");
        static_assert(offsetof(V, mClosingVelocityOtherCarSpace)  - KI_HEAD ==  64,  "X360 +544");
        static_assert(offsetof(V, mPlayerCarVelocity)             - KI_HEAD ==  80,  "X360 +560");
        static_assert(offsetof(V, mOtherCarVelocity)              - KI_HEAD ==  96,  "X360 +576");

        static_assert(offsetof(V, mbDrawLastRaceCarTrafficContact)  - KI_HEAD == 112, "X360 +592 (asm stb 0x250)");
        static_assert(offsetof(V, mbDrawLastSlamInfo)               - KI_HEAD == 113, "X360 +593 (asm stb 0x251)");
        static_assert(offsetof(V, mbDrawLastShuntInfo)              - KI_HEAD == 114, "X360 +594 (asm stb 0x252)");
        static_assert(offsetof(V, mbDrawLastCrashContact)           - KI_HEAD == 115, "X360 +595 (asm stb 0x253)");
        static_assert(offsetof(V, mbDrawCatchupTargets)             - KI_HEAD == 116, "X360 +596 (asm stb 0x254)");
        static_assert(offsetof(V, mbRenderWallContacts)             - KI_HEAD == 117, "X360 +597 (asm stb 0x255)");
        static_assert(offsetof(V, mbRenderGroundContacts)           - KI_HEAD == 118, "X360 +598 (asm stb 0x256)");
        static_assert(offsetof(V, mbDrawPlayerStuckInCollisionTests)- KI_HEAD == 119, "X360 +599 (asm stb 0x257)");

        static_assert(offsetof(V, mLastTrafficContact_RaceCarTransform) - KI_HEAD == 128, "X360 +608 (asm r9  = this+0x260)");
        static_assert(offsetof(V, mLastTrafficContact_TrafficTransform) - KI_HEAD == 192, "X360 +672 (asm r8  = this+0x2A0)");
        static_assert(offsetof(V, mLastTrafficContact_RaceCarHalfExt)   - KI_HEAD == 256, "X360 +736 (asm r26 = 0x2E0)");
        static_assert(offsetof(V, mLastTrafficContact_TrafficHalfExt)   - KI_HEAD == 272, "X360 +752 (asm r26 = 0x2F0)");
        static_assert(offsetof(V, mLastTrafficContact_RaceCarVelocity)  - KI_HEAD == 288, "X360 +768 (not written by Construct)");
        static_assert(offsetof(V, mLastTrafficContact_TrafficVelocity)  - KI_HEAD == 304, "X360 +784 (not written by Construct)");
        static_assert(offsetof(V, mLastTrafficContact_Contact)          - KI_HEAD == 320, "X360 +800 (asm r10 = this+0x320)");

        // Two independent witnesses that the embedded PotentialContact is 80 bytes: 800 -> 880 and
        // 976 -> 1056. That is the committed 76-byte record padded to its alignas(16).
        static_assert(sizeof(CgsSceneManager::SceneManagerIO::PotentialContact) == 80,
                      "PotentialContact: 76 bytes at align 16");
        static_assert(offsetof(V, mLastCrashContact_RaceCarTransform) - offsetof(V, mLastTrafficContact_Contact) == 80,
                      "800 + sizeof(PotentialContact) == 880");
        static_assert(offsetof(V, meLastSlamImpactSituation)          - offsetof(V, mLastCrashContact_Contact)  == 80,
                      "976 + sizeof(PotentialContact) == 1056");

        static_assert(offsetof(V, mLastCrashContact_RaceCarTransform) - KI_HEAD == 400, "X360 +880 (asm r10 = this+0x370)");
        static_assert(offsetof(V, mLastCrashContact_RaceCarHalfExt)   - KI_HEAD == 464, "X360 +944 (asm r28 = 0x3B0)");
        static_assert(offsetof(V, mLastCrashContact_RaceCarVelocity)  - KI_HEAD == 480, "X360 +960 (asm r27 = 0x3C0)");
        static_assert(offsetof(V, mLastCrashContact_Contact)          - KI_HEAD == 496, "X360 +976 (asm r10 = this+0x3D0)");

        static_assert(offsetof(V, meLastSlamImpactSituation)  - KI_HEAD == 576, "X360 +1056 (asm stw -1, 0x420)");
        static_assert(offsetof(V, miLastSlamNumber)           - KI_HEAD == 580, "X360 +1060 (asm stw 0,  0x424)");
        static_assert(offsetof(V, mfLastSlamDuration)         - KI_HEAD == 584, "X360 +1064 (asm stfs 0, 0x428)");
        static_assert(offsetof(V, mfLastSlamBaseDuration)     - KI_HEAD == 588, "X360 +1068 (asm stfs 0, 0x42C)");
        static_assert(offsetof(V, mfLastSlamMassFactor)       - KI_HEAD == 592, "X360 +1072 (asm stfs 0, 0x430)");
        static_assert(offsetof(V, meLastShuntImpactSituation) - KI_HEAD == 596, "X360 +1076 (asm stw -1, 0x434)");
        static_assert(offsetof(V, mfLastShuntMagnitude)       - KI_HEAD == 600, "X360 +1080 (asm stfs 0, 0x438)");
        static_assert(offsetof(V, mfLastShuntClosingSpeed)    - KI_HEAD == 604, "X360 +1084 (asm stfs 0, 0x43C)");

        // The tail loop's three bases are exactly 64 bytes apart -- that IS the loop's [r9-64] /
        // [r9] / [r9+64] addressing, so these three pins are the loop transcription's own proof.
        static_assert(offsetof(V, maStuckInCollisionLineTestStart) - KI_HEAD == 608, "X360 +1088 == r9 - 64");
        static_assert(offsetof(V, maStuckInCollisionLineTestEnd)   - KI_HEAD == 672, "X360 +1152 == r9");
        static_assert(offsetof(V, maStuckInCollisionLineTestPoint) - KI_HEAD == 736, "X360 +1216 == r9 + 64");
        static_assert(offsetof(V, mabStuckInCollisionIntersection) - KI_HEAD == 800, "X360 +1280 == r27");

        // sizeof() ALONE CANNOT PIN THE LAST MEMBER. Growing mabStuckInCollisionIntersection
        // from 4 to 8 bytes leaves sizeof(V) at 1296 (the 16-byte tail padding absorbs it), and a
        // tamper test caught exactly that: the sizeof assert below MISSED the wrong array count.
        // The end-of-DATA pin is what catches it -- 1284 - 480 == 804, the loop's own trip count.
        static_assert(sizeof(V::mabStuckInCollisionIntersection) == KI_MAX_WORLD_CONTACT_SPIES,
                      "one intersection flag per line test (the tail loop's stbx count)");
        static_assert(offsetof(V, mabStuckInCollisionIntersection)
                          + sizeof(V::mabStuckInCollisionIntersection) - KI_HEAD == 804,
                      "the class's last DATA byte is X360 +1284");

        // The whole tail is 816 bytes (1296 - 480), i.e. the class ends exactly where
        // BrnVehicleManager.h's `mDebugComponent[163264 - 161968]` span ends.
        static_assert(sizeof(V) - offsetof(V, mPlayerCarContactPosition) == 816,
                      "1296 - 480 == 816: the class closes on the 1296-byte VehicleManager span");
    }
}
}
