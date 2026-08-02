// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 06: the request ladder and the permanent
// event pump.
//   HandleLeaderboardRequestState @0x8249F458  DWARF cpp:953  (assert cpp:1155)
//   UpdatePermanent               @0x824A96A0  DWARF cpp:590  (asserts 614/698/711/726/730)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (asm arbitrates; Hex-Rays is a
// hint only). Two corrections to the understand-phase spec, both read off the asm:
//   * event 122's status field is read with `lbz r11, 0x10(r29)` -- a ONE-BYTE flag at
//     payload+16, not an s32 word. On the big-endian console an s32 read would have been
//     `lwz`; testing a host s32 would test four bytes where the console tests one.
//   * event 119's "have we got a target?" test and event 122's name-clear are `lbz`/`stb` on
//     mCurrentTargetScorePlayerName -- byte-wide on the name's leading character, not the
//     leading word the spec described.
// Every record size below is a host sizeof of the record type; the console's 16 / 32
// immediates appear only in comments.
//
// HEADER-COLLISION NOTE (resolved during wave-I consolidation): this file was written while
// BrnGuiEventTypeDefs.h and BrnGuiDemangledEventTypes.h both defined GuiAudioTriggerEvent
// and GuiEventRunFsm, which made including both a hard C2011. The catalogue's two shadowing
// placeholders have since been removed in favour of the real hand-reconstructed homes, so
// that collision no longer exists. The file-local wire views below are kept as written --
// they are correct and self-contained -- but they are now an option, not a necessity.
//
// SUPERSEDED CROSS-TU NOTE: an earlier version of this banner argued that
// GuiTelemetryEvent should be { s32 miTelemetryType; char macParams[16]; } with NO
// GuiEvent<323> base, and that ids 111/112/113 were "correctly sized at 4 bytes". That
// reading measured the console PAYLOAD only. The conductor re-dumped the four
// OutputGuiEvent instantiation bodies (0x824940E8 / 0x82493FF8 / 0x82493D38 / 0x82493F88):
// each builds the record {payload_size, id, 12} followed by the payload and calls AddEvent
// with 12 + payload_size (0x20 / 0x10 / 0x1C / 0x30). Because the committed
// StateInterface::OutputGuiEvent passes sizeof(TEvent) and prepends NOTHING, the host type
// must CONTAIN the 12-byte header -- i.e. the GuiEvent<N> base. The catalogue shapes were
// corrected accordingly; do not "fix" them back.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::GuiEventTimeInfo (id 26)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / the in-queue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (pulls BrnGuiEventTypeDefs.h)
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::KI_USERNAME_LENGTH
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // the homed 111/112/113 + 323 wire records

#include <cstring>                                                        // std::memcpy

// BrnGuiDemangledEventTypes.h is deliberately NOT included: it hard-collides with
// BrnGuiEventTypeDefs.h (GuiAudioTriggerEvent / GuiEventRunFsm / the two road-rule records
// are defined in both), and BrnGuiCache.h above already puts this TU on the TypeDefs side.
// The four Demangled payload shapes needed here are carried as file-local wire views below.
// CgsGui::GuiEventNetworkSuspension lives in CgsGuiStateInterface.h and is safe either way.

namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        // X360 BrnNetwork::BrnNetworkModuleIO::TelemetryData::AddParameter -- appends one
        // (SPrintf'd) parameter string to the telemetry record a GUI event builds. The record
        // layout and the AddParameter body are owned by the network telemetry TU; modelled
        // here as an opaque record with the attested member, exactly as the committed
        // GameSource/Game/GameBridgeGUIToX.cpp:59 does. No fields invented.
        struct TelemetryData
        {
            void AddParameter(const char* lpcParameter);
        };
    }
}

namespace BrnGui
{
    namespace
    {
        // ---- the state in-queue (CgsGui::State::mpInGuiEventQueue's real instantiation) ---
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- AddEvent channels (the out-queue selector word) ---------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // X360 `li r5, 0x28`

