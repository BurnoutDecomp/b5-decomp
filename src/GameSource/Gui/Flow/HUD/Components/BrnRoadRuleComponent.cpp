#include "GameSource/Gui/Flow/HUD/Components/BrnRoadRuleComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                                // GuiCache (mode / score-mode / in-event gate reads)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                 // GsmIO::EGameModeType (in-event colouring gate)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptRoadSignIconComponent.h" // FlaptRoadSignIconComponent (SetColour)

// ============================================================================
// BrnGui::RoadRuleComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// This TU bodies five of the class's ledger functions (the class declaration and
// its three X360 header-inlines -- EnterRoad / HandleLeaveRoadEvent /
// HandleUpcomingRoadEvent -- live in the header). Bodied here:
//   IsSameAsCurrentRoad       @0x82410458
//   ShouldUseInEventColouring @0x82410568
//   SetCurrentSignState       @0x82410640
//   UpdateUpcomingRoadLeaders @0x82410720
//   SetCachePointer           @0x82473520
//
// X360-pinned RoadRuleComponent members these bodies touch (this-relative byte
// offsets from the asm): meCurrentSignState @+0x1F0, mTransitionData @+0x3E8
// (miRoadIndex @+0x430), mUpcomingRoadData @+0x460, mpGuiCache @+0x508.
//
// X360 vs PS3-DWARF EVENT INTERIOR: the road-rule "upcoming roads" event's
// interior member offsets on X360 do NOT match the PS3 DWARF's declared member
// order (e.g. the per-side leader-type block the X360 reads sits at object +0x30,
// not the DWARF-natural +0x0C). IsSameAsCurrentRoad / UpdateUpcomingRoadLeaders
// therefore access the event through its X360-attested byte offsets rather than by
// the (unverified-interior) DWARF member names, so the reconstruction stays
// store-for-store. The offsets are relative to the full event object (its 12-byte
// GuiEvent header included), matching the asm's base register.
// ============================================================================

namespace BrnGui
{

namespace
{
    // --- SetCurrentSignState animation-frame tables (X360 off_82F24D08 /
    //     off_82F24CF8 / dword_8204BDA8). Only the first entry of each string
    //     table is attested in the dossier ("invisible" / "small"); the remaining
    //     entries and the road-index->size-index lookup are NOT recovered from the
    //     XEX rodata in this slice. Declared here as the named DWARF constants so
    //     the body resolves against them; their full contents land with the
    //     RoadRuleComponent constant TU. (FLAG: contents unrecovered.)
    extern const char* const KAPC_ANIMATION_FRAMES[RoadRuleComponent::E_ANIMATION_STATE_ADVANCED_COUNT]; // off_82F24D08
    extern const char* const KAAC_ROADSIGN_SIZE_FRAME_LABELS[];                                          // off_82F24CF8
    extern const s32         KAI_ROAD_INDEX_TO_SIGN_SIZE[];                                              // dword_8204BDA8

    // BrnRoadRuleComponent.h:277 (KI_MAX_ROAD_INDEX) -- the miRoadIndex bound the
    // X360 SetCurrentSignState asserts against (cmpwi 0x42).
    const s32 KI_MAX_ROAD_INDEX = 66;

