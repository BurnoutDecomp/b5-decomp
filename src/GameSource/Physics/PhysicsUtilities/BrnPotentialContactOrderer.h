#pragma once

#include "types.hpp"          // s32 / f32
#include "BrnCommonTypes.h"   // Vector3

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
    // FLAG (foreign type): the broad-phase potential-contact record. Its full body is homed
    // elsewhere; this orderer only reads its contact point (mPointOnB @+16) and normal
    // (mNormal @+32), so it is modelled here as a minimal record with those two Vector3 fields
    // at their asm-attested offsets plus opaque padding. Member offsets are SEMANTIC (the two
    // read fields are exact); the record's full width is deferred to its owning TU.
    struct PotentialContact
    {
        u8      maOpaqueHead[16];   // +0x00 .. +0x10
        Vector3 mPointOnB;          // +0x10 (16)
        Vector3 mNormal;            // +0x20 (32)
    };

    struct PotentialContactOrderer
    {
        // The list holds at most 5 entries (KI_MAX_CONTACTS).
        static const s32 KI_MAX_CONTACTS = 5;

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
