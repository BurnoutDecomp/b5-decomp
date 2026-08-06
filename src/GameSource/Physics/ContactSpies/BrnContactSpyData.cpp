#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT (IsEmpty run-list guards)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameSource/World/BrnEntityTypes.h"                        // BrnWorld::EEntityTypeID enumerators (Construct's per-queue tags)

// BrnPhysics::ContactSpy::ContactSpyData -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   Construct()                        @ 0x825AE010
//   AddContact(const RaceCarContact&)  @ 0x825A5230
//   IsEmpty() const                    @ 0x825A50E0

namespace BrnPhysics
{
namespace ContactSpy
{

// BrnPhysics::ContactSpy::ContactSpyData::Construct() @ 0x825AE010 (56 asm lines).
//
// Constructs the ten owned queues/run-lists in declaration order, tagging each with the entity
// type whose contacts it holds. Nothing else -- the X360 body is exactly ten calls plus nine
// tag stores (the discarded-contact queue is a plain CgsModule::EventQueue and has no tag).
//
// EVERY LINE BELOW IS PINNED BY THE ASM, not inferred. The X360 inlines
// ContactSpyQueue/ContactSpyRunList::Construct, so each pair shows up as
// `bl CgsModule::EventQueue<T,N>::Construct` on the sub-object's address, followed by a
// `stw <const>` at that sub-object's meEntityType. Sub-object base addresses come from the
// `addis/addi` pairs; tag offsets from the store displacements:
//
//   sub-object            X360 base   `stw` displacement -> absolute   tag
//   mRaceCarContactQueue          +0        0x7090      -> +0x07090      1  RACECAR
//   mTrafficContactQueue     +0x070A0        0x9610      -> +0x106B0      2  TRAFFIC_VEHICLE
//   mPhysicalCarPartContactQueue +0x106C0    0x4B10      -> +0x151D0      6  RACECAR_DEFORMABLE_PART
//   mHingedPartContactQueue  +0x151E0        0x15F0      -> +0x167D0      6  RACECAR_DEFORMABLE_PART
//   mPropContactQueue        +0x167E0        0x2BD0      -> +0x193B0      3  PROP
//   mDiscardedContactQueue   +0x193C0        (none)                       -  (plain EventQueue)
//   mRaceCarContactRunList   +0x198D0        0x290       -> +0x19B60      1  RACECAR
//   mTrafficContactRunList   +0x19B70        0x1410      -> +0x1AF80      2  TRAFFIC_VEHICLE
//   mPhysicalCarPartContactRunList +0x1AF90  0xFB0       -> +0x1BF40      6  RACECAR_DEFORMABLE_PART
//   mPropContactRunList      +0x1BF50        0x1F50      -> +0x1DEA0      3  PROP
//
// Each displacement equals 0x10 + N*sizeof(T) for that instantiation -- i.e. the meEntityType
// member that sits after the inherited inline maEvents[N] buffer -- so the mapping from store
// to member is proven, not assumed. (Queues: RaceCarContact 96B, TrafficContact 96B,
// PhysicalCarPartContact 128B; run-lists: ContactSpyRunData 80B.) The five queue base offsets
// and the sizeof(ContactSpyData) == 0x1DEB0 tail also match the committed header's layout table
// exactly, which is an independent confirmation of both.
//
// The tag VALUES are the DWARF enumerators, cross-checked against
// ContactSpyQueue::DebugGetEntityTypeName's own switch (1 -> "race car", 2 -> "traffic vehicle",
// 3 -> "prop", 6 -> "race car deformable part").
//
// NOTE on the hinged-part queue: it is tagged RACECAR_DEFORMABLE_PART (6), the SAME tag as the
// physical-car-part queue, not TRAFFIC_DEFORMABLE_PART (7). The asm reuses r28 (== 6) for both
// stores, so this is the console's behaviour and not a transcription slip.
void ContactSpyData::Construct()
{
    mRaceCarContactQueue.Construct(BrnWorld::E_ENTITYTYPE_RACECAR);
    mTrafficContactQueue.Construct(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE);
    mPhysicalCarPartContactQueue.Construct(BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART);
    mHingedPartContactQueue.Construct(BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART);
    mPropContactQueue.Construct(BrnWorld::E_ENTITYTYPE_PROP);

    mDiscardedContactQueue.Construct();

    mRaceCarContactRunList.Construct(BrnWorld::E_ENTITYTYPE_RACECAR);
    mTrafficContactRunList.Construct(BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE);
    mPhysicalCarPartContactRunList.Construct(BrnWorld::E_ENTITYTYPE_RACECAR_DEFORMABLE_PART);
    mPropContactRunList.Construct(BrnWorld::E_ENTITYTYPE_PROP);
}

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

// ============================================================================================
// The remaining typed AddContact overloads -- ADDED 2026-08-06 (bridge de-facade wave), each
// read from its own X360 emission (all three are bl targets of PhysicsModule::StoreContact
// @0x825A5DB0 and share the RaceCarContact overload's exact shape: bounds-gated AddEventSafe
// into the matching queue; on full, a filtered one-line warning naming the queue):
//   * AddContact(const TrafficContact&)          @ 0x825A5288 (queue @+0x070A0)
//   * AddContact(const PhysicalCarPartContact&)  @ 0x825A52E0 (queue @+0x106C0)
//   * AddContact(const PropContact&)             @ 0x825A5340 (queue @+0x167E0)
// ============================================================================================

void ContactSpyData::AddContact(const TrafficContact& lrContact)
{
    if (!mTrafficContactQueue.AddEventSafe(lrContact))
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "WARNING: Ran out of contacts in TrafficContactQueue.\n";
        }
    }
}

void ContactSpyData::AddContact(const PhysicalCarPartContact& lrContact)
{
    if (!mPhysicalCarPartContactQueue.AddEventSafe(lrContact))
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "WARNING: Ran out of contacts in PhysicalCarPartContactQueue.\n";
        }
    }
}

void ContactSpyData::AddContact(const PropContact& lrContact)
{
    if (!mPropContactQueue.AddEventSafe(lrContact))
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "WARNING: Ran out of contacts in PropContactQueue.\n";
        }
    }
}

// AddContact(const DiscardedContact&): the console emits NO out-of-line body -- the overload
// is header-inline and shows up fully inlined in its one caller, PhysicsModule::
// BridgeSimulationToOutput @0x825B0594..0x825B05E0 (AddEventSafe @0x825A3628 on
// mDiscardedContactQueue @+0x193C0, then the filtered warning below on a full queue). The
// BaseEventQueue_DiscardedContact_AddEventSafe.cpp banner has carried this exact attribution
// since it landed. Bodied here beside its three out-of-line siblings.
void ContactSpyData::AddContact(const DiscardedContact& lrContact)
{
    if (!mDiscardedContactQueue.AddEventSafe(lrContact))
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "WARNING: Ran out of contacts in DiscardedContactQueue.\n";
        }
    }
}

}
}
