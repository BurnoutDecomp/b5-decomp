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

// ============================================================================
// GameSource/Director/Camera/BrnBehaviourParameterBank.h
//
// BrnDirector::NamedParameters -- the director's bank of per-named-behaviour camera
// "Parameters" blocks (aftertouch, gyro, bystander, rig, rotate-about-vehicle, ...). The
// arbitrator-state shared context (ArbStateSharedInfo::mpNamedParameters) holds it BY POINTER
// and an arbitrator state reaches one named block out of it to configure a behaviour it has
// just allocated.
//
// FLAG: MINIMAL SLICE. The full bank (every BehaviourXxx::Parameters sub-block, the
//   BehaviourParameterBank wrapper + its serialiser) is a heavy cascade and has no
//   reconstructed home of its own yet. This header models ONLY the one named accessor this
//   build's online-car-select arbitrator state needs -- the "look around car" (rotate-about-
//   vehicle) parameter block -- accessed BY NAME via its address. The DecFIGS DWARF / Feb-2007
//   source name the bank's earlier blocks but NOT this one (the rotate-about-vehicle params
//   were added after the Feb-2007 cut), so the block's precise type is unrecoverable; it is
//   modelled as a named opaque sub-object at the asm-attested offset and only its address is
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
            maLookAroundCarCamParameters.Construct();

            // The bank's own four post-Construct re-tunes -- see the banner.
            maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectXSize         =  0.75f;
            maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectYSize         =  0.75f;
            maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectXScreenOffset =  0.125f;
            maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectYScreenOffset = -0.125f;
        }

        // The reserved span places the addressed block at the asm-attested +0x2334. The rest of
        // the bank (the earlier named Parameters blocks) is not modelled here.
        u8                         maReservedHead[0x2334];           // +0x0000 .. +0x2333
        LookAroundCarCamParameters maLookAroundCarCamParameters;     // +0x2334
    };

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
        // Relationship to BrnDirector::NamedParameters (above) is NOT pinned: the arbitrator
        // shared context reaches named parameter blocks through mpNamedParameters (+0x2334
        // block) and the manager reaches these two through its own +0x12530 bank; whether
        // those are the same object is left to the bank's own TU.
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
                ZeroBlock(&mGameplayExternalCameraParamsForCar,
                          sizeof(mGameplayExternalCameraParamsForCar));
                ZeroBlock(&mGameplayBumperCameraParamsForCar,
                          sizeof(mGameplayBumperCameraParamsForCar));

                mxGameplayCameraCarAttribsKey = 0;                       // std 0, 0x2480
                mGameplayExternalCameraParamsForCar.Construct();         // over +0x2488
                mGameplayBumperCameraParamsForCar.Construct();           // over +0x2538
            }

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

            // X360 bank+0xAB4 (manager+0x12FE4): the gyro-cam block the hit-traffic
            // moment binds (MomentHitTraffic::Update @0x82271EB8 hands it to
            // BehaviourGyroCam::SetParameters). FLAG: accessor name inferred by the
            // sibling naming pattern (the X360 inlines the fetch to the fixed offset).
            const BehaviourGyroCam::Parameters& GetGyroCamMomentParams() const;

            // X360 bank+0x2334 (manager+0x14864): the fixed-cam block the static-cam-
            // impact moment binds (MomentStaticCamImpact::Update @0x82266C00 hands it
            // to BehaviourFixedCam::SetParameters). FLAG: accessor name inferred; note
            // +0x2334 coincides with NamedParameters::maLookAroundCarCamParameters
            // (see the relationship note above -- left to the bank's own TU).
            const BehaviourFixedCam::Parameters& GetStaticCamImpactCamParams() const;

            // The two bystander-sees-action camera blocks (MomentBystanderSeesAction::
            // Update @0x82266730 picks by its Parameters::mbCloseCamera and feeds the
            // block to BehaviourBystanderCam::SetParameters). X360 bank +0xEEC (close)
            // / +0x1024 (manager +78876 / +79188). FLAG: getter names inferred from
            // that role. DECLARATION-ONLY (the bank's own TU bodies them).
            const BehaviourBystanderCam::Parameters& GetBystanderCamCloseMomentParams() const;   // +0xEEC
            const BehaviourBystanderCam::Parameters& GetBystanderCamMomentParams() const;        // +0x1024

            // The passenger-sees-action camera block (MomentPassengerSeesAction::
            // Update @0x8225EEB8 hands manager+83732 == bank+0x21E4 to
            // BehaviourPassengerCam::SetParameters). FLAG: getter name inferred.
            // DECLARATION-ONLY (the bank's own TU bodies it).
            const BehaviourPassengerCam::Parameters& GetPassengerCamMomentParams() const;        // +0x21E4

            // The player-jumping moment's shot parameter blocks (MomentPlayerJumping::
            // Prepare @0x82251048 AddShots). The X360 inlines fixed bank offsets on a
            // sizeof grid -- rig blocks at bank + 0x1280 + 0x120*liIndex (Prepare feeds
            // indices {0,1,6,9,8,4} to the attached-rig collection and {10,11,12} to the
            // dropped-rig collection), bystander blocks at bank + 0xD18 + 0x138*liIndex
            // (indices {0,1}). FLAG: accessor names + the index grids inferred (the
            // 0x120/0x138 strides match the blocks' spacings == the Parameters sizes;
            // the individual offsets are asm-attested). DECLARATION-ONLY (the bank's
            // own TU bodies them). BehaviourRig::Parameters comes in through the
            // BehaviourPassengerCam.h include's BehaviourRig.h base slice.
            const BehaviourRig::Parameters&          GetPlayerJumpingRigShotParams(s32 liIndex) const;
            const BehaviourBystanderCam::Parameters& GetPlayerJumpingBystanderShotParams(s32 liIndex) const;

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

            // ---- the three homed slots (see the banner for the pin) -----------------------
            u64                                   mxGameplayCameraCarAttribsKey;        // +0x2480
            BehaviourGameplayExternal::Parameters mGameplayExternalCameraParamsForCar;  // +0x2488
            BehaviourGameplayBumper::Parameters   mGameplayBumperCameraParamsForCar;    // +0x2538
        };
    }
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_PARAMETER_BANK_H
