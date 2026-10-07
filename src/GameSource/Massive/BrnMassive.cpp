#include "GameSource/Massive/BrnMassive.h"

#include <cstring>   // memset
#include <new>       // placement new

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"   // CMassiveClientCore::Instance / Tick / SuspendAll / ResumeAll

// ===========================================================================
// BrnMassive - bridge to the MassiveAd in-game ad client. Reconstructed from the
// console code (no debug-info declaration exists for this TU).
// ===========================================================================

namespace BrnMassive
{

// The per-impression-event MassiveAd zone names (CreateSubscriber / Prepare index it by IE).
const char* const gapcMassiveZoneNames[KI_NUM_IMPRESSION_EVENTS] =
{
    "billboard_landscape01",
    "billboard_landscape02",
    "billboard_landscape03",
    "billboard_landscape04",
    "billboard_landscape06",
    "billboard_landscape07",
    "billboard_landscape08",
    "billboard_landscape09",
    "billboard_landscape10",
};

// A subscriber still waiting for its ad after this many updates is marked timed out.
static const u32 KU_MAX_IDLE_FRAMES = 300;

// Debug-overlay state codes (BrnMassiveDebugComponent::GetState).
enum EDebugState
{
    E_DEBUGSTATE_NOT_CREATED       = 0,
    E_DEBUGSTATE_SKIPPED           = 1,
    E_DEBUGSTATE_BLOCKED           = 2,
    E_DEBUGSTATE_PENDING           = 3,
    E_DEBUGSTATE_COMPLETE          = 4,
    E_DEBUGSTATE_IN_VIEW           = 5
};

// ---------------------------------------------------------------------------
// Construct: zero the subscriber pool and set the flags (safe to download until the
// first frame's impression pass says otherwise).
// ---------------------------------------------------------------------------
int BrnMassive::Construct()
{
    mnNumActiveSubscribers   = 0;
    mbField92C               = false;
    mbSafeToDownload         = true;
    mbImpressionFrameStarted = false;
    std::memset(maSubscriberPool, 0, sizeof(maSubscriberPool));
    mbIsPaused               = false;
    return 0;
}

// ---------------------------------------------------------------------------
// Destruct: release every subscriber pointer from the active count up to the pool
// size (destroy + free).
// ---------------------------------------------------------------------------
void BrnMassive::Destruct()
{
    for (s32 liSlot = mnNumActiveSubscribers; liSlot < KI_MAX_SUBSCRIBERS; ++liSlot)
    {
        BrnMassiveSubscriber* lpSubscriber = mapSubscribers[liSlot];
        if (lpSubscriber != nullptr)
        {
            lpSubscriber->MassiveAdClient3::CMassiveAdObjectSubscriber::~CMassiveAdObjectSubscriber();
            ::operator delete(lpSubscriber);
        }
    }
}

// ---------------------------------------------------------------------------
// Prepare: cache the lookup table, construct + register the debug overlay, and clear
// the per-IE debug states.
// ---------------------------------------------------------------------------
bool BrnMassive::Prepare(MassiveLookupTable* lpLookupTable)
{
    mpLookupTable = lpLookupTable;

    mDebugComponent.Construct(gapcMassiveZoneNames, KI_NUM_IMPRESSION_EVENTS,
                              maiSubscriberDebugState, mapSubscribers);
    mDebugComponent.Register();

    for (s32 liIE = 0; liIE < KI_NUM_IMPRESSION_EVENTS; ++liIE)
    {
        maiSubscriberDebugState[liIE] = E_DEBUGSTATE_NOT_CREATED;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Update: with a live client, age the subscribers still waiting for an ad and tick the
// client; then, when the debug overlay is on, recompute one state code per impression
// event and resolve the selected debug subscriber.
// ---------------------------------------------------------------------------
void BrnMassive::Update()
{
    if (MassiveAdClient3::CMassiveClientCore::Instance() != nullptr)
    {
        for (s32 liSlot = 0; liSlot < mnNumActiveSubscribers; ++liSlot)
        {
            BrnMassiveSubscriber* lpSubscriber = mapSubscribers[liSlot];
            if (lpSubscriber->miState == 1 || lpSubscriber->miState == 0)
            {
                ++lpSubscriber->muIdleFrames;
                if (lpSubscriber->muIdleFrames > KU_MAX_IDLE_FRAMES)
                {
                    lpSubscriber->miState = 2;
                }
            }
        }

        MassiveAdClient3::CMassiveClientCore::Instance()->Tick(0);
    }

    if (mDebugComponent.mbDisplay)
    {
        bool lbDownloadBlocked = false;

        for (s32 liIE = 0; liIE < KI_NUM_IMPRESSION_EVENTS; ++liIE)
        {
            if (liIE < mnNumActiveSubscribers)
            {
                const BrnMassiveSubscriber* lpSubscriber = mapSubscribers[liIE];
                if (lpSubscriber->miState == 2)
                {
                    maiSubscriberDebugState[liIE] = E_DEBUGSTATE_SKIPPED;
                }
                else if (lpSubscriber->miState == 3 || lpSubscriber->miState == 2)
                {
                    if (lpSubscriber->mImpression.mbInView)
                    {
                        maiSubscriberDebugState[liIE] = E_DEBUGSTATE_IN_VIEW;
                        if (!mbIsPaused)
                        {
                            ++mDebugComponent.miNumImpressions;
                        }
                    }
                    else
                    {
                        maiSubscriberDebugState[liIE] = E_DEBUGSTATE_COMPLETE;
                    }
                }
                else
                {
                    maiSubscriberDebugState[liIE] = E_DEBUGSTATE_PENDING;
                }
            }
            else if (lbDownloadBlocked || IsSafeToDownload())
            {
                maiSubscriberDebugState[liIE] = E_DEBUGSTATE_NOT_CREATED;
            }
            else
            {
                lbDownloadBlocked = true;
                maiSubscriberDebugState[liIE] = E_DEBUGSTATE_BLOCKED;
            }
        }

        const u32 luDebugSubscriber = static_cast<u32>(mDebugComponent.miDebugSubscriber);
        if (luDebugSubscriber > static_cast<u32>(KI_MAX_SUBSCRIBERS - 1))
        {
            mDebugComponent.mpDebugSubscriber = nullptr;
        }
        else
        {
            mDebugComponent.mpDebugSubscriber = mapSubscribers[luDebugSubscriber];
        }
        mDebugComponent.mbPaused = mbIsPaused;
    }
}

// ---------------------------------------------------------------------------
// CreateSubscriber: placement-construct the next pool slot for impression event liIE,
// publish it, and initialise its impression block (1280x720 reference screen) and its
// replacement-texture description.
// ---------------------------------------------------------------------------
BrnMassiveSubscriber* BrnMassive::CreateSubscriber(s32 liIE, void* lpTextureData, u32 luExpectedSize)
{
    CGS_ASSERT(liIE < KI_NUM_IMPRESSION_EVENTS, "IE for Subscriber not recognised");
    CGS_ASSERT(mnNumActiveSubscribers < KI_MAX_SUBSCRIBERS,
               "Non Enough Slots for Subscribers for Massive, please increase this number");

    BrnMassiveSubscriber* lpSlot = GetPoolSlot(mnNumActiveSubscribers);
    BrnMassiveSubscriber* lpSubscriber = nullptr;
    if (lpSlot != nullptr)
    {
        lpSubscriber = new (lpSlot) BrnMassiveSubscriber(gapcMassiveZoneNames[liIE]);
    }
    mapSubscribers[mnNumActiveSubscribers] = lpSubscriber;

    BrnMassiveSubscriber* lpNew = mapSubscribers[mnNumActiveSubscribers];
    lpNew->muCrexID             = 0;
    lpNew->miInvElementID       = 0;
    lpNew->mpTextureData        = nullptr;
    lpNew->miState              = 0;
    lpNew->mbImpressionReported = true;
    lpNew->muIdleFrames         = 0;
    std::memset(&lpNew->mImpression, 0, sizeof(lpNew->mImpression));
    lpNew->mImpression.muScreenWidth  = 1280;
    lpNew->mImpression.muScreenHeight = 720;

    mapSubscribers[mnNumActiveSubscribers]->mpTextureData  = lpTextureData;
    mapSubscribers[mnNumActiveSubscribers]->miIE           = liIE;
    mapSubscribers[mnNumActiveSubscribers]->muExpectedSize = luExpectedSize;

    ++mnNumActiveSubscribers;
    return lpSubscriber;
}

// ---------------------------------------------------------------------------
// GetSubscriberAtIndex: the active subscriber at liIndex.
// ---------------------------------------------------------------------------
BrnMassiveSubscriber* BrnMassive::GetSubscriberAtIndex(s32 liIndex)
{
    CGS_ASSERT(mnNumActiveSubscribers <= KI_MAX_SUBSCRIBERS,
               "mnNumActiveSubsribers <= kMAXSUBSCRIBERS");
    CGS_ASSERT(liIndex < mnNumActiveSubscribers, "No Valid Subscriber at Index");

    return mapSubscribers[liIndex];
}

// ---------------------------------------------------------------------------
// GetSubscriberWithIE: the active subscriber reporting impression event liIE, or null.
// ---------------------------------------------------------------------------
BrnMassiveSubscriber* BrnMassive::GetSubscriberWithIE(s32 liIE)
{
    for (s32 liSlot = 0; liSlot < mnNumActiveSubscribers; ++liSlot)
    {
        if (mapSubscribers[liSlot]->miIE == liIE)
        {
            return mapSubscribers[liSlot];
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// GetSubscriberDataAtIndex: the lookup-table item tagged with luRenderableIndex, resolving
// and caching its subscriber on first use; null when the table has no such item.
// ---------------------------------------------------------------------------
MassiveLookupTableItem* BrnMassive::GetSubscriberDataAtIndex(u8 luRenderableIndex)
{
    const u32 luNumItems = mpLookupTable->GetNumItems();
    MassiveLookupTableItem* lpItems = mpLookupTable->GetItems();

    for (u32 luItem = 0; luItem < luNumItems; ++luItem)
    {
        MassiveLookupTableItem* lpItem = &lpItems[luItem];
        if (lpItem->muRenderableIndex == luRenderableIndex)
        {
            if (lpItem->mpSubscriber == nullptr)
            {
                lpItem->mpSubscriber = GetSubscriberWithIE(lpItem->miIEIndex);
            }
            return lpItem;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// IsSafeToDownload: no advert is in view without its subscriber, or Massive is paused.
// ---------------------------------------------------------------------------
bool BrnMassive::IsSafeToDownload()
{
    return mbSafeToDownload || mbIsPaused;
}

// ---------------------------------------------------------------------------
// SetIsPaused: on a change, suspend/resume the live MassiveAd client; store the flag.
// ---------------------------------------------------------------------------
void BrnMassive::SetIsPaused(bool lbPaused)
{
    if (mbIsPaused != lbPaused && MassiveAdClient3::CMassiveClientCore::Instance() != nullptr)
    {
        if (lbPaused)
        {
            MassiveAdClient3::CMassiveClientCore::Instance()->SuspendAll();
        }
        else
        {
            MassiveAdClient3::CMassiveClientCore::Instance()->ResumeAll();
        }
    }

    mbIsPaused = lbPaused;
}

} // namespace BrnMassive
