#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the not-already-contained tripwire)
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostManager.h" // BoostManager::OnNearMiss

// -----------------------------------------------------------------------------------------------
// Chain-event payloads pushed onto the GameStateModuleIO game-event queue (mirrors the sibling
// AirTimeManager's local InAirEvent idiom). Each derives from CgsModule::Event so the empty-base
// optimisation puts the first payload field at offset 0 -- AddEvent memcpy's exactly the byte
// image the X360 built on the stack. DWARF hint names the queue events NearMissEvent /
// NearMissChainInProgressEvent / NearMissChainCompleteEvent; the first is renamed here to avoid
// clashing with the NearMissManager::NearMissEvent method.
// -----------------------------------------------------------------------------------------------
namespace
{
    // NearMissEvent payload (type 64 / '@', 8 bytes): the running chain count and the category.
    struct NearMissChainEvent : public CgsModule::Event
    {
        s32                   miCount;          // +0x0
        BrnWorld::ENearMissType meNearMissType; // +0x4
    };

    // NearMissChainInProgressEvent (type 65 / 'A', 4 bytes): the chain is still alive this frame.
    struct NearMissChainInProgressEvent : public CgsModule::Event
    {
        s32 miCount;   // +0x0
    };

    // NearMissChainCompleteEvent (type 66 / 'B', 8 bytes): the chain timed out. Carries the final
    // count and whether it completed without failing (mbFailedNearMissChain == false).
    struct NearMissChainCompleteEvent : public CgsModule::Event
    {
        s32  miCount;           // +0x0
        bool mbChainSucceeded;  // +0x4
    };
}

// X360 rodata flt_820149B4 (Feb-2007: NearMissManager::KF_NEAR_MISS_CHAIN_TIME = 5.0f).
const f32 BrnWorld::NearMissManager::KF_NEAR_MISS_CHAIN_TIME = 5.0f;

// BrnWorld::NearMissManager::HasThereBeenARecentNearMiss @ 0x822CD2E8. Reconstructed from
// BURNOUT_X360_ARTIST.XEX. True iff either the traffic-vehicle near-miss list or the race-car
// near-miss list currently holds a remembered vehicle.
//
// The X360 body inlines the traffic side's query (reads mTrafficNearMissData.maNearMiss' count
// word @ this+0x54, firing the CgsArray Construct/Clear assert, then tests count>0) and issues a
// real call to the race-car side (mRaceCarNearMissData @ this+0x148). Both collapse to the same
// public NearMissData<A,B>::HasThereBeenARecentNearMiss() query at source level; the compiler's
// choice to inline one and call the other is a codegen detail with no observable difference.
bool BrnWorld::NearMissManager::HasThereBeenARecentNearMiss() const
{
    if (mTrafficNearMissData.HasThereBeenARecentNearMiss())
    {
        return true;
    }
    return mRaceCarNearMissData.HasThereBeenARecentNearMiss();
}

// BrnWorld::NearMissManager::AddNearRaceCar @ 0x822ED198. Log a race-car section/entity id into
// the race-car near-section working list. The X360 ARTIST build guards with a manager-level debug
// assert (the rodata names the manager member mRaceCarNearMissData), then defers the length<cap
// append to NearMissData<4,7>::AddNear (which the compiler inlines here).
void BrnWorld::NearMissManager::AddNearRaceCar(u32 luRaceCarId)
{
    CGS_ASSERT(!mRaceCarNearMissData.IsContainedInNearArray(luRaceCarId),
               "!mRaceCarNearMissData.IsContainedInNearArray( luRaceCarId )");
    mRaceCarNearMissData.AddNear(luRaceCarId);
}

// BrnWorld::NearMissManager::AddNearTraffic @ 0x822ED250. Sibling of AddNearRaceCar for the
// traffic-vehicle near list (NearMissData<4,8> @ +0x0C); the AddNear body is likewise inlined.
void BrnWorld::NearMissManager::AddNearTraffic(u32 luEntityId)
{
    CGS_ASSERT(!mTrafficNearMissData.IsContainedInNearArray(luEntityId),
               "!mTrafficNearMissData.IsContainedInNearArray( luEntityId )");
    mTrafficNearMissData.AddNear(luEntityId);
}

// BrnWorld::NearMissManager::NearMissEvent @ 0x822F88B8. Re-prime the chain timeout to
// KF_NEAR_MISS_CHAIN_TIME (5.0), bump the running chain count, notify the boost manager, then push
// one NearMiss chain event { miCount, category } (type 64 / '@', 8 bytes). The X360 loads the count
// after the boost notification -- OnNearMiss touches a different object, so the value is unchanged.
// luEntityId is passed for the caller's symmetry but is never read by the X360 body.
void BrnWorld::NearMissManager::NearMissEvent(ENearMissType leNearMissType, u32 luEntityId,
                                              GameEventQueue* lpEventQueue, BoostManager* lpBoostManager)
{
    (void)luEntityId;

    mfNearMissTimeout = KF_NEAR_MISS_CHAIN_TIME;   // +0x04
    ++miNearMissCount;                             // +0x08

    // Notify the boost manager (the X360 inlines this into a dispatch through the owning entity
    // held at BoostManager+0x450, entity vtable slot +0x18).
    lpBoostManager->OnNearMiss(leNearMissType);

    NearMissChainEvent loEvent;
    loEvent.miCount        = miNearMissCount;
    loEvent.meNearMissType = leNearMissType;
    lpEventQueue->AddEvent(&loEvent, 64, 8);
}

