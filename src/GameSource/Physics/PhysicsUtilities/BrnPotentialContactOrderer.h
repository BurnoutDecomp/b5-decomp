#pragma once

#include "types.hpp"          // s32 / f32
#include "BrnCommonTypes.h"   // Vector3
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // CgsSceneManager::SceneManagerIO::PotentialContact

// BrnPhysics::PotentialContactOrderer -- keeps a small (<= 5) list of candidate contacts
// ordered by ascending impact time, then prunes the list so no kept contact sits behind an
// earlier kept contact's normal plane. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (BrnPotentialContactOrderer.{h,cpp}).
//
// LAYOUT (asm-attested displacements):
//   +0x00  ContactEntry maContactEntries[KI_MAX_CONTACTS]   (stride 12: ptr@+0, time@+4, valid@+8)
//   +0x3C  s32          miNumEntries                        (live entry count)

namespace BrnPhysics
{
    // The broad-phase potential-contact record. The orderer keeps pointers to these and only
    // reads their contact point (mPointOnB @+16) and normal (mNormal @+32). This is the SAME record
    // its clients pass in (VehicleManager::HandleCrashPredictionForRaceCarAndWorld feeds contacts
    // straight from the scene-manager potential-contact queue), so the type is unified onto its
    // canonical home CgsSceneManager::SceneManagerIO::PotentialContact (was a minimal 16+16+16 stand-in;
    // the read fields land at the identical byte offsets mPointOnB @+16 / mNormal @+32).
    typedef CgsSceneManager::SceneManagerIO::PotentialContact PotentialContact;

    struct PotentialContactOrderer
    {
        // The list holds at most 5 entries (KI_MAX_CONTACTS).
        static const s32 KI_MAX_CONTACTS = 5;

        // Reset the orderer to empty (miNumEntries = 0). The X360 inlines this at each call site
        // (e.g. the crash-prediction driver clears the orderer at start and after every group flush,
        // asm `stw 0, +0x3C`). DWARF PotentialContactOrderer.h:44.
        void Construct() { miNumEntries = 0; }

        // @ 0x825C0B30 -- ordered insert (by ascending impact time), dropping overflow.
        void AddContact(const PotentialContact* lpContact, f32 lfImpactTime);

        // @ 0x825B37B8 -- return the liIndex-th VALID contact (skips invalidated entries).
        const PotentialContact* GetContact(s32 liIndex);

        // @ 0x825B36E8 -- prune contacts behind an earlier kept contact's normal plane;
        // returns the number of entries still valid.
        s32 SortContacts();

    private:
        struct ContactEntry
        {
            const PotentialContact* mpContact;    // +0
            f32                     mfImpactTime; // +4
            bool                    mbValid;      // +8
        };  // stride 12

        ContactEntry maContactEntries[KI_MAX_CONTACTS]; // +0x00
        s32          miNumEntries;                       // +0x3C
    };
}
