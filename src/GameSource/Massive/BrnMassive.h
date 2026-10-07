#ifndef GAMESOURCE_MASSIVE_BRNMASSIVE_H
#define GAMESOURCE_MASSIVE_BRNMASSIVE_H

#include "types.hpp"
#include "GameSource/Massive/BrnMassiveDebugComponent.h"
#include "SharedClasses/Massive/MassiveLookupTable.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Subscriber.h"   // CMassiveAdObjectSubscriber (base of BrnMassiveSubscriber)

// ---------------------------------------------------------------------------
// BrnMassive - Burnout's in-engine bridge to the MassiveAd in-game advertising
// client (MassiveAdClient3). It owns a fixed pool of ad subscribers (one per
// in-world advert impression event), drives the MassiveAd client tick, and feeds
// a debug overlay describing each subscriber's download/render state.
//
// No debug-info declaration exists for this TU (only this platform has Massive); the layout
// is recovered from the console's stores/loads and is reached BY NAME here:
//
//   +0x000  maSubscriberPool           inline BrnMassiveSubscriber pool (15 * 152)
//   +0x8E8  mapSubscribers[15]         pointers into the pool (CreateSubscriber)
//   +0x924  mnNumActiveSubscribers     active-slot count
//   +0x928  mpLookupTable              the loaded "MassiveTable" resource
//   +0x92C  mbField92C                 (= 0 at Construct; never read)
//   +0x92D  mbSafeToDownload           IsSafeToDownload
//   +0x92E  mbImpressionFrameStarted   per-frame latch of the impression pass
//   +0x92F  mbIsPaused                 SetIsPaused mirrors this into the client core
//   +0x930  mDebugComponent            BrnMassiveDebugComponent (60 bytes)
//   +0x96C  maiSubscriberDebugState[9] per-IE debug state codes filled by Update
// ---------------------------------------------------------------------------

namespace BrnMassive
{
    // Maximum pooled subscribers (15) and impression-event count (9).
    const s32 KI_MAX_SUBSCRIBERS       = 15;
    const s32 KI_NUM_IMPRESSION_EVENTS = 9;

    // -----------------------------------------------------------------------
    // Per-surface ad subscriber. Inline-pooled by value inside BrnMassive and
    // constructed in place by CreateSubscriber with the surface's MassiveAd zone
    // name. Implements the SDK's three download callbacks.
    // -----------------------------------------------------------------------
    class BrnMassiveSubscriber : public MassiveAdClient3::CMassiveAdObjectSubscriber
    {
    public:
        // Constructs the SDK subscriber and logs "Creating Massive Subscriber IE: <zone>".
        explicit BrnMassiveSubscriber(const char* lpcZoneName);

        // ----- SDK download callbacks ----------------------------------------
        int  MediaDownload(int liAdvertId) override;
        int  MediaDownloadComplete(const void* lpData, int liDataSize,
                                   unsigned int luMediaType, int liAdvertId) override;
        void Tick() override;

        // Latch a fresh impression description when it supersedes the stored one
        // (a larger screen size, or the previous one has been reported by Tick).
        void SetImpressionData(bool lbInView, f32 lfAngle, u32 luScreenSize,
                               u16 luScreenWidth, u16 luScreenHeight);

        // The 32-byte impression record handed to the SDK by Tick
        // (CMassiveAdObjectSubscriber::SetImpression copies it whole).
        struct ImpressionData
        {
            u32  muAccumulator0;        // +0x00 reset with each new impression
            u32  muAccumulator1;        // +0x04 reset with each new impression
            bool mbInView;              // +0x08
            u16  muScreenWidth;         // +0x0A reference screen (1280)
            u16  muScreenHeight;        // +0x0C reference screen (720)
            u32  muScreenSize;          // +0x10 projected size of the advert's box
            f32  mfAngle;               // +0x14 facing term, clamped to [0, 1]
            u8   maPad[8];              // +0x18
        };

        ImpressionData mImpression;     // +0x58

        u32  muCrexID;                  // +0x78 delivered creative id
        s32  miInvElementID;            // +0x7C delivered inventory-element id
        s32  miIE;                      // +0x80 impression-event index (GetSubscriberWithIE key)
        u32  muExpectedSize;            // +0x84 replacement texture pixel-data size
        u32  muIdleFrames;              // +0x88 frames since the last download activity
        void* mpTextureData;            // +0x8C replacement texture pixel data
        s32  miState;                   // +0x90 0 idle, 1 downloading, 2 timed out, 3 ready
        bool mbImpressionReported;      // +0x94 Tick has handed the impression to the SDK
    };

    // -----------------------------------------------------------------------
    class BrnMassive
    {
    public:
        int  Construct();
        void Destruct();

        // Cache the lookup table, build + register the debug overlay, clear the
        // per-IE debug state array.
        bool Prepare(MassiveLookupTable* lpLookupTable);

        // Per frame: age subscribers, tick the MassiveAd client, and (when the debug
        // overlay is on) recompute the per-IE debug states.
        void Update();

        // Construct a pooled subscriber for impression event liIE.
        BrnMassiveSubscriber* CreateSubscriber(s32 liIE, void* lpTextureData, u32 luExpectedSize);

        BrnMassiveSubscriber*   GetSubscriberAtIndex(s32 liIndex);
        MassiveLookupTableItem* GetSubscriberDataAtIndex(u8 luRenderableIndex);
        BrnMassiveSubscriber*   GetSubscriberWithIE(s32 liIE);
        bool                    IsSafeToDownload();
        void                    SetIsPaused(bool lbPaused);

        // Read and written directly by the world entity module's impression pass.
        bool                    mbSafeToDownload;                                  // +0x92D
        bool                    mbImpressionFrameStarted;                          // +0x92E
        bool                    mbIsPaused;                                        // +0x92F

        s32                     GetNumActiveSubscribers() const { return mnNumActiveSubscribers; }

    private:
        BrnMassiveSubscriber* GetPoolSlot(s32 liSlot)
        {
            return reinterpret_cast<BrnMassiveSubscriber*>(maSubscriberPool) + liSlot;
        }

        // The inline subscriber pool: raw storage that Construct zeroes and
        // CreateSubscriber placement-constructs one slot of at a time. FLAG PC: held as
        // bytes so no C++ constructor/destructor runs on unused slots.
        alignas(BrnMassiveSubscriber) u8 maSubscriberPool[KI_MAX_SUBSCRIBERS * sizeof(BrnMassiveSubscriber)]; // +0x000
        BrnMassiveSubscriber*    mapSubscribers[KI_MAX_SUBSCRIBERS];               // +0x8E8
        s32                      mnNumActiveSubscribers;                           // +0x924
        MassiveLookupTable*      mpLookupTable;                                    // +0x928
        bool                     mbField92C;                                       // +0x92C
        BrnMassiveDebugComponent mDebugComponent;                                  // +0x930
        s32                      maiSubscriberDebugState[KI_NUM_IMPRESSION_EVENTS]; // +0x96C
    };
}

#endif // GAMESOURCE_MASSIVE_BRNMASSIVE_H
