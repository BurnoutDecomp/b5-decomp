#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H

#include "types.hpp"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"    // BehaviourGameplayBumper::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"  // BehaviourGameplayExternal::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"           // BehaviourGyroCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"          // BehaviourFixedCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.h"          // BehaviourBystanderCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"            // BehaviourPassengerCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h" // BehaviourRotateAboutVehicle::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h" // BehaviourSpirallingDeathcam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"      // BehaviourAftertouchCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"    // BehaviourAftertouchCrash::Parameters
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"                   // BehaviourRig::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h" // BehaviourLooseAttachment::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h"         // BehaviourHeliCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h"        // BehaviourFailsafe::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h"      // BehaviourRoadRunner::Parameters
#include "GameSource/Director/Camera/Utils/CameraUtils.h"                      // Utils::VersionNumber (the bank's version word)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT

// ============================================================================
// GameSource/Director/Camera/BrnBehaviourParameterBank.h
//
// BrnDirector::NamedParameters -- the director's bank of per-named-behaviour camera
// "Parameters" blocks (aftertouch, gyro, bystander, rig, rotate-about-vehicle, ...). The
// arbitrator-state shared context (ArbStateSharedInfo::mpNamedParameters) holds it BY POINTER
// and an arbitrator state reaches one named block out of it to configure a behaviour it has
// just allocated.
//
// THE RECORD HAS EXACTLY ONE STORAGE: BehaviourParameterBank::mNamedParameters, by value at
// bank +0x10 (see the RECORD MAP banner on that class). The shared-info pointer is bound to
// it by MainDirector::BuildArbStateSharedInfo. There is no second copy anywhere.
//
// The record carries every one of the console's 48 named blocks, in the console's member order.
// Every block uses the native four-byte debug-name pointer. The record offsets
// and block sizes below are the original console contract. Construction and
// serialiser walks live in BrnBehaviourParameterBank.cpp.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    struct NamedParameters
    {
        // The "look around car" / rotate-about-vehicle camera parameter block the online
        // car-select and (offline) car-select states hand to
        // BehaviourRotateAboutVehicle::SetParameters. @+0x2334.
        // ⭐ TYPED 2026-08-01: it is not opaque -- SetParameters @0x821F55B8 asserts
        // `lpParameters->GetType() == eBehaviourRotateAboutVehicle` (tag 18) on whatever the
        // caller hands it, and both call sites hand it exactly this block, so this block IS a
        // BehaviourRotateAboutVehicle::Parameters. (Its interior beyond the shared
        // Behaviour::Parameters head is still unmodelled -- see that class.)
        typedef Camera::BehaviourRotateAboutVehicle::Parameters LookAroundCarCamParameters;

        // The record's leading aftertouch-cam block, at record +0. The behaviour factory hands
        // it to BehaviourAftertouchCam::SetParameters for every authored aftertouchcam shot.
        const Camera::BehaviourAftertouchCam::Parameters& GetAftertouchCamParameters() const
        {
            return mAftertouchCamDefault;
        }

        // The record's first aftertouch-crash block, at record +108. ArbStateCrashMode::Prepare
        // hands it to the crash camera behaviour's SetParameters.
        const Camera::BehaviourAftertouchCrash::Parameters& GetAftertouchCrashParameters() const
        {
            return mAftertouchCrashParams;
        }

        // ---- the gyro blocks the takedown arbitrator states adopt ----------------------
        // Each returns the block by const reference; the caller passes &block to
        // BehaviourGyroCam::SetParameters, whose type-tag tripwire Construct below satisfies.

        // Record +480, gyro slot 0. DestructionPathTakedownPlayer::Prepare adopts it for the
        // first of its two gyro behaviours.
        const Camera::BehaviourGyroCam::Parameters& GetGyroCamDefaultParameters() const
        {
            return mGyroCamDefaultParams;
        }

        // Record +2112, gyro slot 8. B3ClassicTakedownPlayer::Prepare and
        // DestructionPathTakedownPlayer::Prepare both adopt it.
        const Camera::BehaviourGyroCam::Parameters& GetGyroCamTakedownParameters() const
        {
            return mGyroCamTakedownParams;
        }

        // Record +2928, gyro slot 12. DriveByTakedownPlayer::Prepare adopts it for BOTH of its
        // gyro behaviours -- see the run's banner.
        const Camera::BehaviourGyroCam::Parameters& GetGyroCamDriveByLParameters() const
        {
            return mGyroCamDriveByLParams;
        }

        // ---- the three loose-attachment blocks the shutdown-takedown beats adopt ----------
        // ShutdownTakedownPlayer::Update runs three zoom beats; each allocates a loose-attachment
        // behaviour and adopts the NEXT of these three blocks, reading it straight off the shared
        // context's named-parameter record at +8696 / +8796 / +8896. Each returns by const
        // reference; the caller passes &block to BehaviourLooseAttachment::SetParameters, whose
        // type-tag tripwire the class-default seed below satisfies -- the per-record tunings are
        // not recovered, see the FLAG beside that seed.

        const Camera::BehaviourLooseAttachment::Parameters& GetLooseAttachmentTakedown1Parameters() const
        {
            return mLooseAttachmentTakedown1;
        }

        const Camera::BehaviourLooseAttachment::Parameters& GetLooseAttachmentTakedown2Parameters() const
        {
            return mLooseAttachmentTakedown2;
        }

        const Camera::BehaviourLooseAttachment::Parameters& GetLooseAttachmentTakedown3Parameters() const
        {
            return mLooseAttachmentTakedown3;
        }

        // Accessor returning the address of the look-around-car parameter block (mpNamedParameters
        // + 0x2334). Returns by const reference; the caller passes &block to SetParameters.
        const LookAroundCarCamParameters& GetLookAroundCarCamParameters() const
        {
            return maLookAroundCarCamParameters;
        }

        // Seeds the record: the per-block Parameters::Construct calls and the authored re-tunes
        // of BehaviourParameterBank::Construct. Body: BrnBehaviourParameterBank.cpp.
        void Construct();

        // The spiralling-deathcam parameter block ArbStateCrashing::Prepare hands to
        // BehaviourSpirallingDeathcam::SetParameters (the console's record +0x23B4, 0x80 past the
        // look-around block).
        const Camera::BehaviourSpirallingDeathcam::Parameters& GetSpirallingDeathcamParameters() const
        {
            return maSpirallingDeathcamParameters;
        }

        // ---- THE RECORD, in the console's member order ------------------------------------
        // Each comment is the block's console record offset (bank offset = record + 0x10). The
        // console record is a run of same-typed blocks:
        //   4 head blocks | 14 gyro | 7 bystander | 14 rig | failsafe | passenger |
        //   3 loose-attachment | fixed | rotate-about-vehicle | deathcam | road-runner
        // and its consumers' displacements land on those offsets (see the RECORD MAP on
        // BehaviourParameterBank below): MomentTumbling's six gyro reads sit on the gyro block's
        // 204-byte console stride, the three shutdown-takedown beats on the loose-attachment
        // block's 100-byte stride. Native declarations preserve those block offsets.
        Camera::BehaviourAftertouchCam::Parameters      mAftertouchCamDefault;                   // console +0  "Aftertouch"
        Camera::BehaviourAftertouchCrash::Parameters    mAftertouchCrashParams;                  // console +108  "Aftertouch Crash"
        Camera::BehaviourAftertouchCrash::Parameters    mCrashDebugParams;                       // console +220  "Crash Debug"
        Camera::BehaviourHeliCam::Parameters            mHeliCamDefaultParams;                   // console +332  "HeliCam Default"
        Camera::BehaviourGyroCam::Parameters            mGyroCamDefaultParams;                   // console +480  E_SUBTYPE_LEAD
        Camera::BehaviourGyroCam::Parameters            mGyroCamTruckFront;                      // console +684  E_SUBTYPE_TRUCKING_FRONT
        Camera::BehaviourGyroCam::Parameters            mGyroCamLeft;                            // console +888  E_SUBTYPE_SIDE
        Camera::BehaviourGyroCam::Parameters            mGyroCamRight;                           // console +1092
        Camera::BehaviourGyroCam::Parameters            mGyroCamDefaultSideTruckingLeftParams;   // console +1296  E_SUBTYPE_TRUCKING_SIDE
        Camera::BehaviourGyroCam::Parameters            mGyroCamDefaultSideTruckingRightParams;  // console +1500  E_SUBTYPE_TRUCKING_SIDE
        Camera::BehaviourGyroCam::Parameters            mGyroCamFollow;                          // console +1704  E_SUBTYPE_FOLLOW
        Camera::BehaviourGyroCam::Parameters            mGyroCamAlwaysLowParams;                 // console +1908
        Camera::BehaviourGyroCam::Parameters            mGyroCamTakedownParams;                  // console +2112  the takedown states
        Camera::BehaviourGyroCam::Parameters            mGyroCamTakedownZoomedOutParams;         // console +2316
        Camera::BehaviourGyroCam::Parameters            mGyroCamHighParams;                      // console +2520
        Camera::BehaviourGyroCam::Parameters            mGyroCamHelicamParams;                   // console +2724  the hit-traffic moment
        Camera::BehaviourGyroCam::Parameters            mGyroCamDriveByLParams;                  // console +2928  the drive-by takedown
        Camera::BehaviourGyroCam::Parameters            mGyroCamDriveByRParams;                  // console +3132  no reader
        Camera::BehaviourBystanderCam::Parameters       mBystanderJumpLeftParameters;            // console +3336
        Camera::BehaviourBystanderCam::Parameters       mBystanderJumpParameters2;               // console +3492
        Camera::BehaviourBystanderCam::Parameters       mBystanderJumpFromBehindParameters;      // console +3648
        Camera::BehaviourBystanderCam::Parameters       mBystanderCloseParameters;               // console +3804
        Camera::BehaviourBystanderCam::Parameters       mBystanderMediumParameters;              // console +3960
        Camera::BehaviourBystanderCam::Parameters       mBystanderFarParameters;                 // console +4116
        Camera::BehaviourBystanderCam::Parameters       mBystanderFarTallParameters;             // console +4272
        Camera::BehaviourRig::Parameters                mRigBonnetLowRight;                      // console +4432
        Camera::BehaviourRig::Parameters                mRigRearQFwd;                            // console +4720
        Camera::BehaviourRig::Parameters                mRigFrontQCuFwd;                         // console +5008
        Camera::BehaviourRig::Parameters                mRigFrontQBwd;                           // console +5296
        Camera::BehaviourRig::Parameters                mRigFrontRearview;                       // console +5584
        Camera::BehaviourRig::Parameters                mRigBootViewFwd;                         // console +5872
        Camera::BehaviourRig::Parameters                mRigFrontQLowBwd;                        // console +6160
        Camera::BehaviourRig::Parameters                mRigRoofFwd;                             // console +6448
        Camera::BehaviourRig::Parameters                mRigBootFwd;                             // console +6736
        Camera::BehaviourRig::Parameters                mRigFrontQCuFwd2;                        // console +7024
        Camera::BehaviourRig::Parameters                mRigUnderbelly;                          // console +7312
        Camera::BehaviourRig::Parameters                mRigDropUnderbelly;                      // console +7600
        Camera::BehaviourRig::Parameters                mRigDropFrontQCuFwd;                     // console +7888
        Camera::BehaviourRig::Parameters                mRigDropBootViewFwd;                     // console +8176  walk name "Rig Drop Boot Q Cu Fwd"
        Camera::BehaviourFailsafe::Parameters           mFailsafe;                               // console +8464
        Camera::BehaviourPassengerCam::Parameters       mPassengerDefault;                       // console +8660
        Camera::BehaviourLooseAttachment::Parameters    mLooseAttachmentTakedown1;               // console +8696  shutdown-takedown beat 1
        Camera::BehaviourLooseAttachment::Parameters    mLooseAttachmentTakedown2;               // console +8796  beat 2
        Camera::BehaviourLooseAttachment::Parameters    mLooseAttachmentTakedown3;               // console +8896  beat 3
        Camera::BehaviourFixedCam::Parameters           mFixedDefault;                           // console +8996
        LookAroundCarCamParameters                      maLookAroundCarCamParameters;            // console +9012  +0x2334
        Camera::BehaviourSpirallingDeathcam::Parameters maSpirallingDeathcamParameters;          // console +9140  +0x23B4
        Camera::BehaviourRoadRunner::Parameters         mRoadRunnerDefault;                      // console +9316
    };

    // Native sizes attested by the parameter walks and bank consumers.
    CGS_STATIC_ASSERT((sizeof(Camera::BehaviourAftertouchCam::Parameters) == 108), "X360 camera parameter block");
    CGS_STATIC_ASSERT((sizeof(Camera::BehaviourAftertouchCrash::Parameters) == 112), "X360 camera parameter block");
    CGS_STATIC_ASSERT((sizeof(Camera::BehaviourHeliCam::Parameters) == 148), "X360 camera parameter block");
    CGS_STATIC_ASSERT((sizeof(Camera::BehaviourGyroCam::Parameters) == 204), "X360 camera parameter block");
    CGS_STATIC_ASSERT((sizeof(Camera::BehaviourFailsafe::Parameters) == 196), "X360 camera parameter block");
    CGS_STATIC_ASSERT((sizeof(Camera::BehaviourLooseAttachment::Parameters) == 100), "X360 camera parameter block");
    CGS_STATIC_ASSERT((sizeof(Camera::BehaviourSpirallingDeathcam::Parameters) == 176), "X360 camera parameter block");
    // Actual console offsets for every named block.
    CGS_STATIC_ASSERT((sizeof(NamedParameters) == 0x2470), "X360 named camera bank size");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mAftertouchCamDefault) == 0), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mAftertouchCrashParams) == 108), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mCrashDebugParams) == 220), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mHeliCamDefaultParams) == 332), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamDefaultParams) == 480), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamTruckFront) == 684), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamLeft) == 888), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamRight) == 1092), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamDefaultSideTruckingLeftParams) == 1296), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamDefaultSideTruckingRightParams) == 1500), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamFollow) == 1704), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamAlwaysLowParams) == 1908), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamTakedownParams) == 2112), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamTakedownZoomedOutParams) == 2316), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamHighParams) == 2520), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamHelicamParams) == 2724), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamDriveByLParams) == 2928), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mGyroCamDriveByRParams) == 3132), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mBystanderJumpLeftParameters) == 3336), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mBystanderJumpParameters2) == 3492), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mBystanderJumpFromBehindParameters) == 3648), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mBystanderCloseParameters) == 3804), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mBystanderMediumParameters) == 3960), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mBystanderFarParameters) == 4116), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mBystanderFarTallParameters) == 4272), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigBonnetLowRight) == 4432), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigRearQFwd) == 4720), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigFrontQCuFwd) == 5008), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigFrontQBwd) == 5296), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigFrontRearview) == 5584), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigBootViewFwd) == 5872), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigFrontQLowBwd) == 6160), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigRoofFwd) == 6448), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigBootFwd) == 6736), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigFrontQCuFwd2) == 7024), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigUnderbelly) == 7312), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigDropUnderbelly) == 7600), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigDropFrontQCuFwd) == 7888), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRigDropBootViewFwd) == 8176), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mFailsafe) == 8464), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mPassengerDefault) == 8660), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mLooseAttachmentTakedown1) == 8696), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mLooseAttachmentTakedown2) == 8796), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mLooseAttachmentTakedown3) == 8896), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mFixedDefault) == 8996), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, maLookAroundCarCamParameters) == 9012), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, maSpirallingDeathcamParameters) == 9140), "ARTIST named camera block");
    CGS_STATIC_ASSERT((offsetof(NamedParameters, mRoadRunnerDefault) == 9316), "ARTIST named camera block");

    namespace Camera
    {
        // BrnDirector::Camera::BehaviourParameterBank (PS3 DWARF: the type of the local
        // `lrBehaviourParameterBank` in SharedCameraContainer::Prepare, and of the
        // BehaviourManager's embedded :325 sub-object at X360 manager +0x12530). MINIMAL
        // SLICE: only the two named fetches SharedCameraContainer::Prepare @0x82263D50 needs.
        // The X360 inlines them to fixed bank offsets -- the external ("chase") block at
        // bank+0x2488 and the bumper block at bank+0x2538 == +0x2488 + 0xB0
        // (sizeof(BehaviourGameplayExternal::Parameters)), so the two blocks are adjacent.
        // Both accessors are DECLARATION-ONLY (their trivial fetch bodies need the bank
        // layout, which is un-homed -- same status as the manager's opaque :325 slot).
        // The PS3 DWARF names the bumper fetch GetGameplayBumperCameraParamsForCar with the
        // X360 ABI showing NO car argument (a fixed-offset fetch): the DWARF name is kept
        // with the X360 arity. FLAG: the external accessor's name is inferred by symmetry
        // (its PS3 hint line is truncated).
        //
        // ⭐⭐⭐ THE RECORD MAP -- RESOLVED 2026-09-11 (moment-camera wave). The note that used
        // to stand here said the relationship between NamedParameters (above) and this bank was
        // NOT pinned. IT IS PINNED NOW, and pinning it is what closed the moment-camera
        // accessor wall, so the derivation is recorded here in full rather than as a verdict.
        //
        // THE ANSWER: NamedParameters IS this bank's leading sub-record, at bank + 0x10.
        //     bank + 0x00   muVersion (+ pad to the record's 16-byte alignment)
        //     bank + 0x10   mNamedParameters          9328 bytes
        //     bank + 0x2480 the latched car key       (already homed below)
        //     bank + 0x2488 the external gameplay block / bank + 0x2538 the bumper one
        // so every bank displacement below is `record offset + 0x10`, and the arbitrator's
        // mpNamedParameters is &bank.mNamedParameters.
        //
        // THE DERIVATION, four independent anchors, no free parameters:
        //   (a) The record's member ORDER is the bank serialiser's walk order, and the type of
        //       every member is known, so the record is a run of same-typed blocks:
        //       4 head blocks | 14 gyro | 7 bystander | 14 rig | failsafe | passenger |
        //       3 loose-attachment | fixed | rotate-about-vehicle | deathcam | road-runner.
        //   (b) MomentTumbling::SetGyroCamParameters reads six gyro blocks off
        //       mpNamedBehaviourParams at +480/+684/+888/+1296/+1500/+1704 -- an exact
        //       204-byte grid == sizeof(BehaviourGyroCam::Parameters) -- which fixes gyro
        //       slot 0 at record +480 and so the 14-block run at +480..+3336. The subtype
        //       names land on the block names (see NamedParameters above).
        //   (c) MomentBystanderSeesAction::Update reads manager+78876 (its "close" flag set)
        //       and manager+79188 (clear), 312 apart. The only bystander stride that puts
        //       the FIRST of those on the block named Close is 156, giving slots 3 and 5 and
        //       forcing bank == record - 0x10. The second solves as mBystanderFarParameters.
        //   (d) That same bank base then lands, with NO further freedom: manager+84068 on the
        //       block named Fixed Cam Default (its consumer is a fixed cam); the arbitrator's
        //       record+0x2334 on mRotateAboutVehicleDefault, 16 bytes past it, 16 being exactly
        //       sizeof(BehaviourFixedCam::Parameters); the arbitrator's record+0x23B4 on
        //       mSpirallingDeathCamDefault, 0x80 past THAT, 0x80 being exactly the console
        //       rotate-about-vehicle block size this file already recorded; and the record's
        //       end on bank+0x2480, the latched car key homed below. Four consumers that were
        //       never compared to each other all agree.
        //
        // RECORD OFFSETS, for the blocks this slice or its consumers name:
        //   +480 .. +3336   14 gyro blocks, stride 204  (slot 11 == mGyroCamHelicamParams,
        //                   the hit-traffic moment's block at +2724)
        //   +3336 .. +4428  7 bystander blocks, stride 156  (0 JumpLeft, 1 Jump2,
        //                   2 JumpFromBehind, 3 Close, 4 Medium, 5 Far, 6 FarTall)
        //   +4432 .. +8464  14 rig blocks, stride 288 (the run is 4-byte padded up to its
        //                   16-byte alignment); slots 11..13 are the three named "Drop"
        //   +8464           mFailsafe (196)
        //   +8660           mPassengerDefault
        //   +8996           mFixedDefault (16)
        //   +9012           mRotateAboutVehicleDefault (128)   == the +0x2334 block above
        //   +9140           mSpirallingDeathCamDefault         == the +0x23B4 block above
        //   +9328           end of record
        // ⭐ THE FOUR HEAD BLOCKS ARE SEPARATED NOW (2026-09-11). The note that stood here said
        // their individual sizes were "not separated by anything that reads them"; the bank's
        // own Construct separates them, by calling a per-block Parameters::Construct on each in
        // turn at record +0 / +108 / +220 / +332:
        //   +0    mAftertouchCamDefault      (108)  aftertouch-cam   -- carved above
        //   +108  aftertouch-crash           (112)
        //   +220  aftertouch-crash           (112)  the second of the pair
        //   +332  helicam                    (148)
        // 108 + 112 + 112 + 148 == 480, which is the gyro run's start, so the head closes
        // exactly and the third block is an aftertouch-crash one rather than a "crash-debug".
        //
        // ⭐ THE STORAGE FORK IS CLOSED (2026-09-11). The record used to exist TWICE: once as
        // MainDirector::mNamedParameters (what the arbitrator's mpNamedParameters pointed at)
        // and once, implicitly, as this bank the console owns it in. The bank now carries it
        // BY VALUE at bank +0x10 -- mNamedParameters below, placed behind a 16-byte head so
        // the record starts exactly where the derivation above puts it -- the director member
        // is deleted, and BuildArbStateSharedInfo publishes &bank.mNamedParameters. One
        // object, seeded once, by this bank's own Construct as the console does it.
        //
        // ⓘ HOST WIDTH INSIDE THE RECORD. Every block's Behaviour::Parameters head carries a
        // debug-name POINTER that is 8 bytes wide on this host against the console's 4, so every
        // block is wider than its console stride and the record offsets above (and bank +0x2480
        // and below) are provenance rather than placements; every reader goes by member name.
        //
        // ⭐⭐ THE TWO GAMEPLAY BLOCKS + THE LATCHED CAR KEY ARE HOMED AS OF 2026-08-02
        // (camera parameter-chain wave). They are the three slots the whole chase/bumper
        // camera chain turns on, and until now they existed NOWHERE, which is why every
        // consumer of them was commented out. The pin is derived, not guessed:
        //
        //   1. MainDirector::UpdateCameraBehavioursPreScene @0x82255318 builds the `this`
        //      for BehaviourManager::UpdateAllBehaviours as
        //          addis r26, r31, 2 ; addi r26, r26, -0x34F0     (@0x82255770/@0x8225577C)
        //      == director + 0x1CB10  ⇒ BehaviourManager sits at MainDirector + 0x1CB10.
        //   2. SharedCameraContainer::Prepare @0x82263D50 forms the bank as manager+0x12530,
        //      so bank == director + 0x1CB10 + 0x12530 == director + 0x2F040.
        //   3. MainDirector::ProcessNewVehicleEvents @0x8221A6B0 and UpdateAttribSys
        //      @0x8221AFD0 then reach, off the DIRECTOR:
        //          director + 0x314C0  ==  bank + 0x2480   the 8-byte car key (`std`/`ldx`)
        //          director + 0x314C8  ==  bank + 0x2488   the EXTERNAL params block
        //          director + 0x31578  ==  bank + 0x2538   the BUMPER   params block
        //      -- the same two block offsets SharedCameraContainer::Prepare inlines, and the
        //      same 0xB0 spacing (== sizeof(BehaviourGameplayExternal::Parameters)) at both
        //      sites. Two independent functions agreeing on both offsets AND the gap is what
        //      makes this a pin rather than an arithmetic coincidence.
        //
        // ⇒ the 8 bytes at bank+0x2480, immediately below the external block, are a BANK
        // member: the attribute-collection key of the car the two gameplay blocks were last
        // seeded from. ProcessNewVehicleEvents `std`s it after seeding; UpdateAttribSys
        // `ldx`s it every frame to re-seed from the same car without re-reading the queue.
        // FLAG: the member NAME is ours (the console has no symbol for it); its offset, width
        // and role are all asm-attested.
        //
        // Every one of the record's 48 named blocks is a member of mNamedParameters; x64 parity
        // is BY NAMED MEMBER, so the console displacements above are provenance only.
        class BehaviourParameterBank
        {
        public:
            // ⭐ X360 BehaviourParameterBank::Construct @0x8223DC90, the three-slot slice --
            // the console's own three statements, in its own order:
            //   0x8223DCCC  std r30(=0), 0x2480(r31)      the latched car key = 0
            //   0x8223DCB4  addi r11, r31, 0x2488  + the INLINED external Parameters::Construct
            //   0x8223DCC8  addi r10, r31, 0x2538  + the INLINED bumper   Parameters::Construct
            // Both per-block Constructs are now REAL (BrnBehaviourGameplayExternal.cpp /
            // BrnBehaviourGameplayBumper.cpp, each transcribed from those inlined stores), so
            // this is a faithful call rather than a stand-in. THE BANK DELIBERATELY LEAVES
            // BOTH mbIsValid FALSE; only Parameters::Set raises them.
            //
            // ⭐ THE `std` AT 0x8223DCCC IS ALSO THE DIRECT PROOF that bank+0x2480 is an
            // eight-byte member of THIS class -- the banner's derivation from
            // ProcessNewVehicleEvents' `std` and UpdateAttribSys' `ldx` is corroborated here
            // by the bank's own zeroing of the same slot at the same width.
            //
            // [FLAG, PC-only] the two ZeroBlock calls are NOT console behaviour: the console
            // leaves the rest of each block at whatever the manager's storage held and relies
            // on Parameters::Set writing every 4-byte slot before mbIsValid goes true. They
            // are here so no PC consumer can read an indeterminate f32 in the window before
            // the first Set. Strict superset of the console's stores; remove if the bank ever
            // gets a zero-initialised home of its own.
            void Construct();

            // The named-parameter record this bank owns, at bank +0x10. The arbitrator states
            // reach one block out of it through ArbStateSharedInfo::mpNamedParameters, which
            // MainDirector binds to this member; the moment family reaches it through the
            // behaviour manager's bank accessor.
            const NamedParameters& GetNamedParameters() const { return mNamedParameters; }
            NamedParameters&       GetNamedParameters()       { return mNamedParameters; }

            // The `burnoutcarasset` collection key of the car the two blocks below currently
            // hold the tuning for. X360 bank+0x2480 -- see the banner.
            u64  GetGameplayCameraCarAttribsKey() const { return mxGameplayCameraCarAttribsKey; }
            void SetGameplayCameraCarAttribsKey(u64 lxKey) { mxGameplayCameraCarAttribsKey = lxKey; }

            // X360 bank+0x2538: the bumper-cam ("in car") gameplay parameter block.
            const BehaviourGameplayBumper::Parameters& GetGameplayBumperCameraParamsForCar() const
            {
                return mGameplayBumperCameraParamsForCar;
            }
            // The write-side overload the director's attribute pump seeds through
            // (ProcessNewVehicleEvents / UpdateAttribSys hand `director+0x31578` straight to
            // Parameters::Set). FLAG: the non-const spelling is ours; the console reaches the
            // same storage by inlined displacement.
            BehaviourGameplayBumper::Parameters& GetGameplayBumperCameraParamsForCar()
            {
                return mGameplayBumperCameraParamsForCar;
            }

            // X360 bank+0x2488: the external ("chase") gameplay parameter block.
            const BehaviourGameplayExternal::Parameters& GetGameplayExternalCameraParamsForCar() const
            {
                return mGameplayExternalCameraParamsForCar;
            }
            BehaviourGameplayExternal::Parameters& GetGameplayExternalCameraParamsForCar()
            {
                return mGameplayExternalCameraParamsForCar;
            }

            // ---- the four moment camera blocks (see the RECORD MAP banner) ---------------
            // Each returns a const reference, so none of them could ever have been stubbed;
            // they were the whole moment closure's wall until the record map pinned which
            // named block each one is. All four are now real named members of this class.

            // The gyro-cam block the hit-traffic moment binds (MomentHitTraffic::Update
            // hands manager+77796 == record +2724 to BehaviourGyroCam::SetParameters).
            // Record +2724 is gyro slot 11 == mGyroCamHelicamParams, so this returns the
            // record's own block.
            const BehaviourGyroCam::Parameters& GetGyroCamMomentParams() const
            {
                return mNamedParameters.mGyroCamHelicamParams;
            }

            // The fixed-cam block the static-cam-impact moment binds
            // (MomentStaticCamImpact::Update hands manager+84068 == record +8996 to
            // BehaviourFixedCam::SetParameters). Record +8996 is mFixedDefault -- whose
            // own name says "Fixed Cam Default" and whose consumer is a fixed cam.
            // ⓘ This RETIRES the old note that +0x2334 "coincides with
            // maLookAroundCarCamParameters": it does not. The static-cam block is at
            // BANK+0x2334 == record +0x2324, and the look-around block is at RECORD
            // +0x2334 == bank+0x2344. They are adjacent, not the same block, and the
            // 16-byte gap between them is exactly sizeof(BehaviourFixedCam::Parameters).
            const BehaviourFixedCam::Parameters& GetStaticCamImpactCamParams() const
            {
                return mNamedParameters.mFixedDefault;
            }

            // The two bystander-sees-action camera blocks (MomentBystanderSeesAction::
            // Update picks by its Parameters::mbCloseCamera and feeds the block to
            // BehaviourBystanderCam::SetParameters): manager+78876 == record +3804 for
            // the close camera, manager+79188 == record +4116 otherwise. Those are
            // bystander slots 3 and 5 == mBystanderCloseParameters / mBystanderFarParameters
            // -- the close flag selecting the block literally named Close is the
            // corroboration that fixes the whole bystander run's stride.
            const BehaviourBystanderCam::Parameters& GetBystanderCamCloseMomentParams() const
            {
                return mNamedParameters.mBystanderCloseParameters;
            }
            const BehaviourBystanderCam::Parameters& GetBystanderCamMomentParams() const
            {
                return mNamedParameters.mBystanderFarParameters;
            }

            // The passenger-sees-action camera block (MomentPassengerSeesAction::Update
            // hands manager+83732 == record +8660 to BehaviourPassengerCam::SetParameters).
            // Record +8660 is mPassengerDefault, constructed by Construct above (type 7).
            const BehaviourPassengerCam::Parameters& GetPassengerCamMomentParams() const
            {
                return mNamedParameters.mPassengerDefault;
            }

            // The camera tweaker's two debug-menu actions (DebugCallbackFunction shape: the
            // user data is the bank). SaveParameters writes the whole bank to "d:\\camera.txt"
            // through TextFileWriteSerialiser; LoadParameters reads it back through
            // TextFileReadSerialiser. Bodies: BrnBehaviourParameterBank.cpp.
            static void SaveParameters(void* lpVoid);
            static void LoadParameters(void* lpVoid);

            // The bank's serialiser-visitor template: stamps muVersion, then hands every named
            // block of the record to SerialiseBehaviourParameters under its walk name. One
            // generic body + one explicit instantiation per serialiser (TextFileRead /
            // TextFileWrite / TestbedSetup / BehaviourParameterNaming):
            // BrnBehaviourParameterBank.cpp.
            template<class T> void Serialise(T& lrSerialiser);

            // NEVER CALLED. Pins the record's placement inside this class -- a member
            // function so the assert can see the private member. See _AssertBankLayout below.
            static void _AssertBankLayout();

        private:
            // Byte zero-fill helper for the two blocks -- see Construct's FLAG. Kept as a
            // named helper so no caller memsets a class type in place.
            static void ZeroBlock(void* lpBlock, u32 luBytes)
            {
                u8* lpBytes = static_cast<u8*>(lpBlock);
                for (u32 luByte = 0; luByte < luBytes; ++luByte)
                {
                    lpBytes[luByte] = 0;
                }
            }

            // ---- the record, by value at its attested offset ------------------------------
            // The bank's head is its version word (the bank walks stamp it with the current file
            // version) and the pad up to the record's alignment; the offsetof ratchet below
            // fails the build if the record ever stops starting at +0x10.
            Utils::VersionNumber muVersion;                                         // +0x0000
            u8                   maReservedBankHead[0x10 - sizeof(Utils::VersionNumber)];  // +0x0004 .. +0x000F
            NamedParameters      mNamedParameters;                                  // +0x0010

            // ---- the three homed slots (see the banner for the pin) -----------------------
            u64                                   mxGameplayCameraCarAttribsKey;        // +0x2480
            BehaviourGameplayExternal::Parameters mGameplayExternalCameraParamsForCar;  // +0x2488
            BehaviourGameplayBumper::Parameters   mGameplayBumperCameraParamsForCar;    // +0x2538
        };

        // NEVER CALLED. The record's start is the one bank offset that holds on the host as on
        // the console, and the whole derivation in the banner rests on it: every attested
        // displacement in the tree is `record offset + 0x10`. If a future edit puts a member
        // ahead of the record, or widens the head, the build fails here.
        inline void BehaviourParameterBank::_AssertBankLayout()
        {
            CGS_STATIC_ASSERT((offsetof(BehaviourParameterBank, muVersion) == 0x0),
                          "BehaviourParameterBank::muVersion @ bank +0x0");
            CGS_STATIC_ASSERT((offsetof(BehaviourParameterBank, mNamedParameters) == 0x10),
                          "BehaviourParameterBank::mNamedParameters @ bank +0x10");
        }
    }
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H
