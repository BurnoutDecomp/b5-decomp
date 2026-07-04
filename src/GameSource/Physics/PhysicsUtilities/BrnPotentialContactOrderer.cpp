#include "GameSource/Physics/PhysicsUtilities/BrnPotentialContactOrderer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"               // rw::math::vpu::Dot / operator-

// BrnPhysics::PotentialContactOrderer -- reconstructed from BURNOUT_X360_ARTIST.XEX.

namespace BrnPhysics
{

// X360 0x825C0B30: ordered insert of one candidate contact into maContactEntries,
// keeping the array sorted by ascending mfImpactTime (earliest impact first). The
// array holds at most KI_MAX_CONTACTS(5) entries; miNumEntries is clamped and only
// bumped while below 5. On overflow the last entry is simply dropped by the clamp.
// NOTE (faithful): only mpContact + mfImpactTime are written on insert; mbValid is
// not touched here (SortContacts seeds/uses the valid flags).
void PotentialContactOrderer::AddContact(const PotentialContact* lpContact, f32 lfImpactTime)
{
    s32 liInsert = 0;

    if (miNumEntries > 0)
    {
        // Find the insertion slot: advance while the new impact time is no earlier
        // than the entry already there (a3 >= entry.mfImpactTime).
        while (lfImpactTime >= maContactEntries[liInsert].mfImpactTime)
        {
            ++liInsert;
            if (liInsert >= miNumEntries)
            {
                // Append at the end if there is room.
                if (miNumEntries < KI_MAX_CONTACTS)
                {
                    maContactEntries[miNumEntries].mpContact    = lpContact;
                    maContactEntries[miNumEntries].mfImpactTime = lfImpactTime;
                    ++miNumEntries;
                }
                return;
            }
        }

        // Shift the tail up by one entry to open the insertion slot. The top of the
        // shift is capped at KI_MAX_CONTACTS-1 (=4) so an overflowing entry is dropped.
        s32 liTop = miNumEntries;
        if (liTop >= (KI_MAX_CONTACTS - 1))
            liTop = (KI_MAX_CONTACTS - 1);
        for (s32 liEntry = liTop; liEntry > liInsert; --liEntry)
        {
            maContactEntries[liEntry] = maContactEntries[liEntry - 1];
        }

        maContactEntries[liInsert].mfImpactTime = lfImpactTime;
        maContactEntries[liInsert].mpContact    = lpContact;
        if (miNumEntries < KI_MAX_CONTACTS)
            ++miNumEntries;
        return;
    }

    // Empty orderer: just place the first entry.
    if (miNumEntries < KI_MAX_CONTACTS)
    {
        maContactEntries[miNumEntries].mpContact    = lpContact;
        maContactEntries[miNumEntries].mfImpactTime = lfImpactTime;
        ++miNumEntries;
    }
}

// X360 0x825B37B8 (asm symbol truncated to 'Ge'; DWARF PotentialContactOrderer.h:56
// gives the true shape GetContact(int32_t)->const PotentialContact*). Returns the
// liIndex-th VALID contact (mbValid set), skipping invalidated entries. If no such
// valid entry exists it fires the tripwire and returns null.
const PotentialContact* PotentialContactOrderer::GetContact(s32 liIndex)
{
    s32 liValid = 0;
    for (s32 liEntry = 0; liEntry < miNumEntries; ++liEntry)
    {
        if (maContactEntries[liEntry].mbValid)
        {
            if (liValid == liIndex)
                return maContactEntries[liEntry].mpContact;
            ++liValid;
        }
    }

    CGS_ASSERT(false, "Couldn't find contact at specified index\n");
    return nullptr;
}

// X360 0x825B36E8: prune the ordered contact list. Walking from the earliest impact
// outward, an entry is kept only if it is not "behind" any earlier kept entry --
// tested by dot(this.mPointOnB - prior.mPointOnB, prior.mNormal): a negative dot means
// the later contact point sits on the back side of an earlier contact's normal plane,
// so it is discarded (mbValid cleared). Returns the number of entries left valid.
s32 PotentialContactOrderer::SortContacts()
{
    s32 liValidCount = miNumEntries;
    maContactEntries[0].mbValid = true;

    if (miNumEntries > 1)
    {
        for (s32 liEntry = 1; liEntry < miNumEntries; ++liEntry)
        {
            const PotentialContact* lpContact = maContactEntries[liEntry].mpContact;
            maContactEntries[liEntry].mbValid = true;

            for (s32 liPrior = 0; liPrior < liEntry; ++liPrior)
            {
                if (maContactEntries[liPrior].mbValid)
                {
                    const PotentialContact* lpPrior = maContactEntries[liPrior].mpContact;
                    const f32 lfSide = rw::math::vpu::Dot(
                        lpContact->mPointOnB - lpPrior->mPointOnB,
                        lpPrior->mNormal);
                    if (0.0f > lfSide)
                    {
                        --liValidCount;
                        maContactEntries[liEntry].mbValid = false;
                        break;
                    }
                }
            }
        }
    }

    return liValidCount;
}

} // namespace BrnPhysics
