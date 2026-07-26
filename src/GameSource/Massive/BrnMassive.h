#ifndef GAMESOURCE_MASSIVE_BRNMASSIVE_H
#define GAMESOURCE_MASSIVE_BRNMASSIVE_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

// ---------------------------------------------------------------------------
// BrnMassive - Burnout's in-engine bridge to the MassiveAd in-game advertising
// client (MassiveAdClient3). It owns a fixed pool of ad "subscribers" (one per
// in-world ad surface / impression event), drives the MassiveAd client tick,
// and feeds a debug overlay describing each subscriber's download/render state.
//
// Original home: ..\..\..\GameSource\Massive/BrnMassive.{h,cpp} (X360 ARTIST).
// Reconstructed from the X360 ARTIST.XEX pseudocode + assembly. There is no
// Feb-2007 source and no DecFIGS DWARF for this TU, so the class layout below is
// recovered entirely from the field offsets stored/loaded in the assembly:
//
//   +0x000  maSubscribers[15]          inline BrnMassiveSubscriber pool (15 * 152)
//   +0x8E8  mapSubscribers[15]         pointers into the pool (stored by CreateSubscriber)
//   +0x924  mnNumActiveSubscribers     active-slot count
//   +0x928  mpLookupTable              -> MassiveLookupTable runtime record block
//   +0x92C  mbField92C  (= 0 at Construct; semantics unknown -> named flag)
//   +0x92D  mbReady     (= 1 at Construct) IsSafeToDownload short-circuits true on this
//   +0x92E  mbField92E
//   +0x92F  mbIsPaused                 SetIsPaused mirrors this into the client core
//   +0x930  mDebugComponent            BrnMassiveDebugComponent (60 bytes)
//   +0x96C  maSubscriberDebugState[9]  per-IE debug state codes filled by Update
// ---------------------------------------------------------------------------

namespace MassiveAdClient3
{
    // ----- MassiveAd client objects/core (separate TUs; declarations only) -----
    //
    // CMassiveAdObjectSubscriber is the SDK's per-surface ad-subscriber object.
    // BrnMassiveSubscriber derives from it (BrnMassive::Destruct frees each pooled
    // entry through ~CMassiveAdObjectSubscriber + operator delete). Its body lives
    // in the MassiveAd client package; only the polymorphic-destructor declaration
    // and the X360-attested 88-byte footprint (the first BrnMassive-owned field is
    // stored at +0x58) are needed to compile/lay out BrnMassiveSubscriber.
    class CMassiveAdObjectSubscriber
    {
    public:
        virtual ~CMassiveAdObjectSubscriber();

        // The X360 frees a pooled subscriber through the MassiveAd heap hook
        // (BrnMassive::Destruct calls operator delete on the SDK base pointer).
        static void operator delete(void* lpMemory);

        // ----- SDK ad accessors (bodies live in the MassiveAd client package;
        // separate TUs). BrnMassiveSubscriber calls these non-virtually on
        // download-complete / per-tick. Declarations only here (compile gate is
        // per-TU cl /c); the addresses are attested in the X360 ledger:
        //   GetInvElementID -> the delivered ad's inventory-element id.
        //   GetCrexID       -> the delivered ad's creative id.
        //   SetImpression   -> record a viewed impression from the impression
        //                      data block (BrnMassiveSubscriber passes &+0x58).
        int GetInvElementID();
        int GetCrexID();
        int SetImpression(void* lpImpressionData);

    private:
        // Opaque SDK body. Size pinned by the ASM: CreateSubscriber's first own
        // store lands at +0x58, so the base occupies bytes [0x00, 0x58). The
        // leading 4 bytes are the polymorphic vftable pointer (virtual dtor).
        u8 maMassiveObjectBody[0x58 - sizeof(void*)];
    };

    // The MassiveAd client singleton. Static Instance() returns the live core (or
    // null before init); SuspendAll/ResumeAll gate ad streaming; Tick advances the
    // client. Bodies live in the MassiveAd client package (separate TU).
    class CMassiveClientCore;

    CMassiveClientCore* Instance();          // MassiveAdClient3::CMassiveClientCore::Instance
    int   SuspendAll(CMassiveClientCore* lpCore);
    int   ResumeAll(CMassiveClientCore* lpCore);
    int   Tick(int liArg);
}

namespace BrnMassive
{
    // Never-called layout-pin helper (defined in BrnMassive.cpp); befriended by
    // BrnMassive so its offsetof asserts can see the private members.
    void BrnMassive_AssertLayout();

    // -----------------------------------------------------------------------
    // Per-surface ad subscriber. Inline-pooled by value inside BrnMassive
    // (maSubscribers[15]); constructed in place by CreateSubscriber with the
    // surface's MassiveAd zone name (off_82F2413C[ie], e.g. "billboard_landscape01").
    // Derives from the MassiveAd SDK subscriber object. sizeof == 152 (0x98).
    //
    // Members below are exactly those the BrnMassive TU stores/loads by offset;
    // the SDK base supplies bytes [0x00,0x58). Unmapped gaps are reserved with
    // named padding so the asserted offsets hold.
    // -----------------------------------------------------------------------
    class BrnMassiveSubscriber : public MassiveAdClient3::CMassiveAdObjectSubscriber
    {
    public:
        // @ 0x823AB6C8. Constructs the SDK subscriber and logs "Creating Massive
        // Subscriber IE: <zone>" when message logging is enabled.
        explicit BrnMassiveSubscriber(const char* lpcZoneName);

