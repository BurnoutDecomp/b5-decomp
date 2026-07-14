#include "GameSource/Physics/ContactSpies/BrnContactSpyQueue.h"      // ContactSpyQueue<T,N> (SortAndCreateRunList decl)
#include "GameSource/Physics/ContactSpies/BrnContactSpyRunList.h"    // ContactSpyRunData(+Construct), ContactSpyRunList<N>
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags

#include <algorithm>   // std::sort (models the X360 std::_Sort<PhysicalCarPartContact*,int> introsort call)

// BrnPhysics::ContactSpy::ContactSpyQueue<PhysicalCarPartContact, 150>::SortAndCreateRunList<50>
//   @ X360 0x825AF8F8   (home: GameSource/Physics/ContactSpies/BrnContactSpyQueue.cpp, DWARF-attested
//    assert paths d:\...\Physics/ContactSpies/BrnContactSpyQueue.cpp:110/180/240).
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. The generic member-template body is
// emitted out-of-line here (it needs the ContactSpyRunList/ContactSpyRunData + debug-log trees that
// the queue header deliberately does not pull in) and the <PhysicalCarPartContact,150>::<50>
// specialisation is explicitly instantiated at the bottom -- mirroring the committed
// ContactSpyQueue_RaceCarContact_300_GetBaseContact.cpp / EventQueue_* / BaseEventQueue_* sibling
// pattern in this directory. Called by BrnPhysics::PhysicsModule::ProcessContactSpies.
//
// Behaviour (X360 asm):
//   * assert lpOutRunList != NULL (:110); no-op when the queue is empty (GetLength() == 0);
//   * std::sort the live [maEvents, maEvents+GetLength()) contacts (non-predicated _Sort; the order
//     key is the colliding entity id -- proven by the consecutive-run grouping below, and the class's
//     own SortCompareContacts sibling predicate. Modelled here with that attested key);
//   * Clear() the output run list, then walk the sorted contacts collapsing each maximal run of equal
//     mEntityIdA into ONE ContactSpyRunData: mTotalFrictionStress = sum of the run's mFrictionStress,
//     mAverageStress = run-mean of mNormalStress, mAverageContactPoint = run-mean of mPointOnA;
//   * FAITHFUL QUIRK (X360-exact): on every run boundary the accumulators are reset to ZERO and the
//     boundary contact that STARTS the next run does NOT have its stresses added -- only its entity id
//     and start index seed the new run (asm 0x825AFC24-3C: vspltisw v0,0 then vmr v12{5,6,7},v0 with
//     no re-accumulate). So each run after the first omits its first element's stress contribution.
//     The very first run is seeded from GetBaseContact(0) before the loop, so it is complete;
//   * per run, if appending would fill the list (GetLength()+1 >= GetMaxLength()) it logs a
//     "queue overflow" warning through gpDebugPrint (gated by gxMessageFilterFlags & 1) but appends
//     anyway (AddEvent is unconditional); the in-loop emit additionally asserts liRunLength > 0 (:180)
//     and its warning names the tuning header, the trailing emit does neither;
//   * assert lpOutRunList->GetLength() == GetNumUniqueEntities() (:240).
//
// The 128-byte contact stride (asm `slwi ..,7`, `addi r26,r26,0x80`) matches sizeof(PhysicalCarPartContact)
// == 128 (BrnContactSpyEvents.h); the 80-byte run-data stride is committed in BrnContactSpyRunList.h.

namespace BrnPhysics
{
namespace ContactSpy
{
namespace
{
    // De-optimisation of the VMX vaddfp128 (full 16-byte / 4-lane add). The X360 accumulates the
    // whole register including the unused w lane, so all four lanes are summed here for byte-fidelity.
    inline void AccumulateVec4(Vector3& lrAccum, const Vector3& lrValue)
    {
        lrAccum.x += lrValue.x;
        lrAccum.y += lrValue.y;
        lrAccum.z += lrValue.z;
        lrAccum.w += lrValue.w;
    }

    // De-optimisation of the VMX vmulfp128 by the broadcast reciprocal (the asm computes 1/liRunLength
    // via vrefp + two Newton-Raphson refinement steps -> full float precision; expressed here as the
    // equivalent IEEE divide). Full 4-lane multiply, matching the register op.
    inline Vector3 ScaleVec4(const Vector3& lrValue, f32 lfScale)
    {
        Vector3 lResult;
        lResult.x = lrValue.x * lfScale;
        lResult.y = lrValue.y * lfScale;
        lResult.z = lrValue.z * lfScale;
        lResult.w = lrValue.w * lfScale;
        return lResult;
    }