        // ---- in-queue ids this pump acts on --------------------------------------------
        // The screen registers for {14, 21, 6, 44, 26, 64, 116..119, 122, 123}; the ids below
        // are the subset UpdatePermanent handles (6 goes to UpdateRunning, 14/21/64 to the
        // The six leaderboard reply ids are NOT anonymous: they are named in the recovered
        // DWARF and homed in BrnGuiDemangledEventTypes.h (116 category / 117 index /
        // 118 variation / 119 table / 122 target-score / 123 downloaded-challengeable).
        // An earlier draft of this banner claimed they were absent from the DWARF slice --
        // that was false; do not reinstate it.
        const s32 KI_EVENT_TIME_INFO                        = 26;
        const s32 KI_EVENT_NETWORK_DISCONNECTED             = 44;
        const s32 KI_EVENT_SCOREBOARD_CATEGORY_LIST         = 116;
        const s32 KI_EVENT_SCOREBOARD_INDEX_LIST            = 117;
        const s32 KI_EVENT_SCOREBOARD_VARIATION_LIST        = 118;
        const s32 KI_EVENT_SCOREBOARD_TABLE_DATA            = 119;
        const s32 KI_EVENT_SCOREBOARD_TARGET_SCORE_RESPONSE = 122;
        const s32 KI_EVENT_SCOREBOARD_DLD_CHALLENGEABLE     = 123;

        // ---- out-queue id --------------------------------------------------------------
        // The "send me the leaderboard category list" command (X360 `li r11, 0x6E`).
        const s32 KI_EVENT_SCOREBOARD_REQUEST_CATEGORIES = 110;

        // ---- telemetry -----------------------------------------------------------------
        // The telemetry record this screen raises when it asks for a table (X360 `li r11, 0x2B`).
        // FLAG: consumer-named -- the telemetry-type enum's home is not recovered.
        const s32 KI_TELEMETRY_TYPE_SCOREBOARD_TABLE = 43;
        // Scratch length the X360 int-parameter wrapper passes to SPrintf (`li r4, 0x10`).
        const u32 KU_TELEMETRY_PARAM_LENGTH = 16;

        // The loading-icon state identifier the request ladder pushes (aLoading_0).
        const char KAC_LOADING_ICON_STATE[] = "loading";

        // The two state-machine events the disconnect path raises on the owning flow.
        const char KAC_STATE_EVENT_GO_BACK[]      = "GO_BACK";
        const char KAC_STATE_EVENT_GO_BACK_EASY[] = "GO_BACK_EASY";

        // ---- out-queue wire records -----------------------------------------------------
        // The bare one-byte-payload command record { 1, N, 12, <payload byte> }, X360 record
        // size 16 (the wH_00 template, shared with the two pagers in group 01). The X360
        // stack-builds ONLY the three header words, so the payload byte keeps whatever the
        // reused stack slot held -- the constructor leaves it alone rather than inventing a
        // stored value.




        // ---- out-queue wire records -----------------------------------------------------
        // The bare one-byte-payload command record { 1, N, 12, <payload byte> }, X360 record
        // size 16 (the wH_00 template, shared with the two pagers in group 01). The X360
        // stack-builds ONLY the three header words, so the payload byte keeps whatever the
        // reused stack slot held -- the constructor leaves it alone rather than inventing a
        // stored value.
        template <s32 TI_EVENT_ID>
        struct GuiCommandWire16 : public CgsGui::GuiEvent<TI_EVENT_ID>
        {
            u8 mu8Payload;   // +0x0C (not written by the X360)

            GuiCommandWire16()
                : CgsGui::GuiEvent<TI_EVENT_ID>(
                      static_cast<u32>(sizeof(u8)),
                      static_cast<u32>(sizeof(CgsGui::GuiEvent<TI_EVENT_ID>)))
            {
            }
        };

        // ---- in-queue payload views ------------------------------------------------------
        // The queue hands back HEADER-STRIPPED payloads, so these views start at the payload's
        // first field. Each group redeclares only the ones it reads.

        // ev 26 -- the per-frame tick -- needs no view here: its payload has a committed home,
        // CgsGui::GuiEventTimeInfo (CgsGuiEventTypeDefs.h, static_assert sizeof == 8). The X360
        // reads the frame delta with `lfs f13, 0(r29)` @0x824A9994, which is that type's
        // mfTimeDelta at +0x00, i.e. exactly what GetTimeStep() returns.