        // PC instantiability (world-module mount): on the X360 a pool slot in
        // maSubscribers[15] is RAW storage until CreateSubscriber placement-
        // constructs it with the zone-name ctor above; C++ requires the by-value
        // pool default-constructible once BrnMassive is embedded up the
        // WorldModule chain (WorldEntityModule::mMassive, by value, inside the
        // game module). Empty body = the raw-slot state; CreateSubscriber remains
        // the real constructor of record.
        BrnMassiveSubscriber() {}

        // ----- MassiveAd ad-download callbacks (BrnMassiveSubscriber.cpp) -------
        // FLAG: these are the subscriber's MassiveAd callbacks; the constructor
        // installs the derived vtable (off_820369E8), so the class is polymorphic
        // via the inherited virtual dtor. No DWARF/base vtable is available for this
        // TU, so the callbacks' vtable slot order cannot be grounded - they are
        // declared as plain members (faithful bodies) rather than fabricating
        // `virtual` slot positions. Signatures/return match the X360 asm.
        int MediaDownload(int liAdvertId, int liArg3);                       // @ 0x823AB778
        int MediaDownloadComplete(int liDataValid, int liDataSize,
                                  int liFormat, int liAdvertId);            // @ 0x823AB858
        int SetImpressionData(u8 luFlags, f32 lfValue, int liArg4,
                              u32 luSize, u16 luWidth, u16 luHeight);        // @ 0x823AB9E0
        int Tick();                                                          // @ 0x823ABA20

        // Impression-data block @ +0x58 (SetImpression is handed &muField58). The
        // two leading dwords are reset accumulators; the trailing fields carry the
        // impression's flags/size/dimensions and a scalar. Zeroed by CreateSubscriber
        // and re-armed by SetImpressionData.
        u32 muField58;            // +0x58 : impression accumulator (reset 0)
        u32 muField5C;            // +0x5C : impression accumulator (reset 0)
        u8  mbHasContent;         // +0x60 : impression flags byte; non-zero -> a rendered ad is available (Update)
        u8  maPad61;              // +0x61
        u16 muWidth;              // +0x62 : default 1280
        u16 muHeight;             // +0x64 : default 720
        u8  maPad66[0x68 - 0x66]; // +0x66
        u32 muField68;            // +0x68 : impression size threshold (SetImpressionData gate/store)
        f32 mfField6C;            // +0x6C : impression scalar (stored single-precision)
        u8  maPad70[0x78 - 0x70]; // +0x70
        u32 muField78;            // +0x78 : delivered ad CrexID (MediaDownloadComplete)
        u32 muField7C;            // +0x7C : delivered ad InvElementID (MediaDownloadComplete)
        u32 muIE;                 // +0x80 : impression-event / zone index (GetSubscriberWithIE key)
        u32 muField84;            // +0x84 : expected payload size (CreateSubscriber arg a4)
        u32 muIdleFrames;         // +0x88 : frame counter, +1/Update until > 300
        u32 muField8C;            // +0x8C : texture-data pointer (CreateSubscriber arg a3)
        u32 muState;              // +0x90 : download/render state (0..3; 2 == ready-after-timeout)
        u8  mbField94;            // +0x94 : impression-pending flag (Tick sets 1, SetImpressionData clears)
        u8  maPad95[0x98 - 0x95]; // +0x95 : tail padding to sizeof 0x98
    };

    // -----------------------------------------------------------------------
    // BrnMassiveDebugComponent - the in-game debug overlay for the Massive pool.
    // Derives from CgsDev::DebugComponent. Embedded by value in BrnMassive at
    // +0x930; BrnMassive::Update writes a few of its fields directly, so they are
    // named here. Construct/registration body lives in its own TU.
    //
    // FLAG: the X360 reads these fields at fixed offsets from the BrnMassive base
    //   +0x93C mbEnabled        gate Update's per-subscriber debug pass
    //   +0x950 muUnpausedAds    ++ when a paused subscriber would otherwise stream
    //   +0x958 muViewIndex      selected subscriber index
    //   +0x95C muViewSubscriber resolved pointer for the selected index
    //   +0x968 mbViewPaused     paused-flag mirror
    // which place them at +0x0C / +0x20 / +0x28 / +0x2C / +0x38 inside the
    // component (so the X360 CgsDev::DebugComponent base is 12 bytes there). The
    // reconstructed host CgsDev::DebugComponent is larger (its shared header is
    // global state and must NOT be forked/padded), so the absolute X360 offsets
    // cannot be host-pinned here. Fields are named in ascending X360 order; the
    // intervening gaps are reserved with named padding sized to the X360 deltas so
    // the structure stays faithful, but the resulting host offsets intentionally
    // differ from the X360 ones (not asserted).
    // -----------------------------------------------------------------------
    class BrnMassiveDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // @ separate TU. Registers the per-IE debug-state array with the debug menu.
        void Construct(const char* const* lpapcZoneNames, s32 liNumZones,
                       const u32* lpauStateArray, const BrnMassiveSubscriber* lpaSubscribers);

