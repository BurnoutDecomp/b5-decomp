#pragma once

// =================================================================================================
// GameSource/Physics/VehicleManager/BrnPotentialContactAverager.h
//
// BrnPhysics::Vehicle::PotentialContactAverager -- the per-frame accumulator DoCrashPrediction
// hands to every car-vs-car / car-vs-world potential-contact handler. It folds the many
// scene-manager contacts a single ENTITY PAIR generates in one frame into ONE averaged contact,
// so the crash/takedown chain fires once per pair instead of once per triangle/volume overlap.
//
// The DecFIGS DWARF only FORWARD-declares this type (BrnVehicleConstants.h:1597 / :1845 /
// Wheel.h:3268) -- the console defines it inside BrnVehicleManager.cpp, which is why its four
// asserts cite that file (:422 / :493 / :494 / :503). The member layout below is therefore
// recovered from the X360 asm of its three methods, which pin every offset:
//
//   0x825B4FD0  FindSlotForContact       (50)   stride 0x50 walk, +0x30/+0x38 id compares
//   0x825C3218  AddContactPair          (108)   +0x690 count, +0x640 per-slot weights
//   0x825C34D0  GetAveragedContactPoint (135)   80-byte copy, weight divide, renormalise
//
//   maContactPairs[20]        @0x0000   stride 0x50 == sizeof(PotentialContact)
//   mafContactPairsCounts[20] @0x0640   (4 * (index + 400) + this)
//   muContactPairCount        @0x0690   (1680)
//
// The 20-slot cap is the `cmplwi r?, 0x14` bound AddContactPair returns false on.
// =================================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"

namespace BrnPhysics
{
namespace Vehicle
{
    struct alignas(16) PotentialContactAverager
    {
        // The console cap; the `>= 0x14` early-out in AddContactPair.
        static const u32 KU_MAX_CONTACT_PAIRS = 20;

        // @0x825B4FD0. Linear search for a slot already holding this ENTITY PAIR, in either
        // order. Writes *lpbOutSwapped = true when the stored slot holds (B,A) rather than
        // (A,B); returns -1 when the pair is new. Only the two VolumeInstanceIds' embedded
        // ENTITY words are compared (`ld ; srdi 32 ; clrlwi`) -- the volume index is ignored.
        s32 FindSlotForContact(CgsSceneManager::SceneManagerIO::PotentialContact lContact,
                               bool* lpbOutSwapped) const;

        // @0x825C3218. Fold lContact into its pair's slot (or claim a new one). Returns false
        // ONLY when a new slot was needed and all 20 are taken.
        bool AddContactPair(CgsSceneManager::SceneManagerIO::PotentialContact lContact);

        // @0x825C34D0. Read slot luIndex back as one averaged contact: the two points divided
        // by the slot's accumulated weight and the summed normal renormalised.
        void GetAveragedContactPoint(u32 luIndex,
                                     CgsSceneManager::SceneManagerIO::PotentialContact& lrOutContact) const;

        // Not an X360 symbol: the console clears the accumulator by storing 0 to +0x690 at the
        // tail of each Handle*CrashPrediction* driver (`stw r24, 0x690(r30)` @0x82640C14).
        void Reset() { muContactPairCount = 0; }

        CgsSceneManager::SceneManagerIO::PotentialContact maContactPairs[KU_MAX_CONTACT_PAIRS];   // +0x0000
        f32                                              mafContactPairsCounts[KU_MAX_CONTACT_PAIRS]; // +0x0640
        u32                                              muContactPairCount;                     // +0x0690
    };
}
}
