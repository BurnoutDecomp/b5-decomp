#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT (IsEmpty run-list guards)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags

// BrnPhysics::ContactSpy::ContactSpyData -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   AddContact(const RaceCarContact&) @ 0x825A5230
//   IsEmpty() const                    @ 0x825A50E0

namespace BrnPhysics
{
namespace ContactSpy
{

// BrnPhysics::ContactSpy::ContactSpyData::AddContact(const RaceCarContact&) @ 0x825A5230.
// Appends the contact to mRaceCarContactQueue via the bounds-gated AddEventSafe; on a full
// queue (AddEventSafe returns false) it logs a filtered warning. Return type is void (DWARF
// BrnContactSpyData.h:160/165); the Hex-Rays int/tail-return of operator<< is a dead register
// leftover.
void ContactSpyData::AddContact(const RaceCarContact& lrContact)
{
    if (!mRaceCarContactQueue.AddEventSafe(lrContact))
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "WARNING: Ran out of contacts in RaceCarContactQueue.\n";
        }
    }
}

// BrnPhysics::ContactSpy::ContactSpyData::IsEmpty() const @ 0x825A50E0.
// Returns true iff all five resolved-contact queues are empty (race car / traffic / physical
// car part / hinged part / prop). The discarded-contact queue is deliberately NOT consulted
// (matches the X360 body -- only five miLength loads). When empty, the four contact run-lists
// must also be empty; each is guarded by its own assert (X360 de-inlined
// BeginAssert/FireAssert/EndAssert -> one CGS_ASSERT apiece).
bool ContactSpyData::IsEmpty() const
{
    const bool lbIsEmpty =
        mRaceCarContactQueue.GetLength() == 0 &&
        mTrafficContactQueue.GetLength() == 0 &&
        mPhysicalCarPartContactQueue.GetLength() == 0 &&
        mHingedPartContactQueue.GetLength() == 0 &&
        mPropContactQueue.GetLength() == 0;

    if (lbIsEmpty)
    {
        CGS_ASSERT(mRaceCarContactRunList.GetLength() == 0,
                   "mRaceCarContactRunList.GetLength() == 0");
        CGS_ASSERT(mTrafficContactRunList.GetLength() == 0,
                   "mTrafficContactRunList.GetLength() == 0");
        CGS_ASSERT(mPhysicalCarPartContactRunList.GetLength() == 0,
                   "mPhysicalCarPartContactRunList.GetLength() == 0");
        CGS_ASSERT(mPropContactRunList.GetLength() == 0,
                   "mPropContactRunList.GetLength() == 0");
    }

    return lbIsEmpty;
}

}
}
