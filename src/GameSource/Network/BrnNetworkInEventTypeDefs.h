#pragma once

// ===================================================================================
// BrnNetwork::BrnNetworkModuleIO network IN-event leaves -- owning header
//   b5-decomp/src/GameSource/Network/BrnNetworkInEventTypeDefs.h
//
// Each leaf is a NetworkEvent<N> (the empty Event spine + the compile-time event-type tag
// N it is queued under). N is the console tag the producers pass to
// VariableEventQueue<14000,16>::AddEvent, which drifts from the reference numbering for the
// later leaves (the select-scoreboard event is queued as 49, the reference says 47).
// A leaf whose tag is not yet attested is modelled on the empty Event base directly; both
// bases are byte-identical (no data members, first field at +0x00).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT (builder >= 0 guards)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"      // BrnNetwork::Event, NetworkEvent<N>, TelemetryData
#include "GameSource/Network/Shared Server Types/BrnNetworkSharedServerTypes.h" // ServerGeneratedTypes::OfflineProgressionT
#include "pc/gcm/renderengine/pixelformat.h"                     // renderengine::PixelFormat (NetworkInReqCamPicEvent)

namespace CgsNetwork
{
    class NetworkTexture;   // GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h (held by pointer only)
}

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // DWARF (BrnNetworkInEventTypeDefs.h:438) -- the IN-event that delivers a player's
    // accumulated offline-play progress to the network player-stats manager. DWARF spells it
    // `: public NetworkEvent<33>` (tag 33); the empty BrnNetwork::Event base is byte-identical
    // to that NetworkEvent<N> base (no data members, first field at +0x00 -- see the
    // banner), so it is modelled on Event directly.
    //
    // LAYOUT is X360-AUTHORITATIVE from NetworkPlayerStatsManager::HandleOfflineProgressionEvent
    // (@0x82546BB0): that body memcpy's exactly 0x44 == 68 bytes of this event into its buffered
    // copy, and reads a 32-bit word at event+0x40 == +64 as the freeburn-challenge success count
    // (handed to ServerInterfaceCustomCommands::UploadOfflineProgress as its count argument).
    // The 64-byte OfflineProgressionT at +0 plus that trailing s32 give the 68-byte total. The
    // trailing count field is not present in the (incomplete) DWARF member list but is required
    // by the asm; FLAGGED as asm-recovered.
    struct NetworkInOfflineProgression : public Event
    {
        ServerGeneratedTypes::OfflineProgressionT mOfflineProgression; // +0x00 (64 bytes)
        s32                                       miFreeburnChallengeSuccessCount; // +0x40 (asm-recovered)
    };

    // The select-scoreboard IN-event (reference home line 668). The GUI bridge and the
    // scoreboard debug component build it on the stack: Prepare() (inlined: the three headings
    // to KI_INVALID_HEADING, meType to E_TYPE_NONE), then ONE selector, then AddEvent. The
    // debug component skips Prepare, so its unset headings are stack garbage exactly as on the
    // console. ScoreboardManager::ProcessEventQueue pops it and reads it back through the
    // GetChosen* accessors (their asserts sit at the reference header's lines 1109/1124/1139).
    //
    //   +0x00 miCategory   GetIndexes     (asserts >= 0; reference line 1031)
    //   +0x04 miIndex      GetVariations  (asserts >= 0; reference line 1048)
    //   +0x08 miVariation  GetScoreboard  (asserts >= 0; reference line 1065)
    //   +0x0C meType       every selector
    //
    // GetIndexes / GetVariations / GetScoreboard are out of line on the console (their
    // asserts keep them from inlining); the rest inline at every call site.
    struct NetworkInSelectScoreboardEvent : public NetworkEvent<49>
    {
        enum EScoreboardEventType
        {
            E_TYPE_NONE           = 0,
            E_TYPE_GET_CATEGORY   = 1,
            E_TYPE_GET_INDEX      = 2,
            E_TYPE_GET_VARIATION  = 3,
            E_TYPE_GET_SCOREBOARD = 4,
            E_TYPE_PAGE_UP        = 5,
            E_TYPE_PAGE_DOWN      = 6,
            E_TYPE_COUNT          = 7,
        };

        static const s32 KI_INVALID_HEADING = -1;

        void Prepare()
        {
            miCategory  = KI_INVALID_HEADING;
            miIndex     = KI_INVALID_HEADING;
            miVariation = KI_INVALID_HEADING;
            meType      = E_TYPE_NONE;
        }

        void GetCategories() { meType = E_TYPE_GET_CATEGORY; }
        void GetIndexes(s32 liCategory);
        void GetVariations(s32 liIndex);
        void GetScoreboard(s32 liVariation);
        void PageUp()        { meType = E_TYPE_PAGE_UP; }
        void PageDown()      { meType = E_TYPE_PAGE_DOWN; }

        s32 GetChosenCategory() const
        {
            CGS_ASSERT(miCategory != KI_INVALID_HEADING, "miCategory != KI_INVALID_HEADING");
            return miCategory;
        }
        s32 GetChosenIndex() const
        {
            CGS_ASSERT(miIndex != KI_INVALID_HEADING, "miIndex != KI_INVALID_HEADING");
            return miIndex;
        }
        s32 GetChosenVariation() const
        {
            CGS_ASSERT(miVariation != KI_INVALID_HEADING, "miVariation != KI_INVALID_HEADING");
            return miVariation;
        }
        EScoreboardEventType GetScoreboardEventType() { return meType; }

    private:
        s32                  miCategory;    // +0x00
        s32                  miIndex;       // +0x04
        s32                  miVariation;   // +0x08
        EScoreboardEventType meType;        // +0x0C

        static void _AssertLayout();
    };

    // The telemetry IN-event (reference home line 273, tag 16 on both builds): one
    // TelemetryData record, queued with its full 20-byte size.
    struct NetworkInTelemetryEvent : public NetworkEvent<16>
    {
        TelemetryData mEventData;   // +0x00
    };
    static_assert(sizeof(NetworkInTelemetryEvent) == 20, "NetworkInTelemetryEvent is queued as 20 bytes");

    // The compressed camera-picture request (reference home line 776). Queued as tag 53 with
    // the console size 12; the texture pointer makes the host record wider, so producers queue
    // sizeof().
    struct NetworkInReqCamPicEvent : public NetworkEvent<53>
    {
        s32                         miQualitySetting;            // +0x00
        renderengine::PixelFormat   meCompressedFormat;          // +0x04
        CgsNetwork::NetworkTexture* mpTextureToCompressedInto;   // +0x08 (4 bytes on the console)
    };
}
} // namespace BrnNetwork