    // --- DEFINITIONS of the three animation-frame tables above ------------------
    // *** UNRECOVERED VALUES -- CONSOLIDATOR MUST FILL ***  Only the FIRST cell of
    // each string table is X360-attested ("invisible" / "small"); the remaining cells
    // and the whole road-index->size-index lookup are NOT recovered from the XEX rodata
    // in this slice. Read off_82F24D08 / off_82F24CF8 / dword_8204BDA8 and replace the
    // placeholders. Sizes are pinned by the enums (E_ANIMATION_STATE_ADVANCED_COUNT /
    // E_ROAD_SIGN_COUNT) and by the KI_MAX_ROAD_INDEX assert bound. (FLAG: contents
    // unrecovered; defined here as internal-linkage placeholders so the TU compiles.)
    const char* const KAPC_ANIMATION_FRAMES[RoadRuleComponent::E_ANIMATION_STATE_ADVANCED_COUNT] =
    {
        "invisible",          // [0] E_ANIMATION_STATE_INVISIBLE -- X360-attested
        "", "", "", "", "",   // [1]..[5] TODO(consolidator) off_82F24D08
    };
    const char* const KAAC_ROADSIGN_SIZE_FRAME_LABELS[RoadRuleComponent::E_ROAD_SIGN_COUNT] =
    {
        "small",       // [0] E_ROAD_SIGN_SMALL -- X360-attested
        "", "", "",    // [1]..[3] TODO(consolidator) off_82F24CF8
    };
    const s32 KAI_ROAD_INDEX_TO_SIGN_SIZE[KI_MAX_ROAD_INDEX] =
    {
        0,   // [0..65] TODO(consolidator) dword_8204BDA8 -- road-index -> ERoadSignSizes;
             // remaining entries default-zero (E_ROAD_SIGN_SMALL) until recovered.
    };
}

// @ 0x82410458 -- BrnRoadRuleComponent.h:707/:708. Is the cached upcoming-road for
// side leRoadSide identical to the same side in lpEvent? Compares the per-side road
// id (qword) plus three per-side state/index words between the component's cached
// mUpcomingRoadData and the freshly-arrived event; any difference -> not the same.
bool RoadRuleComponent::IsSameAsCurrentRoad(GuiEventRoadRuleUpcomingRoads::ERoadSide leSide,
                                            const GuiEventRoadRuleUpcomingRoads* lpEvent) const
{
    CGS_ASSERT(leSide < GuiEventRoadRuleUpcomingRoads::E_ROAD_COUNT,
               "(0 <= leRoadSide) && (GuiEventRoadRuleUpcomingRoads::E_ROAD_COUNT > leRoadSide)"); // :707 (non-gating)
    CGS_ASSERT(0 != lpEvent, "NULL != lpEvent");   // :708 (non-gating)

    // X360-attested event-object byte offsets (see file header). The cached side is
    // read from mUpcomingRoadData; the incoming side straight from lpEvent.
    const u8* lpCached  = reinterpret_cast<const u8*>(&mUpcomingRoadData);
    const u8* lpIncoming = reinterpret_cast<const u8*>(lpEvent);

    const u32 luSideQword = static_cast<u32>(leSide) * 8u;   // qword-strided fields
    const u32 luSideWord  = static_cast<u32>(leSide) * 4u;   // word-strided field

    // 1) per-side road id (qword @ +0x00 + side*8).
    if (*reinterpret_cast<const u64*>(lpIncoming + luSideQword) !=
        *reinterpret_cast<const u64*>(lpCached + luSideQword))
    {
        return false;
    }
    // 2) per-side word @ +0x6C + side*4.
    if (*reinterpret_cast<const s32*>(lpIncoming + 0x6C + luSideWord) !=
        *reinterpret_cast<const s32*>(lpCached + 0x6C + luSideWord))
    {
        return false;
    }
    // 3) per-side word @ +0x30 + side*8.
    if (*reinterpret_cast<const s32*>(lpIncoming + 0x30 + luSideQword) !=
        *reinterpret_cast<const s32*>(lpCached + 0x30 + luSideQword))
    {
        return false;
    }
    // 4) per-side word @ +0x34 + side*8.
    if (*reinterpret_cast<const s32*>(lpIncoming + 0x34 + luSideQword) !=
        *reinterpret_cast<const s32*>(lpCached + 0x34 + luSideQword))
    {
        return false;
    }
    return true;
}

// @ 0x82410568 -- BrnRoadRuleComponent.h:731. Should the road sign fall back to the
// "in-event" (green) colouring? True only in the showtime-family game modes
// (offline showtime / online showtime / free-burn lobby) AND while the cache's
// in-event colouring gate byte is set.
bool RoadRuleComponent::ShouldUseInEventColouring()
{
    CGS_ASSERT(0 != mpGuiCache, "mpGuiCache");   // :731 (non-gating)

    const s32 leGameMode = mpGuiCache->GetGameMode();

    // v7 in the asm: mode is one of the showtime-family modes.
    const bool lbShowtimeFamily =
        (leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY) ||
        (leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) ||
        (leGameMode == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME);

    // The cache's in-event colouring gate byte (X360 mpGuiCache+0x4B4A); when clear
    // the sign never uses in-event colouring.
    if (!mpGuiCache->GetInEventColouringGate())
    {
        return false;
    }
    return lbShowtimeFamily;
}

// @ 0x82410640 -- BrnRoadRuleComponent.h:755/:767. Drive the current sign to a new
// animation state: play the state's animation frame on the animation clip and stop
// the two road-sign clips on the size-label frame for the transition road's index.
// No-op when already in that state.
void RoadRuleComponent::SetCurrentSignState(EAnimationState leState)
{
    CGS_ASSERT((leState >= E_ANIMATION_STATE_FIRST) && (leState < E_ANIMATION_STATE_ADVANCED_COUNT),
               "( E_ANIMATION_STATE_FIRST <= leNewSignState ) && ( E_ANIMATION_STATE_ADVANCED_COUNT > leNewSignState )"); // :755 (non-gating)

    if (meCurrentSignState == leState)
    {
        return;
    }

    // Play the state's animation frame on the animation clip. FLAG: the X360 plays
    // on the clip at this+0x0C and stops the two sign clips at this+0x04 / +0x14 --
    // the second, first and third leading MovieClipRefs by attested offset. The
    // header's leading-member names are DWARF-declared but their head byte offsets
    // are not independently X360-pinned, so this maps play->2nd, stops->1st/3rd by
    // slot to preserve the exact clip targeting.
    mSubComponentPositionsMC.GotoAndPlayLabel(KAPC_ANIMATION_FRAMES[leState]);

    CGS_ASSERT(mTransitionData.miRoadIndex < KI_MAX_ROAD_INDEX,
               "mTransitionData.miRoadIndex < KI_MAX_ROAD_INDEX");   // :767 (non-gating)

    // Pick the size-label frame from the transition road's index and stop both sign
    // clips on it.
    const char* lpcSizeFrame =
        KAAC_ROADSIGN_SIZE_FRAME_LABELS[KAI_ROAD_INDEX_TO_SIGN_SIZE[mTransitionData.miRoadIndex]];
    mRoadSignAnimationsMC.GotoAndStopLabel(lpcSizeFrame);
    mLeftSignAlignmentsMC.GotoAndStopLabel(lpcSizeFrame);

    meCurrentSignState = leState;
}

// @ 0x82410720 -- BrnRoadRuleComponent.h:795 / BrnGuiCache.h:4210. Refresh the two
// upcoming-road panels' active leader-type slots from either the online or the
// offline leader-type set, chosen by the cache's road-rule score mode. Writes into
// the passed event's active leader-type block.
void RoadRuleComponent::UpdateUpcomingRoadLeaders(GuiEventRoadRuleUpcomingRoads* lpEvent)
{
    CGS_ASSERT(0 != mpGuiCache, "mpGuiCache");   // :795 (non-gating)

    // The road-rule score mode (X360 mpGuiCache+0xAC40). The accessor reproduces the
    // X360-inlined "E_ROAD_PANEL_MODE_COUNT != meRoadRuleScoreMode" assert
    // (BrnGuiCache.h:4210, non-gating) this call site emits.
    const s32 leScoreMode = mpGuiCache->GetActiveRoadRuleScoringMode();

    // X360-attested event-object byte offsets (see file header): the active
    // leader-type block @ +0x30 (two per-side words at +0x30 / +0x38), the offline
    // set @ +0x40 (+0x40 / +0x48) and the online set @ +0x50 (+0x50 / +0x58); the
    // loop advances one word per score-type index, running E_SCORE_TYPE_COUNT times.
    u8* lpBase = reinterpret_cast<u8*>(lpEvent) + 0x30;
    const bool lbUseOnline = (leScoreMode == 1);
    const u32 luSrc0 = lbUseOnline ? 0x20u : 0x10u;   // v5[8]/v5[4] relative to v5
    const u32 luSrc1 = lbUseOnline ? 0x28u : 0x18u;   // v5[10]/v5[6] relative to v5

    for (s32 liEnumIndex = 0; ; )
    {
        u8* lpSlot = lpBase;
        *reinterpret_cast<s32*>(lpSlot + 0x00) = *reinterpret_cast<const s32*>(lpSlot + luSrc0);
        *reinterpret_cast<s32*>(lpSlot + 0x08) = *reinterpret_cast<const s32*>(lpSlot + luSrc1);
        lpBase += 4;
        ++liEnumIndex;
        CGS_ASSERT(liEnumIndex <= BrnStreetData::E_SCORE_TYPE_COUNT,
                   "leEnumIndex <= E_SCORE_TYPE_COUNT");   // BrnChallengeData.h:56 (non-gating)
        if (liEnumIndex >= 2)
        {
            break;
        }
    }
}

// @ 0x82473520 -- BrnRoadRuleComponent.h:628. Adopt the GUI cache pointer.
void RoadRuleComponent::SetCachePointer(GuiCache* lpGuiCache)
{
    CGS_ASSERT(0 != lpGuiCache, "lpCache");   // :628 (non-gating)
    mpGuiCache = lpGuiCache;
}

}