        // ev 122 -- the reply to a "who is the score target?" request. X360 memcpy's 16 bytes
        // from +0 and tests +0x10 with `lbz`, so the status is a single byte, not a word.
        // FLAG: consumer-named. Nonzero means no target came back.
        struct TargetScoreResponsePayload
        {
            char macPlayerName[CgsNetwork::KI_USERNAME_LENGTH];   // +0x00
            u8   mu8Status;                                       // +0x10
        };

        // ev 123 -- "this player is challengeable". Only the name is read (memcpy 16 from +0).
        struct DldChallengeablePayload
        {
            char macPlayerName[CgsNetwork::KI_USERNAME_LENGTH];   // +0x00
        };

        // X360 sub_82354200 -- the out-of-line "append one integer telemetry parameter"
        // wrapper: SPrintf "%i" into a 16-byte scratch, then TelemetryData::AddParameter.
        // (The "%i" comes from the raw asm's aI operand, not the pseudocode.) It has no
        // recovered home of its own, so it is carried file-locally; the committed
        // GameBridgeGUIToX.cpp case-286 site spells the same two calls out inline.
        void AddIntTelemetryParameter(BrnNetwork::BrnNetworkModuleIO::TelemetryData* lpTelemetry,
                                      s32 liValue)
        {
            char lacParameter[KU_TELEMETRY_PARAM_LENGTH];
            CgsCore::SPrintf(lacParameter, KU_TELEMETRY_PARAM_LENGTH, "%i", liValue);
            lpTelemetry->AddParameter(lacParameter);
        }
    }

