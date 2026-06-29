#include "GameSource/Massive/BrnMassive.h"

#include <cstddef>   // offsetof
#include <cstring>   // memset
#include <new>       // placement new

#include "GameShared/GameClasses/Core/CgsAssert.h"

// ===========================================================================
// BrnMassive - bridge to the MassiveAd in-game ad client.
//
// Reconstructed from the X360 ARTIST.XEX (pseudocode + assembly). No Feb-2007
// source, no DecFIGS DWARF for this TU; every constant/offset/branch below is
// taken from the assembly in the dossier.
// ===========================================================================

namespace BrnMassive
{

// ---------------------------------------------------------------------------
// off_82F2413C: the per-impression-event MassiveAd zone-name table (9 entries,
// KI_NUM_IMPRESSION_EVENTS). CreateSubscriber/Prepare index it by IE. The X360
// .data table's first entry is the string "billboard_landscape01"; the remaining
// eight strings are NOT recoverable from this TU's code/asm (the table lives in
// .rodata and only its base symbol + entry[0] are attested).
//
// FLAGGED: entries [1..8] are placeholders, not fabricated zone names. They keep
// the table the right length so CreateSubscriber's `off_82F2413C[ie]` index and
// Prepare's table pointer compile and lay out correctly; the real strings must be
// recovered from the .rodata dump before this table is trusted at runtime.
// ---------------------------------------------------------------------------
const char* const gapcMassiveZoneNames[KI_NUM_IMPRESSION_EVENTS] =
{
    "billboard_landscape01", // [0] attested (off_82F2413C[0])
    "UNRECOVERED_ZONE_1",    // [1] FLAG: .rodata string not recovered
    "UNRECOVERED_ZONE_2",    // [2] FLAG
    "UNRECOVERED_ZONE_3",    // [3] FLAG
    "UNRECOVERED_ZONE_4",    // [4] FLAG
    "UNRECOVERED_ZONE_5",    // [5] FLAG
    "UNRECOVERED_ZONE_6",    // [6] FLAG
    "UNRECOVERED_ZONE_7",    // [7] FLAG
    "UNRECOVERED_ZONE_8",    // [8] FLAG
};

// ---------------------------------------------------------------------------
// Construct @ 0x823AB5C0
//
// Zeroes the inline subscriber pool (the leading 2280 bytes = 15 * 152) and sets
// the small trailing flags. mnNumActiveSubscribers (+0x924) is set to 0 before
// the memset; it lives past the 2280-byte cleared region so it survives.
// ---------------------------------------------------------------------------
int BrnMassive::Construct()
{
    mnNumActiveSubscribers = 0; // +0x924
    mbField92C = 0;             // +0x92C (asm stb r30=0)
    mbReady    = 1;             // +0x92D (asm stb r11=1; gates IsSafeToDownload)
    mbField92E = 0;             // +0x92E

    // memset clears exactly the inline subscriber pool (15 * sizeof(BrnMassiveSubscriber)).
    int liResult = reinterpret_cast<int>(
        std::memset(maSubscribers, 0, sizeof(maSubscribers)));

    mbIsPaused = 0;             // +0x92F
    return liResult;
}

// ---------------------------------------------------------------------------
// Destruct @ 0x823C6CE8
//
// Walks the pooled subscriber pointers from the current active slot up to slot 15
// and, for each non-null entry, runs the MassiveAd subscriber destructor and frees
// it. (The active entries below mnNumActiveSubscribers are left to the inline pool.)
// ---------------------------------------------------------------------------
int BrnMassive::Destruct()
{
    int liResult = reinterpret_cast<int>(this);

    if (mnNumActiveSubscribers < KI_MAX_SUBSCRIBERS)
    {
        for (s32 liSlot = mnNumActiveSubscribers; liSlot < KI_MAX_SUBSCRIBERS; ++liSlot)
        {
            BrnMassiveSubscriber* lpSubscriber = mapSubscribers[liSlot];
            if (lpSubscriber != nullptr)
            {
                lpSubscriber->~BrnMassiveSubscriber();
                MassiveAdClient3::CMassiveAdObjectSubscriber::operator delete(lpSubscriber);
                liResult = reinterpret_cast<int>(lpSubscriber);
            }
        }
    }

    return liResult;
}

// ---------------------------------------------------------------------------
// Prepare @ 0x823C12F0
//
// Caches the lookup-table pointer, constructs + registers the debug overlay, and
// clears the per-IE debug-state array (9 entries). Returns 1.
// ---------------------------------------------------------------------------
int BrnMassive::Prepare(int liLookupTable)
{
    mpLookupTable = reinterpret_cast<MassiveLookupTable*>(liLookupTable); // +0x928

    mDebugComponent.Construct(gapcMassiveZoneNames, KI_NUM_IMPRESSION_EVENTS,
                              maSubscriberDebugState, maSubscribers);
    mDebugComponent.Register();

    for (s32 liIE = 0; liIE < KI_NUM_IMPRESSION_EVENTS; ++liIE)
    {
        maSubscriberDebugState[liIE] = 0;
    }

    return 1;
}

// ---------------------------------------------------------------------------
// CreateSubscriber @ 0x823C1528
//
// Constructs a pooled subscriber (in place inside maSubscribers) for impression
// event liIE, stores its pointer in mapSubscribers, initialises its render/state
// fields (default 1280x720), and bumps the active count.
// ---------------------------------------------------------------------------
int BrnMassive::CreateSubscriber(int liIE, int liArg3, int liArg4)
{
    CGS_ASSERT(liIE < KI_NUM_IMPRESSION_EVENTS, "IE for Subscriber not recognised");
    CGS_ASSERT(mnNumActiveSubscribers < KI_MAX_SUBSCRIBERS,
               "Non Enough Slots for Subscribers for Massive, please increase this number");

    // The next free inline slot. (`this` is never null, but the X360 guards the
    // placement target against 0; preserved as a defensive check.)
    BrnMassiveSubscriber* lpSlot = &maSubscribers[mnNumActiveSubscribers];

    int liResult;
    if (lpSlot != nullptr)
    {
        liResult = reinterpret_cast<int>(
            new (lpSlot) BrnMassiveSubscriber(gapcMassiveZoneNames[liIE]));
    }
    else
    {
        liResult = 0;
    }

    mapSubscribers[mnNumActiveSubscribers] = reinterpret_cast<BrnMassiveSubscriber*>(liResult);

    BrnMassiveSubscriber* lpSubscriber = mapSubscribers[mnNumActiveSubscribers];
    lpSubscriber->muField78 = 0;       // +0x78
    lpSubscriber->muField7C = 0;       // +0x7C
    lpSubscriber->muField8C = 0;       // +0x8C
    lpSubscriber->muState = 0;         // +0x90
    lpSubscriber->mbField94 = 1;       // +0x94
    lpSubscriber->muIdleFrames = 0;    // +0x88

    // Zero the 8-DWORD block at +0x58.
    u8* lpBlock = lpSubscriber->maPad58;
    for (s32 li = 0; li < 8; ++li)
    {
        reinterpret_cast<u32*>(lpBlock)[li] = 0;
    }

    lpSubscriber->muWidth = 1280;      // +0x62
    lpSubscriber->muHeight = 720;      // +0x64

    mapSubscribers[mnNumActiveSubscribers]->muField8C = static_cast<u32>(liArg3); // +0x8C
    mapSubscribers[mnNumActiveSubscribers]->muIE = static_cast<u32>(liIE);        // +0x80
    mapSubscribers[mnNumActiveSubscribers]->muField84 = static_cast<u32>(liArg4); // +0x84

    ++mnNumActiveSubscribers;
    return liResult;
}

// ---------------------------------------------------------------------------
// GetSubscriberAtIndex @ 0x822A3E08
//
// Returns the pooled subscriber pointer at liIndex, with the two debug asserts
// the X360 fires (the second streams the index into the assert buffer; modelled
// as a plain message via CGS_ASSERT).
// ---------------------------------------------------------------------------
int BrnMassive::GetSubscriberAtIndex(int liIndex)
{
    CGS_ASSERT(mnNumActiveSubscribers <= KI_MAX_SUBSCRIBERS,
               "mnNumActiveSubsribers <= kMAXSUBSCRIBERS");
    CGS_ASSERT(liIndex < mnNumActiveSubscribers, "No Valid Subscriber at Index");

    return reinterpret_cast<int>(mapSubscribers[liIndex]);
}

// ---------------------------------------------------------------------------
// GetSubscriberWithIE @ 0x823AB618
//
// Linear-scan the active pool for the subscriber whose IE (+0x80) matches liIE.
// Returns its pooled pointer, or 0 if none.
// ---------------------------------------------------------------------------
int BrnMassive::GetSubscriberWithIE(int liIE)
{
    s32 liCount = mnNumActiveSubscribers;
    if (liCount <= 0)
    {
        return 0;
    }

    for (s32 liSlot = 0; liSlot < liCount; ++liSlot)
    {
        if (static_cast<int>(mapSubscribers[liSlot]->muIE) == liIE)
        {
            return reinterpret_cast<int>(mapSubscribers[liSlot]);
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// GetSubscriberDataAtIndex @ 0x822A3F08
//
// Find the lookup-table record whose IE byte (+0x30) matches lueIE, resolving and
// caching its subscriber-data pointer (+0x28) on first use via GetSubscriberWithIE.
// Returns the record pointer (as a raw u32), or 0 when not found / table empty.
// ---------------------------------------------------------------------------
u32 BrnMassive::GetSubscriberDataAtIndex(u8 lueIE)
{
    MassiveLookupTable* lpTable = mpLookupTable;
    u32 luNumRecords = lpTable->muNumRecords;
    MassiveLookupRecord* lpRecords = lpTable->mpRecords;

    if (luNumRecords == 0)
    {
        return 0;
    }

    u32 luIndex = 0;
    while (lpRecords[luIndex].meIE != lueIE)
    {
        if (++luIndex >= luNumRecords)
        {
            return 0;
        }
    }

    MassiveLookupRecord* lpRecord = &lpRecords[luIndex];
    if (lpRecord->muSubscriberData == 0)
    {
        lpRecord->muSubscriberData =
            static_cast<u32>(GetSubscriberWithIE(static_cast<int>(lpRecord->muField2C)));
    }

    return reinterpret_cast<u32>(lpRecord);
}

// ---------------------------------------------------------------------------
// IsSafeToDownload @ 0x822A3FA8
//
// True when the pool is ready (+0x92D) or paused (+0x92F).
// ---------------------------------------------------------------------------
int BrnMassive::IsSafeToDownload()
{
    if (mbReady)
    {
        return 1;
    }
    if (mbIsPaused)
    {
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// SetIsPaused @ 0x823AB668
//
// Mirror the pause flag into the MassiveAd client core (suspend vs resume all),
// then store it. When the flag is unchanged, or the client core is not live, just
// store the flag.
// ---------------------------------------------------------------------------
void BrnMassive::SetIsPaused(u8 lbPaused)
{
    if (mbIsPaused != lbPaused)
    {
        MassiveAdClient3::CMassiveClientCore* lpCore = MassiveAdClient3::Instance();
        if (lpCore != nullptr)
        {
            if (lbPaused != 0)
            {
                MassiveAdClient3::SuspendAll(MassiveAdClient3::Instance());
            }
            else
            {
                MassiveAdClient3::ResumeAll(MassiveAdClient3::Instance());
            }
        }
    }

    mbIsPaused = lbPaused;
}

// ---------------------------------------------------------------------------
// Update @ 0x823C1370
//
// Per-frame:
//  (1) When the MassiveAd client is live: age every active subscriber whose state
//      is 0 or 1 (bump muIdleFrames; once it passes 300 frames force state 2),
//      then tick the client.
//  (2) When the debug overlay is enabled: recompute one debug-state code per IE
//      slot (9 of them) describing each subscriber's download/render disposition,
//      and resolve the currently-selected debug view.
// ---------------------------------------------------------------------------
void BrnMassive::Update()
{
    if (MassiveAdClient3::Instance() != nullptr)
    {
        for (s32 liSlot = 0; liSlot < mnNumActiveSubscribers; ++liSlot)
        {
            BrnMassiveSubscriber* lpSubscriber = mapSubscribers[liSlot];
            if (lpSubscriber->muState <= 1u)
            {
                u32 luFrames = lpSubscriber->muIdleFrames + 1;
                lpSubscriber->muIdleFrames = luFrames;
                if (luFrames > 300u)
                {
                    lpSubscriber->muState = 2;
                }
            }
        }

        MassiveAdClient3::Instance();
        MassiveAdClient3::Tick(0);
    }

    if (mDebugComponent.mbEnabled)
    {
        bool lbDownloadSlotTaken = false;

        for (s32 liIE = 0; liIE < KI_NUM_IMPRESSION_EVENTS; ++liIE)
        {
            if (liIE < mnNumActiveSubscribers)
            {
                BrnMassiveSubscriber* lpSubscriber = mapSubscribers[liIE];
                u32 luState = lpSubscriber->muState;
                if (luState == 2)
                {
                    maSubscriberDebugState[liIE] = 1;
                }
                else
                {
                    bool lbReadyOrLoaded = (luState == 3 || luState == 2);
                    if (lbReadyOrLoaded)
                    {
                        if (lpSubscriber->mbHasContent)
                        {
                            maSubscriberDebugState[liIE] = 5;
                            if (!mbIsPaused)
                            {
                                ++mDebugComponent.muUnpausedAds;
                            }
                        }
                        else
                        {
                            maSubscriberDebugState[liIE] = 4;
                        }
                    }
                    else
                    {
                        maSubscriberDebugState[liIE] = 3;
                    }
                }
            }
            else if (lbDownloadSlotTaken || IsSafeToDownload())
            {
                maSubscriberDebugState[liIE] = 0;
            }
            else
            {
                lbDownloadSlotTaken = true;
                maSubscriberDebugState[liIE] = 2;
            }
        }

        u32 luViewIndex = mDebugComponent.muViewIndex;
        if (luViewIndex > 14u)
        {
            mDebugComponent.muViewSubscriber = 0;
            mDebugComponent.mbViewPaused = mbIsPaused;
        }
        else
        {
            mDebugComponent.muViewSubscriber =
                reinterpret_cast<u32>(mapSubscribers[luViewIndex]);
            mDebugComponent.mbViewPaused = mbIsPaused;
        }
    }
}

// ---------------------------------------------------------------------------
// Never-called layout pin: assert the X360-relative member offsets so the
// reconstructed class can never silently drift. (offsetof in a dead function.)
// ---------------------------------------------------------------------------
void BrnMassive_AssertLayout()
{
    // BrnMassiveSubscriber's 152-byte footprint is the load-bearing layout fact: it
    // sizes the inline pool (maSubscribers[15] == 0x8E8) so the pointer array, count
    // and flags land at their X360 offsets (0x8E8 / 0x924 / 0x92C..0x92F).
    static_assert(sizeof(BrnMassiveSubscriber) == 0x98, "BrnMassiveSubscriber is 152 bytes");

    // FLAG: offsetof asserts are intentionally omitted. BrnMassiveSubscriber derives
    // from the polymorphic MassiveAd SDK base (and BrnMassiveDebugComponent from the
    // shared CgsDev::DebugComponent), so BrnMassive and BrnMassiveSubscriber are not
    // standard-layout and MSVC's offsetof is unreliable on them. The X360 absolute
    // offsets are documented per-member in BrnMassive.h instead; the shared bases are
    // global state and must not be forked/padded to coerce a host-exact match.
}

} // namespace BrnMassive