        u8  mbEnabled;            // X360 +0x0C
        u8  maPad0D[0x20 - 0x0D]; // X360 +0x0D : 19-byte gap to muUnpausedAds
        u32 muUnpausedAds;        // X360 +0x20
        u8  maPad24[0x28 - 0x24]; // X360 +0x24
        u32 muViewIndex;          // X360 +0x28
        u32 muViewSubscriber;     // X360 +0x2C : raw subscriber pointer for the selected index
        u8  maPad30[0x38 - 0x30]; // X360 +0x30
        u8  mbViewPaused;         // X360 +0x38
        u8  maPad39[0x3C - 0x39]; // X360 +0x39 : tail to X360 sizeof 0x3C
    };

    // -----------------------------------------------------------------------
    // The MassiveAd lookup-table runtime record block pointed to by mpLookupTable.
    // GetSubscriberDataAtIndex walks an array of 64-byte records (an IE byte at
    // +0x30, a cached subscriber-data pointer at +0x28). Grounded in the ASM
    // (record stride 0x40, fields at +0x28/+0x30).
    // -----------------------------------------------------------------------
    struct MassiveLookupRecord
    {
        u8  maPad00[0x28];        // +0x00
        u32 muSubscriberData;     // +0x28 : cached GetSubscriberWithIE result (filled on demand)
        u32 muField2C;            // +0x2C : passed to GetSubscriberWithIE as the IE argument
        u8  meIE;                 // +0x30 : impression-event id this record describes
        u8  maPad31[0x40 - 0x31]; // +0x31 : record stride is 0x40
    };

    struct MassiveLookupTable
    {
        u32                  muNumRecords; // +0x00
        MassiveLookupRecord* mpRecords;    // +0x04
    };

    // Maximum pooled subscribers (15) and impression-event count (9); both from
    // the ASM compares (cmpwi 0xF / 9) in CreateSubscriber.
    const s32 KI_MAX_SUBSCRIBERS = 15;
    const s32 KI_NUM_IMPRESSION_EVENTS = 9;

    // -----------------------------------------------------------------------
    class BrnMassive
    {
        // Never-called layout pin (offsetof on private members).
        friend void BrnMassive_AssertLayout();

    public:
        // @ 0x823AB5C0. Zero-initialise the subscriber pool and counters.
        int Construct();

        // @ 0x823C6CE8. Tear down every pooled subscriber.
        int Destruct();

        // @ 0x823C12F0. Cache the lookup table, build the debug overlay, clear the
        // per-IE debug state array. liArg is the lookup-table pointer (stored raw).
        int Prepare(int liLookupTable);

        // @ 0x823C1370. Per-frame: age subscribers, tick the MassiveAd client, and
        // (when the debug overlay is enabled) recompute the per-IE debug states.
        void Update();

        // @ 0x823C1528. Construct a pooled subscriber for impression event liIE.
        int CreateSubscriber(int liIE, int liArg3, int liArg4);

        // @ 0x822A3E08. Return the pooled subscriber pointer at liIndex (asserts).
        int GetSubscriberAtIndex(int liIndex);

        // @ 0x822A3F08. Look up the lookup-table record for impression event lueIE,
        // resolving + caching its subscriber-data pointer on first use.
        u32 GetSubscriberDataAtIndex(u8 lueIE);

        // @ 0x823AB618. Find the pooled subscriber whose IE matches liIE (or 0).
        int GetSubscriberWithIE(int liIE);

        // @ 0x822A3FA8. True when the pool is ready/paused enough to download.
        int IsSafeToDownload();

        // @ 0x823AB668. Pause/resume; mirrors into the MassiveAd client core.
        void SetIsPaused(u8 lbPaused);

    private:
        BrnMassiveSubscriber     maSubscribers[KI_MAX_SUBSCRIBERS];  // +0x000
        BrnMassiveSubscriber*    mapSubscribers[KI_MAX_SUBSCRIBERS]; // +0x8E8
        s32                      mnNumActiveSubscribers;             // +0x924
        MassiveLookupTable*      mpLookupTable;                      // +0x928
        u8                       mbField92C;                         // +0x92C (= 1 at Construct)
        u8                       mbReady;                            // +0x92D
        u8                       mbField92E;                         // +0x92E
        u8                       mbIsPaused;                         // +0x92F
        BrnMassiveDebugComponent mDebugComponent;                    // +0x930
        u32                      maSubscriberDebugState[KI_NUM_IMPRESSION_EVENTS]; // +0x96C
    };
}

#endif // GAMESOURCE_MASSIVE_BRNMASSIVE_H