// BrnWorld::NearMissManager::Update @ 0x822F8928. One chain-tracking frame. First age the chain
// timeout: while it is still positive, publish an In-Progress event (type 65 / 'A') and decay it;
// once it hits zero, publish a Complete event (type 66 / 'B', carrying whether the chain never
// failed) and reset the count + failed flag. Then, unless the player is crashing, walk each
// category's previous-frame near list and fire a NearMissEvent for every remembered vehicle that
// is not already in the current near list / recently near-missed / checked / contacted (and, for
// race cars, not recently taken down). The category is the crash-escape variant when the vehicle
// recently crashed, else the normal variant. Finally age + snapshot both sub-objects (race car
// first, then traffic), matching the X360 store order.
void BrnWorld::NearMissManager::Update(f32 lfTimeStep, GameEventQueue* lpEventQueue,
                                       BoostManager* lpBoostManager)
{
    if (mfNearMissTimeout > 0.0f)
    {
        if (miNearMissCount > 0)
        {
            NearMissChainInProgressEvent loEvent;
            loEvent.miCount = miNearMissCount;
            lpEventQueue->AddEvent(&loEvent, 65, 4);
        }
        mfNearMissTimeout -= lfTimeStep;
    }
    else
    {
        if (miNearMissCount > 0)
        {
            NearMissChainCompleteEvent loEvent;
            loEvent.miCount          = miNearMissCount;
            loEvent.mbChainSucceeded = !mbFailedNearMissChain;
            CGS_ASSERT(lpEventQueue != 0, "lpEventQueue");
            lpEventQueue->AddEvent(&loEvent, 66, 8);
        }
        miNearMissCount       = 0;
        mbFailedNearMissChain = false;
    }

    // Traffic near list: replay each previous-frame near vehicle.
    for (u32 luIndex = 0; luIndex < mTrafficNearMissData.GetNumPreviousFrameNearElements(); ++luIndex)
    {
        const u32 luEntityId = mTrafficNearMissData.GetPreviousFrameNearElement(luIndex);
        if (!mbCrashing)
        {
            if (!mTrafficNearMissData.IsContainedInNearArray(luEntityId)
                && !mTrafficNearMissData.IsRecentNearMiss(luEntityId)
                && !mTrafficNearMissData.IsRecentTrafficCheck(luEntityId)
                && !mTrafficNearMissData.IsRecentContacted(luEntityId))
            {
                const ENearMissType leType = mTrafficNearMissData.HasRecentlyCrashed(luEntityId)
                    ? E_NEAR_MISS_CRASH_ESCAPE_TRAFFIC
                    : E_NEAR_MISS_NORMAL_TRAFFIC;
                NearMissEvent(leType, luEntityId, lpEventQueue, lpBoostManager);
                mTrafficNearMissData.RememberNearMiss(luEntityId);
            }
        }
    }

    // Race-car near list.
    for (u32 luIndex = 0; luIndex < mRaceCarNearMissData.GetNumPreviousFrameNearElements(); ++luIndex)
    {
        const u32 luEntityId = mRaceCarNearMissData.GetPreviousFrameNearElement(luIndex);
        if (!mbCrashing)
        {
            if (!mRaceCarNearMissData.IsContainedInNearArray(luEntityId)
                && !mRaceCarNearMissData.IsRecentNearMiss(luEntityId)
                && !mRaceCarNearMissData.IsRecentContacted(luEntityId)
                && !mRaceCarNearMissData.IsRecentlyTakeDown(luEntityId))
            {
                const ENearMissType leType = mRaceCarNearMissData.HasRecentlyCrashed(luEntityId)
                    ? E_NEAR_MISS_CRASH_ESCAPE_OTHER_RACE_CAR
                    : E_NEAR_MISS_NORMAL_OTHER_RACE_CAR;
                NearMissEvent(leType, luEntityId, lpEventQueue, lpBoostManager);
                mRaceCarNearMissData.RememberNearMiss(luEntityId);
            }
        }
    }

    mRaceCarNearMissData.UpdateTimers(lfTimeStep);
    mRaceCarNearMissData.BackupNearArray();
    mTrafficNearMissData.UpdateTimers(lfTimeStep);
    mTrafficNearMissData.BackupNearArray();
}
