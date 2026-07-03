#include "GameSource/Replays/Serialisers/BrnReplayDirectorBridgeSerialiser.h"

// BrnReplays::DirectorBridgeSerialiser -- reconstructed from
// BURNOUT_X360_ARTIST.XEX. (This file's earlier Construct-only slice is
// DE-FORKED onto the real header home; its file-local BaseSerialiser fork is
// retired.)
//
// Bodied here (3 of the TU's ledger functions; the six getters are inline in
// the header, matching the original's h:193.. placement):
//   Construct @0x8264C498   Read @0x82657478   Write @0x82657210
//
// Read and Write stream the SAME ten snapshot spans (the Write order below);
// Read adds the mode-skip ladder up front (the batch-11 DirectorSerialiser
// pattern) while Write only null-checks the buffer. Each span re-reads the mode
// and dispatches RECORDING -> append / PLAYING -> restore, faithfully.

namespace BrnReplays
{

namespace
{
    // The serialise plan (offset into the static layout, byte size) in the
    // X360's stream order.
    struct SpanPlan { u32 muOffset; s32 miSize; };
    const SpanPlan KA_BRIDGE_SPANS[] =
    {
        { 0x0000, 1     },   // muActivePlayerIndex
        { 0x0003, 8     },   // mabVehicleActive
        { 0x0010, 10112 },   // maVehicleInfos (8 x 1264)
        { 0x5BA0, 48    },   // maBlock5BA0
        { 0x5BD0, 512   },   // maCentreOfMassTransforms (8 x 64)
        { 0x5DD0, 296   },   // maPlayerData
        { 0x5EF8, 1     },   // mBlock5EF8
        { 0x5EFC, 4     },   // maBlock5EFC
        { 0x0002, 1     },   // mbPlayerWasTakenDown
        { 0x0001, 1     },   // muPlayerKillerIndex
    };
    const s32 KI_NUM_BRIDGE_SPANS = sizeof(KA_BRIDGE_SPANS) / sizeof(KA_BRIDGE_SPANS[0]);
}

// One span step: RECORDING appends, PLAYING restores, anything else no-ops (the
// X360 re-reads the mode word per span; taking the live serialiser keeps that).
static void DirectorBridgeSerialiser_StreamSpan(BaseSerialiser* lpSerialiser,
                                                u8* lpStaticBase, const SpanPlan& lrSpan)
{
    if (lpSerialiser->GetMode() == BaseSerialiser::E_MODE_RECORDING)
        lpSerialiser->Write(lpStaticBase + lrSpan.muOffset, lrSpan.miSize);
    else if (lpSerialiser->GetMode() == BaseSerialiser::E_MODE_PLAYING)
        lpSerialiser->Read(lpStaticBase + lrSpan.muOffset, lrSpan.miSize);
}

// @ 0x8264C498 -- the leaf forwarder: id 3, mode 0, the 0x8000 buffer pair,
// the channel name, flag 0.
s32 DirectorBridgeSerialiser::Construct()
{
    return BaseSerialiser::Construct(3, 0, 0x8000, 0x8000, "DirectorBridge", 0);
}

// @ 0x82657478 -- cpp:149. Lock, resolve the snapshot, run the mode-skip ladder
// (STALLED 3/6, PREPARING 1/4, RESTORING 7 no-op), then stream the ten spans.
// Always Unlocks.
void DirectorBridgeSerialiser::Read()
{
    BaseSerialiser::Lock();

    DirectorBridgeSerialiserStaticLayout* lpStatic = GetStaticLayout();
    CGS_ASSERT(lpStatic != 0, "lpStatic");   // :149 (non-gating)

    const EMode leMode = GetMode();
    bool lbSkip = (leMode == E_MODE_RECORDING_STALLED || leMode == E_MODE_PLAYING_STALLED);
    if (!lbSkip)
    {
        lbSkip = (leMode == E_MODE_RECORDING_PREPARING || leMode == E_MODE_PLAYING_PREPARING);
        if (!lbSkip && leMode != E_MODE_RESTORING)
        {
            u8* lpBase = reinterpret_cast<u8*>(lpStatic);
            for (s32 liSpan = 0; liSpan < KI_NUM_BRIDGE_SPANS; ++liSpan)
                DirectorBridgeSerialiser_StreamSpan(this, lpBase, KA_BRIDGE_SPANS[liSpan]);
            SerialiseGameActionQueue();   // the queue stream step (own ledger fn)
        }
    }

    BaseSerialiser::Unlock();
}

// @ 0x82657210 -- lock, resolve, stream the same ten spans (only the null
// guard, no skip ladder). Always Unlocks.
void DirectorBridgeSerialiser::Write()
{
    BaseSerialiser::Lock();

    DirectorBridgeSerialiserStaticLayout* lpStatic = GetStaticLayout();
    if (lpStatic != 0)
    {
        u8* lpBase = reinterpret_cast<u8*>(lpStatic);
        for (s32 liSpan = 0; liSpan < KI_NUM_BRIDGE_SPANS; ++liSpan)
            DirectorBridgeSerialiser_StreamSpan(this, lpBase, KA_BRIDGE_SPANS[liSpan]);
        SerialiseGameActionQueue();   // the queue stream step (own ledger fn)
    }

    BaseSerialiser::Unlock();
}

}
