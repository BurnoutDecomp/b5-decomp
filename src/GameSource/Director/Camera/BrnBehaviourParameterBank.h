#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H

#include "types.hpp"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"    // BehaviourGameplayBumper::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"  // BehaviourGameplayExternal::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"           // BehaviourGyroCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"          // BehaviourFixedCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.h"       // BehaviourBystanderCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"            // BehaviourPassengerCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h" // BehaviourRotateAboutVehicle::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h" // BehaviourSpirallingDeathcam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"      // BehaviourAftertouchCam::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"    // BehaviourAftertouchCrash::Parameters
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"                   // BehaviourRig::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h" // BehaviourLooseAttachment::Parameters
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
// FLAG: MINIMAL SLICE. The full bank (every BehaviourXxx::Parameters sub-block, the
//   BehaviourParameterBank wrapper + its serialiser) is a heavy cascade and has no
//   reconstructed home of its own yet. This header models ONLY the one named accessor this
//   build's online-car-select arbitrator state needs -- the "look around car" (rotate-about-
//   vehicle) parameter block -- accessed BY NAME via its address. The recovered type
//   information names the bank's earlier blocks but NOT this one (the rotate-about-vehicle
//   params are a later addition), so the block's precise type is unrecoverable; it is
//   modelled as a named opaque sub-object at the attested offset and only its address is
//   taken (passed to BehaviourRotateAboutVehicle::SetParameters as an opaque parameter block).
//   Replace with the real layout when the BehaviourParameterBank TU lands; the accessor NAME
//   is stable.
//
//   X360 (ArbStateOnlineCarSelect::Prepare @0x82271020): the block sits at
//   mpNamedParameters + 0x2334 (asm `addi r31, r11, 0x2334`); modelled here as the named
//   member maLookAroundCarCamParameters at that offset and returned by address.
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

        // ⭐ ADDED 2026-08-01 (junkyard-fire wave). The console builds this bank from
        // BehaviourParameterBank::Construct @0x8223DC90, called by BehaviourManager::Construct
        // @0x82251778 (the gate is marked in that body). Only the ONE block this slice models is
        // seeded -- with the tag its own Parameters::Construct @0x821FB330 stores, so
        // BehaviourRotateAboutVehicle::SetParameters' `GetType() == eBehaviourRotateAboutVehicle`
        // tripwire passes. The un-modelled head is zeroed rather than left as pool garbage.
        //
        // ⭐⭐ THE BANK'S OWN FOUR RE-TUNES FOR THIS BLOCK LAND 2026-08-02 (framing wave), AND
        // THEY RETIRE A WRONG PREMISE. The note that used to end this banner said "the authored
        // tunings are NOT loaded", which read as *there is a data file we do not read*. There is
        // no such file for this bank. BehaviourParameterBank::LoadParameters @0x82273268 opens
        // "d:\\camera.txt" and has ZERO xrefs in the whole XEX -- it is the dev tweaker's
        // reload, the mirror of SaveParameters. The console's authored tunings for this camera
        // are COMPILED IN, in two places:
        //   (a) BehaviourRotateAboutVehicle::Parameters::Construct @0x821FB300 -- its thirteen
        //       re-tunes, which this tree already transcribed in full; and
        //   (b) ⭐ FOUR MORE `stfs` in the BANK's Construct, applied to this block AFTER the
        //       call, which nothing here reproduced. THOSE FOUR ARE THE FRAMING.
        //
        // The four, read straight off the asm (block base is bank+0x2344, so the displacements
        // below are block-relative):
        //   0x8223E6BC  stfs f25, 0x235C(r31)   -> +0x18  mfTargetSubjectXSize
        //   0x8223E6C8  stfs f25, 0x2360(r31)   -> +0x1C  mfTargetSubjectYSize
        //   0x8223E6B8  stfs f19, 0x2364(r31)   -> +0x20  mfTargetSubjectXScreenOffset
        //   0x8223E6C4  stfs f0,  0x2368(r31)   -> +0x24  mfTargetSubjectYScreenOffset
        // f25's last load is `lfs f25, flt_82004018` @0x8223E258, f19's is
        // `lfs f19, flt_82004010` @0x8223E140, and f0 is loaded from flt_82009B70 at
        // 0x8223E6C0 -- no other instruction in the function touches f19 or f25 in between.
        // The three .rdata words (read with the recalibrated .id1 reader, NOT cam5_id1.py):
        //   flt_82004018 = 0x3F400000 =  0.75f
        //   flt_82004010 = 0x3E000000 =  0.125f
        //   flt_82009B70 = 0xBE000000 = -0.125f
        // Sanity-checked in the same read: the two words the neighbouring FixedCam block stores
        // at bank+0x233C/+0x2340 come back as 70.0f and 10.0f, which is exactly what the
        // pseudocode of the same function shows -- so the reader is calibrated on this region.
        //
        // ⛔ AND THE BLOCK GETS NOTHING ELSE. A scan of every store in Construct with a
        // displacement inside [0x2344, 0x23C4) off r31 returns exactly these four, and no
        // `addi` in the function forms an alias base into the block's interior. In particular
        // +0x7C mfShakeBlending0to1 (bank+0x23C0) IS NOT WRITTEN -- see the note this retires in
        // BrnBehaviourRotateAboutVehicle.cpp: the shake staying at 0 is the console's own shape
        // for this camera, not something the authored bank was going to switch on.
        //
        // ⓘ COROBORATION FOR THE +0x2334 MODEL BELOW (still not proof, still flagged): the bank
        // puts this block at bank+0x2344 while the arbitrator states reach it at
        // mpNamedParameters+0x2334, and bank+0x10 is exactly where the bank's FIRST Parameters
        // block starts (`addi r3, r31, 0x10` -> BehaviourAftertouchCam::Parameters::Construct).
        // 0x10 + 0x2334 == 0x2344, so NamedParameters is very likely the bank's payload viewed
        // from +0x10. bank+0x2334 itself holds a 16-byte {tag 15, 0, 70.0f, 10.0f} block --
        // the FixedCam one the header's own accessor list already attributes there.
        //
        // [FLAG PC bring-up] this is still a ONE-BLOCK stand-in for the bank's own Construct:
        // the other ~40 named blocks are neither placed nor seeded.
        // DELETE-WHEN: the BehaviourParameterBank TU lands with the real bank layout.
        void Construct()
        {
            for (u32 luByte = 0; luByte < sizeof(maReservedHead); ++luByte)
            {
                maReservedHead[luByte] = 0;
            }

            // ⭐ 2026-09-11: the record's first block, with its own authored Construct -- the
            // bank's own first statement. Unlike the gyro blocks below it, this one is a real
            // transcription, not a zero-and-tag stand-in: its per-block Construct is a
            // straight-line run of constant stores and every one of them is reproduced.
            mAftertouchCamDefault.Construct();

            // ⭐ 2026-09-12: the two aftertouch-crash blocks, each with its own authored
            // Construct -- a straight-line constant run, reproduced in full. Without them the
            // crash-mode camera's SetParameters type-tag tripwire fires on the first crash and
            // the whole rig runs off a zeroed block.
            mAftertouchCrashParams.Construct();
            mCrashDebugParams.Construct();

            maLookAroundCarCamParameters.Construct();

            // The bank's own four post-Construct re-tunes -- see the banner.
            maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectXSize         =  0.75f;
            maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectYSize         =  0.75f;
            maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectXScreenOffset =  0.125f;
            maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectYScreenOffset = -0.125f;

            // ⭐ 2026-08-29: seed the deathcam block too, with its own attested Construct
            // (@0x821FB498). Without this the block would be pool garbage and
            // BehaviourSpirallingDeathcam::SetParameters' type-tag tripwire would fire on the
            // first road-rage-totalled crash.
            maSpirallingDeathcamParameters.Construct();

            // ⭐ The three loose-attachment blocks. Each is seeded by calling the CLASS
            // default seed, BehaviourLooseAttachment::Parameters::Construct -- itself a real
            // attested constant run, transcribed in that class's own header -- which writes the
            // type tag, so the three shutdown-takedown zoom beats meet a tagged block instead of
            // pool garbage when BehaviourLooseAttachment::SetParameters runs its tripwire.
            // [FLAG PC bring-up] the class default is NOT attested as these records' content.
            // Each of the three is a separate named block in the bank, and whatever per-block
            // re-tunes the bank's own Construct writes over the seed are NOT recovered -- the
            // same gap the gyro blocks below carry, and the reason this is a stand-in rather
            // than a transcription. Until the bank's Construct lands the beats run on the class
            // defaults (mfPitch 5.0, mfDistance 4.0, mfField54 90.0, mfDetachLerpAmount 0.1).
            // A per-record delta is the expected shape, not the exception: the state-owned
            // block the non-beat loose-attachment take adopts is seeded by this same Construct
            // and then re-tuned on three of those very fields at its own call site.
            // DELETE-WHEN: the BehaviourParameterBank TU lands with the real bank Construct.
            mLooseAttachmentTakedown1.Construct();
            mLooseAttachmentTakedown2.Construct();
            mLooseAttachmentTakedown3.Construct();

            // ⭐ 2026-09-11: seed the gyro blocks. BehaviourGyroCam::Parameters
            // has no Construct of its own in this tree, so the blocks are zeroed and stamped
            // with the gyro type tag -- the one field BehaviourGyroCam::SetParameters asserts
            // on. [FLAG PC bring-up] the authored per-block tunings are NOT reproduced: they
            // are compiled into the bank's own Construct, which is not recovered, so every
            // block reads as a zeroed gyro rig. The tag is what keeps the tripwire honest.
            Camera::BehaviourGyroCam::Parameters* const lapGyro[] = {
                &mGyroCamDefaultParams,
                &mGyroCamTruckFront,
                &mGyroCamLeft,
                &mGyroCamRight,
                &mGyroCamDefaultSideTruckingLeftParams,
                &mGyroCamDefaultSideTruckingRightParams,
                &mGyroCamFollow,
                &mGyroCamAlwaysLowParams,
                &mGyroCamTakedownParams,
                &mGyroCamTakedownZoomedOutParams,
                &mGyroCamHighParams,
                &mGyroCamHelicamParams,
                &mGyroCamDriveByLParams,
                &mGyroCamDriveByRParams,
            };
            for (u32 luBlock = 0; luBlock < sizeof(lapGyro) / sizeof(lapGyro[0]); ++luBlock)
            {
                u8* lpBytes = reinterpret_cast<u8*>(lapGyro[luBlock]);
                for (u32 luByte = 0; luByte < sizeof(Camera::BehaviourGyroCam::Parameters); ++luByte)
                {
                    lpBytes[luByte] = 0;
                }
                lapGyro[luBlock]->meType = Camera::eBehaviourGyroCam;
            }
        }

        // ⭐ ADDED 2026-08-29 (crash-camera wave). The spiralling-deathcam parameter block
        // ArbStateCrashing::Prepare @0x822655E8 hands to BehaviourSpirallingDeathcam::
        // SetParameters: `lwz r11, 0x1C(sharedInfo)` (mpNamedParameters) then
        // `addi r29, r11, 0x23B4`. It is the console's block at NamedParameters +0x23B4, i.e.
        // 0x80 past the look-around block above.
        //
        // ⚠️ IT IS DELIBERATELY *NOT* PLACED AT +0x23B4 HERE, and that is not sloppiness.
        // This reconstruction's LookAroundCarCamParameters is 136 bytes (0x88) against the
        // console block's 0x80 -- the Looker::Parameters slice inside it is modelled wider than
        // the console's -- so the two blocks cannot both sit at their console offsets in one
        // struct without forking a type. Parity here is BY NAMED MEMBER (the same rule the
        // arbitrator state container states for its embedded states): the block exists, it is
        // named, it is seeded, and the ONE consumer reaches it through the accessor. The
        // +0x23B4 above is provenance.
        const Camera::BehaviourSpirallingDeathcam::Parameters& GetSpirallingDeathcamParameters() const
        {
            return maSpirallingDeathcamParameters;
        }

        // ⭐⭐ THE FOURTEEN GYRO BLOCKS, PLACED AT THEIR EXACT RECORD OFFSETS. Slots 0..6 came
        // from MomentTumbling; slots 7..13 close the run (takedown-camera wave) and carry the
        // two blocks the takedown arbitrator states adopt. MomentTumbling::SetGyroCamParameters
        // picks one of six of these by Parameters::ESubType and hands it to
        // BehaviourGyroCam::SetParameters:
        //     E_SUBTYPE_LEAD           -> +480   mGyroCamDefaultParams
        //     E_SUBTYPE_TRUCKING_FRONT -> +684   mGyroCamTruckFront
        //     E_SUBTYPE_SIDE           -> +888   mGyroCamLeft
        //     E_SUBTYPE_TRUCKING_SIDE  -> +1296 / +1500  (the left/right alternation)
        //     E_SUBTYPE_FOLLOW         -> +1704  mGyroCamFollow
        // The six reads sit on an exact 204-byte grid == sizeof(BehaviourGyroCam::Parameters),
        // with one unread slot at +1092 between them, so the span is a run of consecutive
        // same-typed blocks and the names above are the record's own, in order. The semantics
        // corroborate the grid independently: the TRUCKING_FRONT subtype lands on the block
        // named TruckFront, and the TRUCKING_SIDE subtype's two-way alternation lands on the
        // pair named SideTruckingLeft / SideTruckingRight.
        //
        // ⭐ SLOTS 7..13, ADDED IN THE TAKEDOWN-CAMERA WAVE, ON THE SAME GRID AND WITH TWO
        // MORE INDEPENDENT CONSUMERS. Three takedown-player Prepare bodies read a gyro block
        // straight off ArbStateSharedInfo::mpNamedParameters (`lwz r11, +0x1C(sharedInfo)`
        // then an `addi` into the record) and hand it to BehaviourGyroCam::SetParameters:
        //     B3ClassicTakedownPlayer::Prepare        +0x840 == +2112
        //     DestructionPathTakedownPlayer::Prepare  +0x1E0 == +480, then +0x840 == +2112
        //     DriveByTakedownPlayer::Prepare          +0xB70 == +2928, twice
        // (2112 - 480) / 204 == 8 and (2928 - 480) / 204 == 12 exactly, so both land on the
        // grid with no remainder, at slots 8 and 12. The recovered type information's member
        // list for this record names the fourteen blocks in order, and slot 8 is
        // mGyroCamTakedownParams while slot 12 is mGyroCamDriveByLParams -- the takedown
        // states landing on the block named Takedown and the drive-by takedown landing on the
        // block named DriveBy is the same kind of semantic corroboration the subtype names
        // gave the first half. It also lands slot 11 on mGyroCamHelicamParams, which is
        // exactly the block this file already attributed to record +2724 from the hit-traffic
        // moment's own displacement -- a fourth consumer agreeing with the grid.
        //
        // ⓘ DriveByTakedownPlayer reads the SAME block for both of its behaviours; the
        // record's mGyroCamDriveByRParams (slot 13) has no reader in this build.
        //
        // Placing them at their real offsets costs nothing -- this reconstruction's gyro
        // Parameters is byte-exact (static_asserted below) -- so unlike the two by-name blocks
        // at the tail, these are BYTE-FAITHFUL, and the +0x2334 block below keeps its offset.
        // ⭐ THE RECORD'S FIRST BLOCK, CARVED 2026-09-11. The bank's own Construct opens by
        // constructing an aftertouch-cam block at record +0, and the behaviour factory hands
        // that same address to BehaviourAftertouchCam::SetParameters when an authored
        // aftertouchcam shot comes in -- so this is a named block, not head padding. The four
        // head blocks are {aftertouch-cam, aftertouch-crash, aftertouch-crash, helicam} at
        // record +0 / +108 / +220 / +332, each pinned by the bank Construct's own per-block
        // call, and together they are the 480 bytes the gyro run starts after. Only the first
        // is carved here (it is the only one with a consumer in this tree); the other three
        // stay inside the reserved remainder below, which is sized so the gyro run cannot
        // move whatever this block's reconstruction weighs.
        Camera::BehaviourAftertouchCam::Parameters mAftertouchCamDefault;   // +0

        // ⭐ THE TWO AFTERTOUCH-CRASH BLOCKS, CARVED 2026-09-12 (crash-mode wave).
        // ArbStateCrashMode::Prepare hands the FIRST of them to
        // BehaviourAftertouchCrash::SetParameters (the console reaches it as bank +0x7C, i.e.
        // record +108, the bank's record starting at bank +0x10). The names are this record's
        // own; the 112-byte stride between them is sizeof the crash Parameters block, which its
        // own Construct pins by writing every word out to +0x6C. The aftertouch-cam block above
        // is exactly 108 bytes, so the first of these starts right after it with no padding.
        Camera::BehaviourAftertouchCrash::Parameters mAftertouchCrashParams;   // +108
        Camera::BehaviourAftertouchCrash::Parameters mCrashDebugParams;        // +220
        u8 maReservedHead[480 - 220 - sizeof(Camera::BehaviourAftertouchCrash::Parameters)];  // +332 (helicam block)
        Camera::BehaviourGyroCam::Parameters mGyroCamDefaultParams;                  // +480
        Camera::BehaviourGyroCam::Parameters mGyroCamTruckFront;                     // +684
        Camera::BehaviourGyroCam::Parameters mGyroCamLeft;                           // +888
        Camera::BehaviourGyroCam::Parameters mGyroCamRight;                          // +1092
        Camera::BehaviourGyroCam::Parameters mGyroCamDefaultSideTruckingLeftParams;  // +1296
        Camera::BehaviourGyroCam::Parameters mGyroCamDefaultSideTruckingRightParams; // +1500
        Camera::BehaviourGyroCam::Parameters mGyroCamFollow;                         // +1704
        // ⭐ THE RUN'S SECOND HALF, CARVED (takedown-camera wave). Slots 7..13 complete the
        // fourteen-block gyro run at +480 .. +3336, all seven on the same 204-byte grid and
        // all seven named by the recovered type information, in the record's own order. Two of
        // them have attested consumers (see the accessors above); the other five are carried
        // because a run is only byte-faithful as a whole, and because slot 11 is the helicam
        // block the bank used to model as a separate by-name member.
        Camera::BehaviourGyroCam::Parameters mGyroCamAlwaysLowParams;                // +1908
        Camera::BehaviourGyroCam::Parameters mGyroCamTakedownParams;                 // +2112
        Camera::BehaviourGyroCam::Parameters mGyroCamTakedownZoomedOutParams;        // +2316
        Camera::BehaviourGyroCam::Parameters mGyroCamHighParams;                     // +2520
        Camera::BehaviourGyroCam::Parameters mGyroCamHelicamParams;                  // +2724
        Camera::BehaviourGyroCam::Parameters mGyroCamDriveByLParams;                 // +2928
        Camera::BehaviourGyroCam::Parameters mGyroCamDriveByRParams;                 // +3132
        // The remaining reserved span carries the addressed block to the attested +0x2334 (it
        // lands at +9016 rather than +9012 -- see the note under the asserts below). The rest
        // of the record (the bystander / rig / failsafe / passenger / fixed
        // blocks) is not modelled here -- see the RECORD MAP in the BehaviourParameterBank
        // banner below for every one of their offsets.
        u8                         maReserved0D08[8696 - 3336];      // +3336 .. +8695
        // ⭐⭐ THE THREE LOOSE-ATTACHMENT BLOCKS, CARVED (takedown-camera wave), at their exact
        // record offsets. ShutdownTakedownPlayer::Update reaches each one off the shared
        // context's named-parameter record (`lwz` the record pointer, then an `addi` into it) and
        // hands it to BehaviourLooseAttachment::SetParameters:
        //     beat 1  record +8696
        //     beat 2  record +8796
        //     beat 3  record +8896
        // 100 apart, and 100 is exactly sizeof(BehaviourLooseAttachment::Parameters) (asserted
        // below), so the three sit on their own exact grid. THE RUN CLOSES WITH NO SLACK at both
        // ends, which is what makes the placement forced rather than fitted: the RECORD MAP below
        // already puts mPassengerDefault at +8660 and mFixedDefault at +8996 from four unrelated
        // consumers, and 8896 + 100 == 8996 exactly. The names are the record's own, in order --
        // the recovered type information lists exactly three consecutive
        // BehaviourLooseAttachment::Parameters members between mPassengerDefault and
        // mFixedDefault, which is the same count the bank serialiser's walk order gives.
        Camera::BehaviourLooseAttachment::Parameters mLooseAttachmentTakedown1;  // +8696
        Camera::BehaviourLooseAttachment::Parameters mLooseAttachmentTakedown2;  // +8796
        Camera::BehaviourLooseAttachment::Parameters mLooseAttachmentTakedown3;  // +8896
        u8                         maReserved22C4[0x2334 - 8996];    // +8996 .. +0x2333 (mFixedDefault)
        LookAroundCarCamParameters maLookAroundCarCamParameters;     // +0x2334
        Camera::BehaviourSpirallingDeathcam::Parameters
                                   maSpirallingDeathcamParameters;   // console +0x23B4 (see note)
    };

    // The grid the fourteen gyro placements rest on, ratcheted so a future widening of the gyro
    // parameter block cannot silently slide them off their attested offsets.
    static_assert(sizeof(Camera::BehaviourGyroCam::Parameters) == 204,
                  "BehaviourGyroCam::Parameters is the 204-byte grid the tumbling blocks sit on");

    // The 112-byte stride the record's two aftertouch-crash head blocks sit on (record +108 and
    // +220). Ratcheted so a future widening of that block cannot slide either off its offset.
    static_assert(sizeof(Camera::BehaviourAftertouchCrash::Parameters) == 112,
                  "BehaviourAftertouchCrash::Parameters is the 112-byte head-run stride");
    static_assert(sizeof(Camera::BehaviourAftertouchCam::Parameters) == 108,
                  "BehaviourAftertouchCam::Parameters is the record's 108-byte first block");
    static_assert(offsetof(NamedParameters, mAftertouchCamDefault) == 0,
                  "NamedParameters::mAftertouchCamDefault @ +0 (the record's first block)");
    static_assert(offsetof(NamedParameters, mGyroCamDefaultParams) == 480,
                  "NamedParameters::mGyroCamDefaultParams @ +480 (E_SUBTYPE_LEAD)");
    static_assert(offsetof(NamedParameters, mGyroCamTruckFront) == 684,
                  "NamedParameters::mGyroCamTruckFront @ +684 (E_SUBTYPE_TRUCKING_FRONT)");
    static_assert(offsetof(NamedParameters, mGyroCamLeft) == 888,
                  "NamedParameters::mGyroCamLeft @ +888 (E_SUBTYPE_SIDE)");
    static_assert(offsetof(NamedParameters, mGyroCamDefaultSideTruckingLeftParams) == 1296,
                  "NamedParameters::mGyroCamDefaultSideTruckingLeftParams @ +1296");
    static_assert(offsetof(NamedParameters, mGyroCamDefaultSideTruckingRightParams) == 1500,
                  "NamedParameters::mGyroCamDefaultSideTruckingRightParams @ +1500");
    static_assert(offsetof(NamedParameters, mGyroCamFollow) == 1704,
                  "NamedParameters::mGyroCamFollow @ +1704 (E_SUBTYPE_FOLLOW)");
    static_assert(offsetof(NamedParameters, mGyroCamAlwaysLowParams) == 1908,
                  "NamedParameters::mGyroCamAlwaysLowParams @ +1908 (gyro slot 7)");
    static_assert(offsetof(NamedParameters, mGyroCamTakedownParams) == 2112,
                  "NamedParameters::mGyroCamTakedownParams @ +2112 (the takedown states' block)");
    static_assert(offsetof(NamedParameters, mGyroCamTakedownZoomedOutParams) == 2316,
                  "NamedParameters::mGyroCamTakedownZoomedOutParams @ +2316 (gyro slot 9)");
    static_assert(offsetof(NamedParameters, mGyroCamHighParams) == 2520,
                  "NamedParameters::mGyroCamHighParams @ +2520 (gyro slot 10)");
    static_assert(offsetof(NamedParameters, mGyroCamHelicamParams) == 2724,
                  "NamedParameters::mGyroCamHelicamParams @ +2724 (the hit-traffic moment's block)");
    static_assert(offsetof(NamedParameters, mGyroCamDriveByLParams) == 2928,
                  "NamedParameters::mGyroCamDriveByLParams @ +2928 (the drive-by takedown's block)");
    static_assert(offsetof(NamedParameters, mGyroCamDriveByRParams) == 3132,
                  "NamedParameters::mGyroCamDriveByRParams @ +3132 (gyro slot 13, no reader)");
    static_assert(offsetof(NamedParameters, maReserved0D08) == 3336,
                  "the gyro run closes at +3336, where the bystander run starts");
    // The 100-byte grid the three loose-attachment placements rest on, ratcheted so a future
    // widening of that block cannot silently slide them off their attested offsets.
    static_assert(sizeof(Camera::BehaviourLooseAttachment::Parameters) == 100,
                  "BehaviourLooseAttachment::Parameters is the 100-byte loose-attachment stride");
    static_assert(offsetof(NamedParameters, mLooseAttachmentTakedown1) == 8696,
                  "NamedParameters::mLooseAttachmentTakedown1 @ +8696 (shutdown-takedown beat 1)");
    static_assert(offsetof(NamedParameters, mLooseAttachmentTakedown2) == 8796,
                  "NamedParameters::mLooseAttachmentTakedown2 @ +8796 (shutdown-takedown beat 2)");
    static_assert(offsetof(NamedParameters, mLooseAttachmentTakedown3) == 8896,
                  "NamedParameters::mLooseAttachmentTakedown3 @ +8896 (shutdown-takedown beat 3)");
    static_assert(offsetof(NamedParameters, maReserved22C4) == 8996,
                  "the loose-attachment run closes at +8996, where mFixedDefault starts");
    // ⓘ The look-around block below the gyro run is NOT asserted, because on this host it
    // does not land on its console offset and never has: BehaviourRotateAboutVehicle::
    // Parameters inherits the Behaviour::Parameters head, whose debug-name POINTER is 8 bytes
    // here against the console build's 4, so the type is 8-aligned and the block sits at
    // +9016 rather than +9012. That is the project's ordinary host-pointer-width divergence,
    // and it is why the two tail blocks are by-name rather than placed. The gyro blocks above
    // are pointer-free, which is what lets them be byte-exact.

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
        // ⓘ HOST WIDTH INSIDE THE RECORD. Every record offset above is byte-exact through the
        // gyro run; below it the reconstruction runs 4 bytes long, because the shared
        // Behaviour::Parameters head carries a debug-name POINTER that is 8 bytes wide on this
        // host against the console's 4, so the rotate-about-vehicle ("look around") block
        // lands at record +9016 rather than the console's +9012 and the record ends past
        // +9328. That is the project's ordinary host-pointer-width divergence and it predates
        // the record's move into this class; it is why the two tail blocks are reached by name
        // and why bank +0x2480 and below stay provenance rather than placements.
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
        // [FLAG PC bring-up] THIS IS STILL A THREE-SLOT SLICE of a bank that holds ~40 named
        // blocks. The other accessors below stay DECLARATION-ONLY and their blocks are not
        // placed. x64 parity is BY NAMED MEMBER, so no reserved head is invented to reproduce
        // +0x2480 -- the console displacements above are provenance only.
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
            void Construct()
            {
                // The named-parameter record at +0x10 -- the console's first statement in this
                // function is the first block of this record. Seeding it here is what makes the
                // bank the record's single owner: nothing outside constructs it any more.
                // [FLAG, PC-only] the unmodelled head above it is zeroed for the same reason
                // the two ZeroBlocks below are: no PC consumer may read indeterminate storage.
                ZeroBlock(maReservedBankHead, sizeof(maReservedBankHead));
                mNamedParameters.Construct();

                ZeroBlock(&mGameplayExternalCameraParamsForCar,
                          sizeof(mGameplayExternalCameraParamsForCar));
                ZeroBlock(&mGameplayBumperCameraParamsForCar,
                          sizeof(mGameplayBumperCameraParamsForCar));

                mxGameplayCameraCarAttribsKey = 0;                       // std 0, 0x2480
                mGameplayExternalCameraParamsForCar.Construct();         // over +0x2488
                mGameplayBumperCameraParamsForCar.Construct();           // over +0x2538

                // ⭐ 2026-09-11: the remaining moment camera blocks. None of their Parameters
                // classes has a recovered Construct, so each is zeroed and -- where the tag
                // is reachable -- stamped with the type tag its SetParameters asserts on.
                // Same [FLAG, PC-only] posture as the two ZeroBlocks above: the console's
                // authored tunings for these blocks are compiled into the bank's own
                // Construct, which is not recovered, so every block reads as a zeroed rig.
                ZeroBlock(&mBystanderCloseParameters, sizeof(mBystanderCloseParameters));
                ZeroBlock(&mBystanderFarParameters,   sizeof(mBystanderFarParameters));
                ZeroBlock(&mPassengerDefault,         sizeof(mPassengerDefault));
                ZeroBlock(&mFixedDefault,             sizeof(mFixedDefault));

                mBystanderCloseParameters.meType = eBehaviourBystanderCam;
                mBystanderFarParameters.meType   = eBehaviourBystanderCam;
                mFixedDefault.meType             = eBehaviourFixedCam;

                // ⭐ 2026-09-12: the eleven player-jumping shot blocks, same posture.
                // ⚠ THE NINE RIG BLOCKS' TYPE TAGS ARE NOT SEEDED and cannot be from here:
                // BehaviourRig::Parameters inherits the shared Behaviour::Parameters head,
                // whose mType is protected, and the only thing that writes it is
                // BehaviourRig::Parameters::Construct -- which lives in the unmounted
                // BehaviourRig.cpp and sets it to 0 anyway (the console's authored tunings,
                // tag included, come from the bank's own compiled-in Construct, which is not
                // recovered). Nothing can reach these blocks yet either: the only consumer is
                // MomentPlayerJumping::Prepare, whose TU is not in the link. Inert, not wrong.
                // DELETE-WHEN: BehaviourRig.cpp is mounted, and Construct calls
                // BehaviourRig::Parameters::Construct on each of the nine instead of zeroing.
                ZeroBlock(&mBystanderJumpLeftParameters,       sizeof(mBystanderJumpLeftParameters));
                ZeroBlock(&mBystanderJumpFromBehindParameters, sizeof(mBystanderJumpFromBehindParameters));
                ZeroBlock(&mRigRearQFwd,        sizeof(mRigRearQFwd));
                ZeroBlock(&mRigFrontQCuFwd,     sizeof(mRigFrontQCuFwd));
                ZeroBlock(&mRigBootViewFwd,     sizeof(mRigBootViewFwd));
                ZeroBlock(&mRigRoofFwd,         sizeof(mRigRoofFwd));
                ZeroBlock(&mRigFrontQCuFwd2,    sizeof(mRigFrontQCuFwd2));
                ZeroBlock(&mRigUnderbelly,      sizeof(mRigUnderbelly));
                ZeroBlock(&mRigDropUnderbelly,  sizeof(mRigDropUnderbelly));
                ZeroBlock(&mRigDropFrontQCuFwd, sizeof(mRigDropFrontQCuFwd));
                ZeroBlock(&mRigDropBootViewFwd, sizeof(mRigDropBootViewFwd));

                mBystanderJumpLeftParameters.meType       = eBehaviourBystanderCam;
                mBystanderJumpFromBehindParameters.meType = eBehaviourBystanderCam;
            }

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
            // Record +2724 is gyro slot 11 == mGyroCamHelicamParams, and the record now
            // places the whole fourteen-block gyro run, so this returns the record's own
            // block rather than a second by-name copy of it.
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
                return mFixedDefault;
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
                return mBystanderCloseParameters;
            }
            const BehaviourBystanderCam::Parameters& GetBystanderCamMomentParams() const
            {
                return mBystanderFarParameters;
            }

            // The passenger-sees-action camera block (MomentPassengerSeesAction::Update
            // hands manager+83732 == record +8660 to BehaviourPassengerCam::SetParameters).
            // Record +8660 is mPassengerDefault.
            // ⚠ ITS TYPE TAG IS NOT SEEDED and cannot be from here: the tag lives in the
            // protected Behaviour::Parameters head and BehaviourPassengerCam::Parameters::
            // Construct is that class's own ledger function, declaration-only in this tree.
            // Nothing can reach the block yet either -- BehaviourPassengerCam::SetParameters
            // is declaration-only too -- so this is inert rather than wrong.
            // DELETE-WHEN: BehaviourPassengerCam::Parameters::Construct lands, and Construct
            // below calls it instead of zeroing the block.
            const BehaviourPassengerCam::Parameters& GetPassengerCamMomentParams() const
            {
                return mPassengerDefault;
            }

            // ⭐ THE PLAYER-JUMPING SHOT BLOCKS, CARVED 2026-09-12. These two were the last
            // moment camera accessors left declaration-only: they are INDEXED, and this class
            // models blocks by name rather than as the record's arrays. They are bodied now
            // as a switch over the RECORD SLOT INDEX, and the eleven blocks the jump moment
            // names are real members below (the same by-name parity the four blocks above
            // have -- placing the runs at their record offsets is still impossible here, see
            // the members' own note).
            //
            // The index base the call sites used WAS off by one, and is corrected in the same
            // change. The eleven attested manager displacements are
            //   rigs      79792 80080 81520 82384 82096 80944  (attached collection)
            //             82672 82960 83248                    (dropped collection)
            //   bystander 78408 78720
            // Against the record map (rig run at record +4432 stride 288, bystander run at
            // record +3336 stride 156, bank == record + 0x10) those are rig slots
            // {1,2,7,10,9,5} + {11,12,13} and bystander slots {0,2} -- mRigRearQFwd /
            // mRigFrontQCuFwd / mRigRoofFwd / mRigUnderbelly / mRigFrontQCuFwd2 /
            // mRigBootViewFwd, then the three blocks whose own names begin "Drop" feeding the
            // DROPPED collection (which is what makes the +1 unambiguous), and the two
            // bystander blocks whose own names begin "Jump" feeding the jump moment.
            // BrnMomentPlayerJumping.cpp used to pass {0,1,6,9,8,4} / {10,11,12} / {0,1};
            // it now passes the slot numbers above.
            //
            // [FLAG, PC-only] the `default:` arm. The console has no such function at all --
            // it inlines each of the eleven reads to its own fixed displacement -- so there is
            // no console behaviour for an index outside the eleven. The arm exists only so a
            // future caller cannot read past the end of this class; it returns the first block
            // of the run it is asked for and fires the assert.
            const BehaviourRig::Parameters& GetPlayerJumpingRigShotParams(s32 liIndex) const
            {
                switch (liIndex)
                {
                case 1:  return mRigRearQFwd;
                case 2:  return mRigFrontQCuFwd;
                case 5:  return mRigBootViewFwd;
                case 7:  return mRigRoofFwd;
                case 9:  return mRigFrontQCuFwd2;
                case 10: return mRigUnderbelly;
                case 11: return mRigDropUnderbelly;
                case 12: return mRigDropFrontQCuFwd;
                case 13: return mRigDropBootViewFwd;
                default: break;
                }
                CGS_ASSERT(false, "GetPlayerJumpingRigShotParams: unmodelled rig slot");
                return mRigRearQFwd;
            }
            const BehaviourBystanderCam::Parameters& GetPlayerJumpingBystanderShotParams(s32 liIndex) const
            {
                switch (liIndex)
                {
                case 0: return mBystanderJumpLeftParameters;
                case 2: return mBystanderJumpFromBehindParameters;
                default: break;
                }
                CGS_ASSERT(false, "GetPlayerJumpingBystanderShotParams: unmodelled bystander slot");
                return mBystanderJumpLeftParameters;
            }

            // X360 0x822732D0. Dumps the whole parameter bank to the debug text file
            // "d:\\camera.txt". The X360 compiler inlines TextFileWriteSerialiser::
            // Construct("d:\\camera.txt") (fopen "w", muRecursionDepth = 0) and Destruct()
            // (CGS_ASSERT muRecursionDepth == 0; fclose) into this body; reconstructed as the
            // three calls the source made. Lives in BrnBehaviourParameterBank.cpp.
            void SaveParameters();

            // The bank's serialiser-visitor template: walks every named Parameters sub-block,
            // handing each field to the supplied serialiser. Attested by the X360 mangled call
            // in SaveParameters (`public: void Serialise<TextFileWriteSerialiser>(
            // TextFileWriteSerialiser&)`). The per-instantiation bodies are separate (still-todo)
            // TUs; declared here so SaveParameters can call it. T is deduced from the argument.
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
            // The bank's leading sub-record. The 16 bytes ahead of it are the bank's own head
            // (the version word and its pad up to the record's alignment); nothing in this
            // slice reads them, so they are a named reserved span rather than typed members --
            // but they are REAL bytes, so the record starts at +0x10 exactly as derived, and
            // the offsetof ratchet below fails the build if that ever stops being true.
            u8              maReservedBankHead[0x10];             // +0x0000 .. +0x000F
            NamedParameters mNamedParameters;                     // +0x0010

            // ---- the three homed slots (see the banner for the pin) -----------------------
            u64                                   mxGameplayCameraCarAttribsKey;        // +0x2480
            BehaviourGameplayExternal::Parameters mGameplayExternalCameraParamsForCar;  // +0x2488
            BehaviourGameplayBumper::Parameters   mGameplayBumperCameraParamsForCar;    // +0x2538

            // ---- the remaining moment camera blocks --------------------------------------
            // On the console these live inside the bank's mNamedParameters sub-record, at the
            // record offsets in the comments (bank offset == record offset + 0x10). The record
            // IS modelled now (mNamedParameters above), but each of these four offsets falls
            // inside one of its reserved spans, and carving them out would mean placing runs
            // whose Parameters this tree models NARROWER than the console's (the bystander and
            // rig strides) -- a type widening, not a span edit. So they stay where they are and
            // parity is BY NAMED MEMBER, as for every other block in this slice: each block
            // exists under its own record name, is seeded, and its one consumer reaches it
            // through the accessor above. They are not a second copy of anything the record
            // holds -- the record does not model these four slots at all. (The helicam block
            // that used to head this list is gone: the gyro run IS placed now, so
            // GetGyroCamMomentParams returns the record's own slot 11.)
            // DELETE-WHEN: the bystander / passenger / fixed runs are placed inside
            // mNamedParameters; then these members go and the accessors return record blocks.
            BehaviourBystanderCam::Parameters mBystanderCloseParameters;   // record +3804
            BehaviourBystanderCam::Parameters mBystanderFarParameters;     // record +4116
            BehaviourPassengerCam::Parameters mPassengerDefault;           // record +8660
            BehaviourFixedCam::Parameters     mFixedDefault;               // record +8996

            // ---- the eleven player-jumping shot blocks (2026-09-12) ----------------------
            // Same by-name posture, same reason: they are the bystander run's slots 0 and 2
            // and the rig run's slots 1/2/5/7/9/10 (attached) + 11/12/13 (dropped), and both
            // runs are modelled NARROWER here than the console's 156 / 288 strides, so they
            // cannot be placed inside mNamedParameters without widening two types. Each block
            // exists under the record's own name for that slot, is seeded by Construct below,
            // and its one consumer (MomentPlayerJumping::Prepare) reaches it through the two
            // indexed accessors above. The record offsets in the comments are provenance.
            // DELETE-WHEN: the rig and bystander runs are placed inside mNamedParameters.
            BehaviourBystanderCam::Parameters mBystanderJumpLeftParameters;       // record +3336
            BehaviourBystanderCam::Parameters mBystanderJumpFromBehindParameters; // record +3648
            BehaviourRig::Parameters          mRigRearQFwd;                       // record +4720
            BehaviourRig::Parameters          mRigFrontQCuFwd;                    // record +5008
            BehaviourRig::Parameters          mRigBootViewFwd;                    // record +5872
            BehaviourRig::Parameters          mRigRoofFwd;                        // record +6448
            BehaviourRig::Parameters          mRigFrontQCuFwd2;                   // record +7024
            BehaviourRig::Parameters          mRigUnderbelly;                     // record +7312
            BehaviourRig::Parameters          mRigDropUnderbelly;                 // record +7600
            BehaviourRig::Parameters          mRigDropFrontQCuFwd;                // record +7888
            BehaviourRig::Parameters          mRigDropBootViewFwd;                // record +8176
        };

        // NEVER CALLED. The record is the one part of this slice whose bank offset is
        // byte-exact, and the whole derivation in the banner rests on it: every attested
        // displacement in the tree is `record offset + 0x10`. If a future edit puts a member
        // ahead of the record, or widens the head, the build fails here instead of quietly
        // re-basing every consumer's arithmetic.
        inline void BehaviourParameterBank::_AssertBankLayout()
        {
            static_assert(offsetof(BehaviourParameterBank, mNamedParameters) == 0x10,
                          "BehaviourParameterBank::mNamedParameters @ bank +0x10");
        }
    }
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H