    // The X360 sort key: contacts ordered by colliding entity id so equal-entity contacts land in
    // consecutive runs (the grouping loop then compares adjacent mEntityIdA). The comparison reads
    // the raw 32-bit id word (`cmplw`, unsigned). Mirrors the class's SortCompareContacts predicate.
    template <typename TContact>
    bool ContactLessByEntityId(const TContact& lrLhs, const TContact& lrRhs)
    {
        return lrLhs.mEntityIdA.muValue < lrRhs.mEntityIdA.muValue;
    }
}

template <typename T, s32 N>
template <s32 OutMaxLength>
void ContactSpyQueue<T, N>::SortAndCreateRunList(ContactSpyRunList<OutMaxLength>* lpOutRunList)
{
    CGS_ASSERT(lpOutRunList != nullptr, "lpOutRunList != NULL");   // BrnContactSpyQueue.cpp:110

    if (this->GetLength() == 0)
    {
        return;
    }

    // Sort the live contacts by colliding entity id (X360 std::_Sort over [begin, end)).
    T* lpBegin = this->maEvents;
    T* lpEnd   = this->maEvents + this->GetLength();
    std::sort(lpBegin, lpEnd, ContactLessByEntityId<T>);

    lpOutRunList->Clear();   // miLength = 0

    // Running per-run accumulators (VMX registers v127/v126/v125 in the asm).
    Vector3 lTotalFrictionStress;
    Vector3 lTotalNormalStress;
    Vector3 lTotalContactPoint;
    lTotalFrictionStress.SetZero();
    lTotalNormalStress.SetZero();
    lTotalContactPoint.SetZero();

    // Seed the first run from contact 0 (this run is complete -- its first element IS accumulated).
    const BaseContact* lpFirst = this->GetBaseContact(0);
    EntityId lCurrentEntityId  = lpFirst->mEntityIdA;
    AccumulateVec4(lTotalFrictionStress, lpFirst->mFrictionStress);
    AccumulateVec4(lTotalNormalStress,   lpFirst->mNormalStress);
    AccumulateVec4(lTotalContactPoint,   lpFirst->mPointOnA);

    s32 liRunStartIndex = 0;
    s32 liRunLength     = 1;

    // Appends one accumulated run to the output list. lpcWarnSuffix distinguishes the two X360 emit
    // sites (the in-loop warning names BrnContactSpyInterface.h, the trailing one does not).
    auto lEmitRun = [&](const char* lpcWarnSuffix)
    {
        if (lpOutRunList->GetLength() + 1 >= lpOutRunList->GetMaxLength())
        {
            const char* lpcTypeName = this->DebugGetEntityTypeName();
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                if (lpcTypeName == nullptr)
                {
                    lpcTypeName = "<NULLSTRING>";
                }
                *CgsDev::Log::gpDebugPrint << "WARNING: queue overflow in " << lpcTypeName << lpcWarnSuffix;
            }
        }

        const f32 lfInvRunLength = 1.0f / static_cast<f32>(liRunLength);
        Vector3 lAverageStress       = ScaleVec4(lTotalNormalStress, lfInvRunLength);
        Vector3 lAverageContactPoint = ScaleVec4(lTotalContactPoint, lfInvRunLength);

        ContactSpyRunData lRunData;
        ContactSpyRunData::Construct(&lRunData,
                                     lCurrentEntityId,
                                     lTotalFrictionStress,
                                     lAverageStress,
                                     lAverageContactPoint,
                                     liRunStartIndex,
                                     liRunLength);
        lpOutRunList->AddEvent(lRunData);
    };

    // Single-contact queues skip the loop and emit their one run directly (X360 `goto LABEL_29`),
    // bypassing the post-loop liRunLength > 0 guard.
    bool lbEmitFinalRun = true;

    if (this->GetLength() > 1)
    {
        s32 liIndex = 1;
        do
        {
            const BaseContact* lpContact = this->GetBaseContact(liIndex);
            if (lpContact->mEntityIdA.muValue == lCurrentEntityId.muValue)
            {
                ++liRunLength;
                AccumulateVec4(lTotalNormalStress,   lpContact->mNormalStress);
                AccumulateVec4(lTotalContactPoint,   lpContact->mPointOnA);
                AccumulateVec4(lTotalFrictionStress, lpContact->mFrictionStress);
            }
            else
            {
                // Close the current run.
                lEmitRun(" entity contact spy run list - increase the constants in BrnContactSpyInterface.h\n");
                CGS_ASSERT(liRunLength > 0, "liRunLength > 0");   // BrnContactSpyQueue.cpp:180

                // Start the next run at this boundary contact. FAITHFUL QUIRK: the accumulators are
                // reset to ZERO and this contact's stresses are NOT added (only its id + start index).
                lCurrentEntityId = lpContact->mEntityIdA;
                liRunStartIndex  = liIndex;
                liRunLength      = 1;
                lTotalFrictionStress.SetZero();
                lTotalNormalStress.SetZero();
                lTotalContactPoint.SetZero();
            }
            ++liIndex;
        }
        while (liIndex < this->GetLength());

        lbEmitFinalRun = (liRunLength > 0);
    }

    if (lbEmitFinalRun)
    {
        lEmitRun(" entity contact spy run list");
    }

    CGS_ASSERT(lpOutRunList->GetLength() == this->GetNumUniqueEntities(),
               "lpOutRunList->GetLength() == liNumUniqueEntities");   // BrnContactSpyQueue.cpp:240
}

// Explicit instantiation -- the physical-car-part deformable-contact pass:
// ContactSpyQueue<PhysicalCarPartContact,150>::SortAndCreateRunList<50>(ContactSpyRunList<50>*).
template void
ContactSpyQueue<PhysicalCarPartContact, 150>::SortAndCreateRunList<50>(ContactSpyRunList<50>*);

}
}
