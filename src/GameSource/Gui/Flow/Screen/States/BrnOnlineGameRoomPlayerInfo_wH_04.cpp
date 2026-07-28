// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 04: the player-list heart.
//   ShowPlayerList     @0x824A5968  (DWARF cpp:3371)
//   HighlightNewPlayer @0x824991E0  (DWARF cpp:3494)
//   GetPlayerOptions   @0x82484CC0  (DWARF cpp:3263)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (raw asm arbitrating; the
// Hex-Rays rendering is a hint only -- see the two notes below where it is WRONG).
// Ground truth: scratchpad/waveB/GameSource_Gui_Flow_Screen_States_
//               BrnOnlineGameRoomPlayerInfo.cpp.asm.txt
//
// ShowPlayerList repaints the whole 8-row lobby list: for each active player it walks
// TWO parallel GuiCache mirrors -- maPlayerInfo[i] (the 312-byte in-game status record)
// and maLobbyPlayerInfo[i] (the 56-byte lobby status record) -- and pushes eight status
// icon states per row, then blanks the unused rows and restores the highlight.
//
// TWO Hex-Rays MISREADINGS arbitrated from the assembly:
//   1. GetPlayerOptions: the pseudocode renders the unconditional option store as
//      `a2[v8] = 0xB86400000001LL`. The asm is
//          li r10, 1 ; lis r9, 0 ; ori r9, r9, 0xB864 ; stdx r10, r11, r29
//      -- r9 is the *offset* of GuiCache::mbIsOnlineHost, consumed by the `lbzx r11,
//      r3, r9` two instructions later; the value stored is r10 == 1. Hex-Rays folded
//      the unrelated `ori` into the immediate. The option id written is 1.
//   2. GetPlayerOptions: `LODWORD(v11) = *(OnlinePlayerI + 302); *a2 = v11` is the
//      compiler reusing the just-tested mbIsLocalPlayer byte (known 0 on that path) as
//      the constant 0. The pair of stores is ids[0] = 0, ids[1] = 2.
//
// De-inlining performed (AGENTS.md "Inlining reversal"), all X360-attested:
//   * The per-row `cache + 312*i + 0xAC80` address arithmetic carries GetOnlinePlayerInfo's
//     own two asserts VERBATIM (file "..\\GameSource\\Gui/BrnGuiCache.h", lines 3645/3646
//     -- exactly the pair in the committed GuiCache::GetOnlinePlayerInfo body), so the
//     compiler inlined that accessor; it is restored as the call.
//   * The two "Ended up with bad ..." asserts are the CgsDev::Assert::BeginAssert /
//     StrStream-into-gpcMessageBuffer / FireAssert / EndAssert machinery inlined at the
//     call site. Per the project convention (BrnReplayLoading.cpp / BrnImageGallery.cpp)
//     that collapses to one CGS_ASSERT carrying the verbatim rodata prefix; the streamed
//     `<< <status>` value is dropped with the machinery.
//
// X360-LITERAL AUDIT: every console offset/stride in these bodies is seeded by a member
// layout (39149 = mPlayerNameComponent.miHighlightedIndex, 87516 = mpGuiCache,
// 24928 + 1184*i = maIcons[i], 71104 = mPlayerStatsDisplay, 84880 =
// mPlayersButtonPromptsAnimation, cache+0xAC80/312*i = maPlayerInfo[i],
// cache+0xB640/56*i = maLobbyPlayerInfo[i], cache+0xA9DF = mbOnlineRanked,
// cache+0xB864 = mbIsOnlineHost, row+0x100 = mPlayerName, row+248 =
// mLiveRevengeRelationship's +112 score word) -- so NONE of them is
// reproduced as a number here; the console values live in comments only. The two array
// strides that ARE reproduced (maPlayerInfo/maLobbyPlayerInfo row addressing) go through
// the named `u8[8][N]` cache members, whose row width is pinned host-side by
// static_assert(sizeof(InGamePlayerStatusData) == 312 / sizeof(LobbyPlayerStatusData) == 56)
// in those records' own headers.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (friend access)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"      // InGamePlayerStatusData
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // GuiEventOut

        // The apt view-name every AddOutputAptViewState call on this screen targets.
        const char KAC_APT_TRANSITION[] = "apt_Transition";

        // ---- out-queue wire record (HighlightNewPlayer) --------------------------------
        // The X360 stack-builds { 4, 247, 12, <lobby mPlayerID> } and AddEvent()s it with
        // size 16 on channel 40 -- i.e. GuiEvent<247> carrying one 4-byte payload word,
        // the network player id of the row that just became highlighted.
        //
        // FLAG -- id/name mismatch, reported not resolved: the DecFIGS DWARF types
        // GuiEvent<247> as BrnGui::GuiEventNetworkFinishRound with an EMPTY payload
        // (CgsGuiEvent.h GuiEventWrapper<GuiEvent<247>,40>), which cannot be the event
        // ARTIST emits here. Neither BrnGuiEventTypeDefs.h nor BrnGuiDemangledEventTypes.h
        // declares a 247 in the committed tree, so this TU carries a file-local wire view
        // named for the role the asm gives it -- the same accommodation BrnInGame.cpp and
        // BrnCarSelectMain_wG_02.cpp make for demangled-only shapes. The record's shape is
        // asm-attested; only the NAME is consumer-derived.
        //
        // The two header words are HOST expressions, not the console's baked immediates.
        typedef CgsGui::GuiEvent<247> PlayerHighlightedHeader;

        struct GuiEventPlayerHighlightedWire : public PlayerHighlightedHeader
        {
            s32 miPlayerID;   // +0x0C

            explicit GuiEventPlayerHighlightedWire(s32 liPlayerID)
                : PlayerHighlightedHeader(static_cast<u32>(sizeof(s32)),
                                          static_cast<u32>(sizeof(PlayerHighlightedHeader)))
                , miPlayerID(liPlayerID)
            {
            }
        };

        // ---- icon state indices -------------------------------------------------------
        // Each value is an index into the matching KAP_*_STATE_ID table declared on the
        // class (the state-id strings themselves are listed in
        // BrnOnlineGameRoomPlayerInfo.h and defined with their X360 rodata values in
        // BrnOnlineGameRoomPlayerInfo.cpp). Typed u32 so every SetState call binds
        // unambiguously to IconComponent::SetState(u32) rather than the const char*
        // sibling overload.
        const u32 KU_MARKED_MAN_VISIBLE        = 0;   // KAP_MARKED_MAN_STATE_ID[0]
        const u32 KU_MARKED_MAN_INVISIBLE      = 1;   //                        [1]
        const u32 KU_REVENGE_AHEAD             = 0;   // KAP_REVENGE_STATUS_STATE_ID[0]
        const u32 KU_REVENGE_EQUAL             = 1;   //                            [1]
        const u32 KU_REVENGE_BEHIND            = 2;   //                            [2]
        const u32 KU_REVENGE_INVISIBLE         = 3;   //                            [3]
        const u32 KU_READY_STATUS_INVISIBLE    = 4;   // KAP_READY_STATUS_STATE_ID[4]
        const u32 KU_VIDEO_STATUS_NO_CAMERA    = 0;   // KAP_VIDEO_STATUS_STATE_ID[0]
        const u32 KU_VIDEO_STATUS_INVISIBLE    = 4;   //                          [4]
        const u32 KU_VOIP_STATUS_NO_HEADSET    = 0;   // KAP_VOIP_STATUS_STATE_ID[0]
        const u32 KU_VOIP_STATUS_INVISIBLE     = 3;   //                         [3]
        const u32 KU_TEAM_STATUS_NONE          = 0;   // KAP_TEAM_STATUS_STATE_ID[0]
        const u32 KU_CONNECTION_TYPE_INVISIBLE = 2;   // KAP_CONNECTION_TYPE_STATE_ID[2]

        // The upper bound the VOIP-status assert compares against (`cmpwi r4, 3`). The
        // producing enum is CgsNetwork::ENetworkHeadsetPlayerStatus, which has no
        // committed home in the tree, so the X360-attested bound is spelled out here.
        const s32 KI_NETWORK_HEADSET_PLAYER_STATUS_COUNT = 3;

        // ---- player-option ids ---------------------------------------------------------
        // 64-bit SelectableGroup ids. The mapping is pinned by the sibling
        // ShowPlayerOptions @0x8248C4D0, whose stack table is indexed BY these ids:
        //   { "$ONLINE_PLAYER_OPTION_MARK_PLAYER" (or ..._UNMARK_PLAYER when the row is
        //     already a marked man), "$ONLINE_PLAYER_OPTION_SHOW_GAMERCARD_PLAYER",
        //     "$ONLINE_PLAYER_OPTION_SUBMIT_FEEDBACK_PLAYER",
        //     "$ONLINE_PLAYER_OPTION_KICK_PLAYER" }.
        const u64 KU64_PLAYER_OPTION_MARK_PLAYER     = 0;
        const u64 KU64_PLAYER_OPTION_SHOW_GAMERCARD  = 1;
        const u64 KU64_PLAYER_OPTION_SUBMIT_FEEDBACK = 2;
        const u64 KU64_PLAYER_OPTION_KICK_PLAYER     = 3;
    }

    // ================================================================================
    // ShowPlayerList @ 0x824A5968   (DWARF cpp:3371)
    //
    // Repaint the lobby player list and restore the highlight onto liSelectedPlayerID.
    // ================================================================================
    void OnlineGameRoomPlayerInfo::ShowPlayerList(s32 liSelectedPlayerID)
    {
        typedef BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData InGamePlayerStatusData;
        typedef BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData  LobbyPlayerStatusData;

        // X360 `lbzx this+39149 ; extsb ; cmpwi -1` -- the group's highlight cursor, with
        // "nothing highlighted" folded to row 0.
        s32 liHighlightedIndex = mPlayerNameComponent.miHighlightedIndex;
        if (liHighlightedIndex == -1)
        {
            liHighlightedIndex = 0;
        }

        // r5 == 0 -> lbWrap false.
        mPlayerNameComponent.SetupMenu(static_cast<s32>(mpGuiCache->muNumActivePlayers), false);

        // ---- the live rows -----------------------------------------------------------
        // The X360 re-loads muNumActivePlayers from the cache on every iteration (the
        // `lwzx r11, r31, 0xAC74` in the loop tail), so the bound is re-read here too.
        s32 liPlayerIndex = 0;
        for (; liPlayerIndex < static_cast<s32>(mpGuiCache->muNumActivePlayers); ++liPlayerIndex)
        {
            // GetOnlinePlayerInfo @0x8240F890, inlined by the X360 -- its two bounds
            // asserts survive verbatim (file "..\\GameSource\\Gui/BrnGuiCache.h",
            // lines 3645 / 3646).
            const InGamePlayerStatusData* lpPlayerInfo =
                mpGuiCache->GetOnlinePlayerInfo(liPlayerIndex);

            // The parallel lobby mirror row (cache +0xB640, 56-byte stride). No accessor
            // exists for it; the screen is a friend of GuiCache and reads the named
            // byte-storage member, exactly as GuiCache::GetOnlinePlayerInfo does for
            // maPlayerInfo.
            const LobbyPlayerStatusData* lpLobbyPlayerInfo =
                reinterpret_cast<const LobbyPlayerStatusData*>(
                    mpGuiCache->maLobbyPlayerInfo[liPlayerIndex]);

            Icons& lIcons = maIcons[liPlayerIndex];

            // Row label: the online player name at record +256.
            mPlayerNameComponent.SetText(liPlayerIndex, lpPlayerInfo->mPlayerName.macName);

            // Marked-man flag at record +300: shown when set.
            lIcons.mMarkedManIcon.SetState(lpPlayerInfo->mbMarkedMan ? KU_MARKED_MAN_VISIBLE
                                                                     : KU_MARKED_MAN_INVISIBLE);

            // Revenge standing: the live-revenge score at record +248
            // (mLiveRevengeRelationship +112 == miCurrentScoreForPlayersPointOfView). The
            // X360 loads that word inline; the field is PRIVATE on LiveRevengeRelationship
            // and this screen is not a friend of it, so the original went through the
            // public accessor -- GetCurrentScoreForLocalPlayer(), the same identification
            // the committed BrnNetworkAggressiveDrivingManager.cpp:587 already makes for a
            // matching inlined +112 read. The X360 computes the <= 0 half with the
            // `cntlzw(cntlzw(x)) >> 5` sign idiom; de-optimised back to the comparison.
            const s32 liRevengeDelta =
                lpPlayerInfo->mLiveRevengeRelationship.GetCurrentScoreForLocalPlayer();
            u32 luRevengeState;
            if (liRevengeDelta > 0)
            {
                luRevengeState = KU_REVENGE_AHEAD;
            }
            else if (liRevengeDelta == 0)
            {
                luRevengeState = KU_REVENGE_EQUAL;
            }
            else
            {
                luRevengeState = KU_REVENGE_BEHIND;
            }
            lIcons.mRevengeStatusIcon.SetState(luRevengeState);

            // Ready column (lobby record +20).
            CGS_ASSERT(lpLobbyPlayerInfo->meReadyStatus >= LobbyPlayerStatusData::E_READY_STATUS_HOST,
                       "lpLobbyPlayerInfo->meReadyStatus >= BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_HOST");   // cpp:3418
            CGS_ASSERT(lpLobbyPlayerInfo->meReadyStatus < LobbyPlayerStatusData::E_READY_STATUS_COUNT,
                       "lpLobbyPlayerInfo->meReadyStatus < BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_COUNT");   // cpp:3419
            lIcons.mReadyStatusIcon.SetState(static_cast<u32>(lpLobbyPlayerInfo->meReadyStatus));

            // Video/camera column (record +284). Out-of-range falls back to "no camera"
            // AFTER firing the streamed assert -- it does not skip the SetState.
            CGS_ASSERT(lpPlayerInfo->meCameraStatus >= BrnNetwork::E_CAMERA_STATUS_NONE,
                       "lpPlayerInfo->meCameraStatus >= BrnNetwork::E_CAMERA_STATUS_NONE");   // cpp:3424
            if (lpPlayerInfo->meCameraStatus < BrnNetwork::E_CAMERA_STATUS_COUNT)
            {
                lIcons.mVideoStatusIcon.SetState(static_cast<u32>(lpPlayerInfo->meCameraStatus));
            }
            else
            {
                // X360 streams the prefix then `<< meCameraStatus` into the assert buffer;
                // collapsed to the verbatim prefix per convention.
                CGS_ASSERT(false, "Ended up with bad meCameraStatus: ");   // cpp:3431
                lIcons.mVideoStatusIcon.SetState(KU_VIDEO_STATUS_NO_CAMERA);
            }

            // VOIP column (record +280); same shape as the camera column.
            CGS_ASSERT(lpPlayerInfo->meVOIPStatus >= 0,
                       "lpPlayerInfo->meVOIPStatus >= CgsNetwork::E_NETWORK_HEADSET_PLAYER_STATUS_NONE");   // cpp:3435
            if (lpPlayerInfo->meVOIPStatus < KI_NETWORK_HEADSET_PLAYER_STATUS_COUNT)
            {
                lIcons.mVoipStatusIcon.SetState(static_cast<u32>(lpPlayerInfo->meVOIPStatus));
            }
            else
            {
                CGS_ASSERT(false, "Ended up with bad mVoipStatusIcon: ");   // cpp:3442
                lIcons.mVoipStatusIcon.SetState(KU_VOIP_STATUS_NO_HEADSET);
            }

            // Team column (lobby record +24).
            lIcons.mTeamStatusIcon.SetState(static_cast<u32>(lpLobbyPlayerInfo->mePlayerTeam));

            // Connection columns (lobby record +28 / +32). The local player's own row
            // carries no connection classification, so both icons go invisible.
            if (lpLobbyPlayerInfo->mbLocalPlayer)
            {
                lIcons.mGameConnectionTypeIcon.SetState(KU_CONNECTION_TYPE_INVISIBLE);
                lIcons.mVoipConnectionTypeIcon.SetState(KU_CONNECTION_TYPE_INVISIBLE);
            }
            else
            {
                lIcons.mGameConnectionTypeIcon.SetState(
                    static_cast<u32>(lpLobbyPlayerInfo->meGameConnectionType));
                lIcons.mVoipConnectionTypeIcon.SetState(
                    static_cast<u32>(lpLobbyPlayerInfo->meVoipConnectionType));
            }
        }

        // ---- blank the unused rows -----------------------------------------------------
        // Continues from the same cursor: when the list is empty the X360 branches
        // straight in here with the index still 0.
        for (; liPlayerIndex < KI_MAX_PLAYERS; ++liPlayerIndex)
        {
            Icons& lIcons = maIcons[liPlayerIndex];

            lIcons.mMarkedManIcon.SetState(KU_MARKED_MAN_INVISIBLE);
            lIcons.mRevengeStatusIcon.SetState(KU_REVENGE_INVISIBLE);
            lIcons.mReadyStatusIcon.SetState(KU_READY_STATUS_INVISIBLE);
            lIcons.mVideoStatusIcon.SetState(KU_VIDEO_STATUS_INVISIBLE);
            lIcons.mVoipStatusIcon.SetState(KU_VOIP_STATUS_INVISIBLE);
            lIcons.mTeamStatusIcon.SetState(KU_TEAM_STATUS_NONE);
            lIcons.mGameConnectionTypeIcon.SetState(KU_CONNECTION_TYPE_INVISIBLE);
            lIcons.mVoipConnectionTypeIcon.SetState(KU_CONNECTION_TYPE_INVISIBLE);
        }

        // ---- restore the highlight -----------------------------------------------------
        // Search the lobby mirror for the requested player id. The count is loaded ONCE
        // for this loop (`lwzx r10` before it, reused as the bound).
        const s32 liNumActivePlayers = static_cast<s32>(mpGuiCache->muNumActivePlayers);
        s32 liFoundIndex = 0;
        for (; liFoundIndex < liNumActivePlayers; ++liFoundIndex)
        {
            const LobbyPlayerStatusData* lpLobbyPlayerInfo =
                reinterpret_cast<const LobbyPlayerStatusData*>(
                    mpGuiCache->maLobbyPlayerInfo[liFoundIndex]);

            if (lpLobbyPlayerInfo->mPlayerID == liSelectedPlayerID)
            {
                // Component vtable slot 12.
                mPlayerNameComponent.HighlightIndex(liFoundIndex);
                break;
            }
        }

        // The X360 re-reads the count here (it is NOT the cached bound above). The list is
        // re-highlighted when the requested player was not found (the cursor ran off the
        // end) or when no "new player" highlight has been published yet.
        const s32 liNumActivePlayersNow = static_cast<s32>(mpGuiCache->muNumActivePlayers);
        if (liNumActivePlayersNow > 0
            && (liFoundIndex == liNumActivePlayersNow || !mbHighlightedNewPlayer))
        {
            if (liHighlightedIndex >= liNumActivePlayersNow)
            {
                liHighlightedIndex = liNumActivePlayersNow - 1;
            }

            mPlayerNameComponent.HighlightIndex(liHighlightedIndex);
            HighlightNewPlayer();
        }
    }

    // ================================================================================
    // HighlightNewPlayer @ 0x824991E0   (DWARF cpp:3494)
    //
    // Publish the newly-highlighted row: announce its network player id, refresh the
    // stats panel from that player's online record, then show or hide the players-page
    // button prompts depending on whether the row has any selectable options at all.
    // ================================================================================
    void OnlineGameRoomPlayerInfo::HighlightNewPlayer()
    {
        typedef BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData InGamePlayerStatusData;
        typedef BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData  LobbyPlayerStatusData;

        const s32 liHighlightedIndex = mPlayerNameComponent.miHighlightedIndex;

        // The id comes from the LOBBY mirror row (cache +0xB640 + 56*index, mPlayerID at
        // row+16), not from the in-game record.
        const LobbyPlayerStatusData* lpLobbyPlayerInfo =
            reinterpret_cast<const LobbyPlayerStatusData*>(
                mpGuiCache->maLobbyPlayerInfo[liHighlightedIndex]);

        GuiEventPlayerHighlightedWire lPlayerHighlighted(lpLobbyPlayerInfo->mPlayerID);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lPlayerHighlighted),
            KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lPlayerHighlighted)));   // X360 record size 16

        mbHighlightedNewPlayer = true;

        // Stats panel: name from the in-game record (+256), no value / no stats block,
        // the row index as the player image index, and the world-rank column gated on the
        // cached game params' "ranked" flag.
        const InGamePlayerStatusData* lpPlayerInfo =
            mpGuiCache->GetOnlinePlayerInfo(liHighlightedIndex);

        mPlayerStatsDisplay.SetInfo(lpPlayerInfo->mPlayerName.macName,
                                    0,
                                    0,
                                    liHighlightedIndex,
                                    mpGuiCache->mbOnlineRanked,
                                    mpGuiCache);

        u64 lau64OptionIds[KI_MAX_PLAYER_OPTIONS];
        s32 liNumOptions;
        GetPlayerOptions(lau64OptionIds, &liNumOptions);

        mPlayersButtonPromptsAnimation.AddOutputAptViewState(
            KAC_APT_TRANSITION,
            liNumOptions != 0 ? KAPC_PLAYERS_BUTTON_PROMPT_ANIMATION_STATES[0]    // "visible"
                              : KAPC_PLAYERS_BUTTON_PROMPT_ANIMATION_STATES[1],   // "invisible"
            false);
    }

    // ================================================================================
    // GetPlayerOptions @ 0x82484CC0   (DWARF cpp:3263)
    //
    // Build the option-id list for the currently-highlighted player row. The ids are the
    // 64-bit SelectableGroup ids ShowPlayerOptions binds onto the option menu.
    // ================================================================================
    void OnlineGameRoomPlayerInfo::GetPlayerOptions(u64* lpau64OptionIds, s32* lpiNumOptions)
    {
        typedef BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData InGamePlayerStatusData;

        CGS_ASSERT(lpiNumOptions != 0, "lpiNumOptions");   // cpp:3279

        s32 liOptionIndex = 0;

        // The highlighted row's online record (mbIsLocalPlayer at +302).
        const InGamePlayerStatusData* lpPlayerInfo =
            mpGuiCache->GetOnlinePlayerInfo(mPlayerNameComponent.miHighlightedIndex);

        // Marking and feedback only apply to OTHER players.
        if (!lpPlayerInfo->mbIsLocalPlayer)
        {
            lpau64OptionIds[liOptionIndex++] = KU64_PLAYER_OPTION_MARK_PLAYER;
            lpau64OptionIds[liOptionIndex++] = KU64_PLAYER_OPTION_SUBMIT_FEEDBACK;
        }

        // The gamercard option is always offered (including on your own row).
        lpau64OptionIds[liOptionIndex++] = KU64_PLAYER_OPTION_SHOW_GAMERCARD;

        // Kicking needs host rights, and again only applies to other players. The X360
        // re-reads mpGuiCache and re-calls GetOnlinePlayerInfo here rather than reusing
        // the record above.
        if (mpGuiCache->mbIsOnlineHost)
        {
            if (!mpGuiCache->GetOnlinePlayerInfo(mPlayerNameComponent.miHighlightedIndex)
                     ->mbIsLocalPlayer)
            {
                lpau64OptionIds[liOptionIndex++] = KU64_PLAYER_OPTION_KICK_PLAYER;
            }
        }

        CGS_ASSERT(liOptionIndex <= KI_MAX_PLAYER_OPTIONS,
                   "liOptionIndex <= KI_MAX_PLAYER_OPTIONS");         // cpp:3302
        CGS_ASSERT(liOptionIndex <= KI_MAX_DISPLAYABLE_OPTIONS,
                   "liOptionIndex <= KI_MAX_DISPLAYABLE_OPTIONS");    // cpp:3303

        *lpiNumOptions = liOptionIndex;
    }
}