    // ================================================================================
    //  HandleLeaderboardRequestState  @ 0x8249F458  (cpp:953)
    //
    //  The leaderboard request ladder. Each GET_* arm fires one request at the network
    //  back end, resets the table to its loading look, and drops through into the matching
    //  WF_* arm, which is nothing but the "now wait for the reply" store. The replies land
    //  in UpdatePermanent, which steps the ladder on and calls back in here.
    // ================================================================================
    void OnlineScoreboards::HandleLeaderboardRequestState()
    {
        switch (meLeaderboardRequestState)
        {
            case E_LEADERBOARD_REQUEST_STATE_GET_CATEGORIES:
            {
                GuiCommandWire16<KI_EVENT_SCOREBOARD_REQUEST_CATEGORIES> lCategoriesRequest;
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lCategoriesRequest),
                    KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lCategoriesRequest)));   // X360 record size 16

                mTable.SetupScoreboard(0);
                // SetLoading(true) inlined, DWARF cpp:1362
                if (mbCurrentlyLoading != true)
                {
                    mbCurrentlyLoading = true;
                    mLoadingIcon.SetState(KAC_LOADING_ICON_STATE);
                }
                ShowTableEmpty(false);
            }
            // X360 falls straight through into the WF_CATEGORIES arm, which is the shared
            // "wait for the reply" store -- mirrored here rather than duplicated.
            // fall through
            case E_LEADERBOARD_REQUEST_STATE_WF_CATEGORIES:
                meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_WF_CATEGORIES;
                break;

            case E_LEADERBOARD_REQUEST_STATE_GET_INDEX:
            {
                GuiEventScoreboardRequestIndexEvent lIndexRequest(miCurrentCategory);
                mpStateInterface->OutputGuiEvent(lIndexRequest);

                mTable.SetupScoreboard(0);
                // SetLoading(true) inlined, DWARF cpp:1362
                if (mbCurrentlyLoading != true)
                {
                    mbCurrentlyLoading = true;
                    mLoadingIcon.SetState(KAC_LOADING_ICON_STATE);
                }
                ShowTableEmpty(false);
            }
            // fall through
            case E_LEADERBOARD_REQUEST_STATE_WF_INDEX:
                meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_WF_INDEX;
                break;

            case E_LEADERBOARD_REQUEST_STATE_GET_VARIATIONS:
            {
                GuiEventScoreboardRequestVariationEvent lVariationRequest(miCurrentIndex);
                mpStateInterface->OutputGuiEvent(lVariationRequest);

                mTable.SetupScoreboard(0);
                // SetLoading(true) inlined, DWARF cpp:1362
                if (mbCurrentlyLoading != true)
                {
                    mbCurrentlyLoading = true;
                    mLoadingIcon.SetState(KAC_LOADING_ICON_STATE);
                }
                ShowTableEmpty(false);
            }
            // fall through
            case E_LEADERBOARD_REQUEST_STATE_WF_VARIATIONS:
                meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_WF_VARIATIONS;
                break;

            case E_LEADERBOARD_REQUEST_STATE_GET_TABLE:
            {
                // A per-road board lists its roads alphabetically, so the row the player is
                // sitting on has to be mapped back through the sort order before the back end
                // is asked for it; every other board indexes its variations directly.
                const s32 liTargetVariation = mbPerRoadLeaderboard
                    ? maiAlphabeticalRoadIndex[miCurrentVariation]
                    : miCurrentVariation;

                GuiEventScoreboardRequestTableEvent lTableRequest(liTargetVariation);
                mpStateInterface->OutputGuiEvent(lTableRequest);

                // ...and log which board was asked for. The telemetry record's parameter
                // buffer starts one word past its type, and TelemetryData is the accessor for
                // that buffer, so the builder addresses the record through it.
                GuiTelemetryEvent lTelemetry(KI_TELEMETRY_TYPE_SCOREBOARD_TABLE);
                BrnNetwork::BrnNetworkModuleIO::TelemetryData* lpTelemetry =
                    reinterpret_cast<BrnNetwork::BrnNetworkModuleIO::TelemetryData*>(
                        &lTelemetry.miTelemetryType);

                AddIntTelemetryParameter(lpTelemetry, miCurrentCategory);
                AddIntTelemetryParameter(lpTelemetry, miCurrentIndex);
                AddIntTelemetryParameter(lpTelemetry, liTargetVariation);

                mpStateInterface->OutputGuiEvent(lTelemetry);
            }
            // fall through
            case E_LEADERBOARD_REQUEST_STATE_WF_TABLE:
                meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_WF_TABLE;
                break;

            case E_LEADERBOARD_REQUEST_STATE_DONE:
                // Everything has arrived; the ladder idles here until an input restarts it.
                break;

            default:
                // The X360 streams the offending state value into the message
                // ("invalid request state : " << meLeaderboardRequestState << "\n"); project
                // policy lowers a streamed assert to its static text. Non-fatal.
                CGS_ASSERT(false, "invalid request state : ");
                break;
        }
    }

    // ================================================================================
    //  UpdatePermanent  @ 0x824A96A0  (cpp:590)
    //
    //  The always-on event pump: it runs in every sub-state, drains nothing (the owning
    //  Update() clears the in-queue once, after this and UpdateRunning have both walked
    //  it), and finishes by stepping the request ladder.
    // ================================================================================
    void OnlineScoreboards::UpdatePermanent()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
                case KI_EVENT_TIME_INFO:
                    // The table is asked for a short beat after the variation list arrives, so
                    // a player scrolling the filters does not fire a request per row.
                    if (mfCurrentTableWaitTime > 0.0f)
                    {
                        mfCurrentTableWaitTime -=
                            reinterpret_cast<const CgsGui::GuiEventTimeInfo*>(lpEvent)
                                ->GetTimeStep();

                        // PPC NaN polarity: the X360 skips the store on `bgt` alone, so an
                        // unordered compare falls INTO it. `<= 0.0f` is false for a NaN and
                        // would invert that path, hence the negated ordered predicate.
                        if (!(mfCurrentTableWaitTime > 0.0f))
                        {
                            meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_GET_TABLE;
                        }
                    }
                    break;

                case KI_EVENT_NETWORK_DISCONNECTED:
                    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

                    // Latch the error the disconnect popup will show. The committed GuiCache
                    // inline IS the X360's store here (the event's leading word, or 0 when no
                    // event rode along).
                    mpGuiCache->SetDoDisconnectPopup(lpEvent);

                    if (mpGuiCache->IsOnlineStartPending())
                    {
                        // A session start was already in flight, so let the network layer run
                        // again on the way out and take the quiet exit.
                        CgsGui::GuiEventNetworkSuspension lResumeNetwork(false);
                        mpStateInterface->GetOutputEventQueue()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lResumeNetwork),
                            KI_CHANNEL_GUI_OUT,
                            static_cast<s32>(sizeof(lResumeNetwork)));   // X360 record size 16

                        mpGuiCache->SetOnlineStartPending(false);
                        SendStateEvent(KAC_STATE_EVENT_GO_BACK_EASY);
                    }
                    else
                    {
                        SendStateEvent(KAC_STATE_EVENT_GO_BACK);
                    }
                    break;

                case KI_EVENT_SCOREBOARD_CATEGORY_LIST:
                    HandleCategoryList(lpEvent);
                    meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_GET_INDEX;
                    break;

                case KI_EVENT_SCOREBOARD_INDEX_LIST:
                    HandleIndexList(lpEvent);
                    meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_GET_VARIATIONS;
                    break;

                case KI_EVENT_SCOREBOARD_VARIATION_LIST:
                    HandleVariationList(lpEvent);

                    // The variation list is the last thing the filters need, so the table
                    // request is armed on a timer and the table goes back to its loading look.
                    mfCurrentTableWaitTime = KF_DELAY_TO_REQUEST_TABLE;
                    mTable.SetupScoreboard(0);
                    // SetLoading(true) inlined, DWARF cpp:1362
                    if (mbCurrentlyLoading != true)
                    {
                        mbCurrentlyLoading = true;
                        mLoadingIcon.SetState(KAC_LOADING_ICON_STATE);
                    }
                    // The X360 inlines ShowTableEmpty's body here; the method is its home.
                    ShowTableEmpty(false);
                    break;

                case KI_EVENT_SCOREBOARD_TABLE_DATA:
                    HandleTableData(lpEvent);
                    meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_DONE;

                    // Re-apply the highlighted target to the freshly filled table. X360 `lbz`
                    // on the name -- the "is it empty?" test is its leading character.
                    if (mCurrentTargetScorePlayerName.macName[0] != 0)
                    {
                        mTable.SetTargetGamertag(mCurrentTargetScorePlayerName.macName);
                    }
                    break;

                case KI_EVENT_SCOREBOARD_TARGET_SCORE_RESPONSE:
                    // Unsolicited replies are ignored; only the screen that asked cares.
                    if (mbWaitingForTargetScoreResponse)
                    {
                        CGS_ASSERT(lpEvent != 0, "lpTargetScoreResponse");

                        const TargetScoreResponsePayload* lpTargetScoreResponse =
                            reinterpret_cast<const TargetScoreResponsePayload*>(lpEvent);

                        if (lpTargetScoreResponse->mu8Status != 0)
                        {
                            // No target came back -- empty the cached name (X360 `stb` of 0
                            // onto its leading character).
                            mCurrentTargetScorePlayerName.macName[0] = 0;
                        }
                        else
                        {
                            std::memcpy(mCurrentTargetScorePlayerName.macName,
                                        lpTargetScoreResponse->macPlayerName,
                                        sizeof(mCurrentTargetScorePlayerName.macName));
                            CGS_ASSERT(mCurrentTargetScorePlayerName.macName[0] != 0,
                                       "!mCurrentTargetScorePlayerName.IsEmpty()");
                        }

                        mTable.SetTargetGamertag(mCurrentTargetScorePlayerName.macName);
                    }
                    break;

                case KI_EVENT_SCOREBOARD_DLD_CHALLENGEABLE:
                {
                    CGS_ASSERT(lpEvent != 0, "lpDldChallengeableEvent");

                    const DldChallengeablePayload* lpDldChallengeable =
                        reinterpret_cast<const DldChallengeablePayload*>(lpEvent);

                    std::memcpy(mCurrentTargetScorePlayerName.macName,
                                lpDldChallengeable->macPlayerName,
                                sizeof(mCurrentTargetScorePlayerName.macName));
                    CGS_ASSERT(mCurrentTargetScorePlayerName.macName[0] != 0,
                               "!mCurrentTargetScorePlayerName.IsEmpty()");

                    mTable.SetTargetGamertag(mCurrentTargetScorePlayerName.macName);
                    break;
                }

                default:
                    break;
            }
        }

        // Whatever the replies changed, the ladder gets its turn at the end of the pass.
        HandleLeaderboardRequestState();
    }
}
