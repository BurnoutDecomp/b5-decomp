// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 08: the frame pump and the icon-hover resolver.
//   Update            @0x824DD6D8  (the state machine's per-frame virtual)
//   UpdateIconManager @0x824CBA70  (cpp:1119 region)
//
// Both bodies are read store-for-store off the raw X360 assembly
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824DD6D8.json and /0x824CBA70.json, dumped to
// scratchpad/waveJ/asm_g08_update.txt and asm_g08_uim.txt), with Hex-Rays arbitrated
// against the asm everywhere the two disagree.
//
// RECONCILIATION PASS (2026-08-03). The wave-J shared-header requests this group filed
// have since been applied by the header owner, so the two bodies are landed here and the
// names are spelled the way the committed headers now spell them. What changed against
// the parked drafts:
//
//   * GuiCursor::IsAlwaysSnap()  ->  GuiCursor::GetAlwaysSnap()
//     The request had invented the `Is` spelling. DWARF
//     references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Screen/Components/BrnCursor.h:348
//     declares `bool GetAlwaysSnap();` (non-const) and that is what
//     b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnCursor.h:201 now carries.
//     GuiCursor::GetPosition() likewise returns Vector2 BY VALUE (DWARF h:334), which is
//     what MapTransform::DeviceToWorld(Vector2) takes, so that call site is unchanged.
//
//   * GuiEventUpdateSatNav::SatNavI(...)  ->  SatNavIconInfo::GetIconType()
//     `SatNavI` was IDA's TRUNCATED symbol for 0x823A6B30, not a function. That body is
//     `lbz r11, 0x28(r3); extsb r31, r11` guarded by the two CGS_ASSERTs from
//     "..\..\..\GameSource\Gui/BrnGuiEventTypeDefs.h" (`li r5,0x777`/`0x778`), i.e. it
//     reads +0x28 of ITS ARGUMENT -- and at 0x824CBEFC / 0x824CBFC0 that argument is the
//     SatNavIconInfo* returned by GetDriveThroughOrJunkyardAtIndex, not the event. DWARF
//     BrnGuiEventTypeDefs.h:1731 names it `SatNavIconType GetIconType() const;` and the
//     committed BrnGuiEventTypeDefs.h:199 already declares it. Both drive-through arms
//     now read the icon's own type, with no cast.
//
// STILL BLOCKED -- ONE STATEMENT, TWO MISSING DECLARATIONS (UpdateIconManager, below).
// Measured, not guessed, from 0x824CBAC4..0x824CBAEC:
//
//     lwz   r10, 0x4C(r31)          ; r10 = mpIconManager
//     lfs   f13, 0x6C0(r31)         ; CrashNavMap+0x6C0 == mMainMapComponent(+96) + 1632
//     ori   r9,  r11, 0xA198        ; r9  = 0xA198
//     lfs   f0,  flt_82F259E0       ; 3500.0f  (scratchpad/waveJ/crashnav_consts.txt)
//     fdivs f0,  f0, f13
//     stfsx f0,  r10, r9            ; mpIconManager + 0xA198 <- 3500.0f / zoom
//
//   (a) The DESTINATION, mpIconManager + 0xA198, has no member in the committed
//       b5-decomp/src/GameSource/Gui/SatNav/BrnMapIconManager.h -- it falls in the
//       "[further selection/flag state: not modelled here]" gap between mRoadSignIconManager
//       (+0x7090) and mpGuiCache (+0xA9F8). It has NO DWARF row either: the DecFIGS
//       MapIconManager has no float member at all, and a repo-wide grep of
//       .ida-exports/BURNOUT_X360_ARTIST.XEX for the immediate 0xA198 matches exactly ONE
//       function -- this one -- so the field has no other writer and no reader I can point
//       at to name it. It therefore stays consumer-named and UNLANDED rather than invented.
//   (b) The SOURCE, MainMapComponent::mfWorldZoomScaleFactor (DWARF BrnMainMap.h:220,
//       X360 comp+1632), is `private` in the committed BrnMainMap.h with no accessor and
//       no `friend struct CrashNavMap`. The DWARF has no getter for it either.
//
// So this one store needs a header edit that is not mine to make (the parallel case,
// MapIconManager's flag tail, was solved by `friend struct CrashNavMap` + named members --
// BrnMapIconManager.h:233). The statement is kept in place, spelled the way it would be
// once those two declarations exist, and this partfile is reported as still_blocked
// rather than having the store deleted or faked. Everything else in both bodies was
// compile-verified against the committed headers with that single statement elided
// (scratchpad/waveJ/probe_CrashNavMap_8_recon/) -- zero further diagnostics.
//
// LINK-TIME EXTERNALS (cl /c cannot see these; reported, not fabricated):
//   LobbyNameCmp @0x82B10050, MapIconManager::{UpdateSatNavInfo, SetIconsVisible, Update,
//   GetSatNavIconPositions, GetRivalIconAtIndex, GetRoadSignNameAtIndex, GetEventIDAtIndex,
//   GetDriveThroughAndJunkyardCount, GetDriveThroughOrJunkyardAtIndex, SetRoadRuleBatchData},
//   CrashNavPanel::{Update, RecEvent, ShowBlank, SetEventPanelData}, GuiCursor::
//   {UpdateToSnapLocations, SetPosition}, MainMapComponent::{RecvEvent, IsZooming},
//   MapTransform::{WorldToDevice, DeviceToWorld}, SatNavIconInfo::{SetIconType, GetIconType},
//   GuiCache::{AreAllAptComponentsInitialised, GetWorldCameraPosition, GetOnlinePlayerInfo,
//   GetProfileEventDisplayInfo}.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT / Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface (out-queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / VariableEventQueue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventRoadRule/ChallengedEventDataRequest
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiFlow / GuiEventUpdateSatNav / GuiEventEnableSatNavIcons
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"  // InGamePlayerStatusData
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                         // BrnGui::MapTransform

#include <cstring>   // std::memcpy (the id-332 record copy + the u64 wire payloads)

// LobbyNameCmp @0x82B10050 -- the DirtySock collation-table string compare (it skips
// characters whose collation class is 1). No header in the tree declares it and no body
// exists here, so it is declared per-cpp exactly as
// GameSource/.../CgsServerInterfaceGames.cpp:47 does. LINK-TIME EXTERNAL: reported, not
// fabricated.
extern "C" s32 LobbyNameCmp(const char* lpcA, const char* lpcB);

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ------------------------
        // Measured from the three template instantiations this TU calls:
        //   OutputGuiEvent<GuiEventRoadRuleDataRequest>        @0x82476EE8  `li r5,0x28`
        //   OutputGuiEvent<GuiEventChallengedEventDataRequest> @0x824C2F50  `li r5,0x28`
        //   OutputViewState<GuiEventSetHoveredEventIcon>       @0x824C2EE8  `li r5,0x29`
        const s32 KI_CHANNEL_GUI_EVENT  = 40;   // GuiEventOut
        const s32 KI_CHANNEL_VIEW_STATE = 41;   // GuiOutViewState

        // ---- the in-queue wire ids Update dispatches on -----------------------------
        // (the jump-table base is `addi r11, r28, -6` @0x824DD774, and the three
        //  above-199 ids come from the `cmpwi` chain at 0x824DD9A8..0x824DD9BC)
        const s32 KI_EVENT_CONTROLLER_INPUT_PRESSED       = 6;
        const s32 KI_EVENT_CONTROLLER_INPUT_RELEASED      = 7;
        const s32 KI_EVENT_CONTROLLER_AXIS                = 8;
        const s32 KI_EVENT_GUI_CACHE                      = 64;
        const s32 KI_EVENT_UPDATE_SATNAV                  = 199;   // 0xC7
        const s32 KI_EVENT_CHALLENGED_EVENT_DATA_RESPONSE = 332;   // 0x14C
        const s32 KI_EVENT_ROAD_RULE_DATA                 = 334;   // 0x14E
        const s32 KI_EVENT_ROAD_RULE_BATCH_DATA_RESPONSE  = 344;   // 0x158

        // The wire id the screen posts back once it has accepted the GuiCache
        // (Update's id-64 arm; `li r19, 0x148` @0x824DD764).
        const s32 KI_EVENT_CRASHNAV_CACHE_ACCEPTED = 328;

        // The wire id of the hover-set publication (`li r11, 0x22F` inside
        // OutputViewState<GuiEventSetHoveredEventIcon> @0x824C2F34).
        const s32 KI_EVENT_SET_HOVERED_EVENT_ICON = 559;

        // The icons are authored against this reference world scale; UpdateIconManager
        // divides it by the map's live zoom factor. X360 flt_82F259E0, measured 3500.0f
        // (scratchpad/waveJ/crashnav_consts.txt) -- UpdateIconManager is its only reader.
        const f32 KF_EVENT_ICON_REFERENCE_SCALE = 3500.0f;

        // The online roster the two active-race-car-index searches walk
        // (X360 `cmpwi r11, 8` at 0x824CBCFC and 0x824CBDD8; GuiCache::maPlayerInfo[8]).
        const s32 KI_MAX_ONLINE_PLAYERS = 8;

        // Capacity of UpdateIconManager's stack snap-location buffer. NOT a recovered
        // source constant -- the X360 encodes no immediate for it. DERIVED: the buffer
        // starts at sp+0x90 inside a 0x12E0-byte frame and the lanes are 16 bytes, so
        // (0x12E0 - 0x90) / 16 == 293 is the array's upper bound. Flagged rather than
        // rounded to a plausible-looking number.
        const s32 KI_MAX_SNAP_LOCATIONS = 293;

        // The X360 assert-site file string, verbatim (aGamesourceGuiF_63).
        const char KAC_ASSERT_FILE[] =
            "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp";

        // The state input queue: CgsGui::State::mpInGuiEventQueue is declared as an
        // opaque InputBuffer::GuiEventQueue*, and the X360 calls
        // VariableEventQueue<18432,16>::GetFirstEvent/GetNextEvent on it. Same typedef +
        // reinterpret_cast as every other committed GUI state (BrnBootAttract.cpp:15,
        // BrnCarSelectVehicle.cpp:54, ...).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- in-queue payload views -------------------------------------------------
        // The queue hands the state the HEADER-STRIPPED payload, so a typed event pointer
        // would mislead by 12 bytes; these are the wH_00 / BrnInGame.cpp idiom.

        // id 64: a bare GuiCache pointer (X360 `lwz r11, 0(r29)` -- a console u32, a real
        // pointer on the host).
        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpGuiCache;   // +0x00
        };

        // id 332 (GuiEventChallengedEventDataResponse). Offsets measured at
        // 0x824DDA48..0x824DDA6C: the leading field is read with a 64-bit `ld` and
        // compared with `cmpld` against the zero-extended muHoveredEventID, so it is one
        // 8-byte id and NOT two words (Hex-Rays' `v5[1]` is the low half only). The
        // handler then lifts the +0x08 word and memcpy's the sixteen bytes at +0x0C.
        // FLAG consumer-named: only the leading id and the +0x08 score word have a
        // recovered role -- see BrnCrashNavMap_08_Update.cpp's banner.
        struct GuiEventChallengedEventDataResponsePayload : public CgsModule::Event
        {
            u64 mu64EventID;          // +0x00  (`ld` / `cmpld`)
            u32 muScoreOverride;      // +0x08  (`lwz`, becomes the panel record's word 0)
            u8  maReserved_0C[16];    // +0x0C..+0x1B (`memcpy` 16 into the panel record)
        };

        // ---- out-queue payload views ------------------------------------------------

        // Wire id 328, posted by Update's id-64 arm. The X360 builds { 1, 328, 12 } and
        // AddEvent's 16 bytes on channel 40 -- and NEVER stores the payload byte itself
        // (there is no `stb` to the record's +12 anywhere in the function), so on the
        // console it ships whatever was on the stack. Modelled zeroed. The size word is
        // sizeof(payload) == 1, NOT 4.
        struct GuiEventCrashNavCacheAcceptedPayload
        {
            bool mbFlag;   // +0x00  never written by the X360; role unrecovered

            GuiEventCrashNavCacheAcceptedPayload() : mbFlag(false) {}
            s32 GetEventType() const { return KI_EVENT_CRASHNAV_CACHE_ACCEPTED; }
        };

        // Wire id 559, posted by UpdateIconManager (both exits). Layout measured from the
        // OutputViewState<GuiEventSetHoveredEventIcon> body @0x824C2EE8 (three `ld`/`std`
        // pairs = 24 payload bytes, record offset word 0x10) cross-checked against the two
        // build sites (0x824CBB5C.. and 0x824CC094..), which fill +0x00 / +0x08 / +0x10 /
        // +0x14 from mHoveredDriveThruID / mHoveringRivalId / muHoveredEventID /
        // mpLockedIconName.
        //
        // NOT the homed BrnGui::GuiEventSetHoveredEventIcon: that catalogue entry models
        // the type as GuiEvent<559> + maPayload[12], i.e. only twelve bytes of actual
        // payload, which cannot cover the measured +0x00..+0x17 writes. Flagged for a
        // future catalogue sweep; kept file-local here rather than widening a shared catalogue
        // entry on one consumer's evidence.
        //
        // HOST WIDTH: the fourth field is a char POINTER. It is a 4-byte word on the X360
        // (payload 24, record 40) and 8 bytes on the x64 host (payload 32, record 48).
        // Never write those console numbers as literals -- the poster takes sizeof().
        struct alignas(8) GuiEventSetHoveredEventIconPayload
        {
            CgsID       mHoveredDriveThruId;   // +0x00
            CgsID       mHoveringRivalId;      // +0x08
            u32         muHoveredEventId;      // +0x10
            const char* mpcLockedIconName;     // +0x14 on X360; +0x18 on the host
            s32 GetEventType() const { return KI_EVENT_SET_HOVERED_EVENT_ICON; }
        };

        // ---- lane conversions -------------------------------------------------------
        // The X360 moves whole 16-byte lanes between the icon records, MapTransform and
        // the cursor with lvx128/stvx128, so all four floats travel. Vector2/Vector3/
        // Vector4 are distinct PODs on the host, hence these lane-for-lane copies. They
        // are transliteration plumbing, not recovered functions.
        inline Vector3 LaneAsVector3(const Vector4& lv4Lane)
        {
            const Vector3 lv3Lane = { lv4Lane.x, lv4Lane.y, lv4Lane.z, lv4Lane.w };
            return lv3Lane;
        }

        inline Vector4 LaneAsVector4(const Vector3& lv3Lane)
        {
            const Vector4 lv4Lane = { lv3Lane.x, lv3Lane.y, lv3Lane.z, lv3Lane.w };
            return lv4Lane;
        }

        // ---- shared helpers ---------------------------------------------------------

        // Publish the current hover set to the view. The X360 stack-builds the
        // OutputViewState wrapper record and calls
        // VariableEventQueue<65536,16>::AddEvent(iface + 0xC, &record, 41, sizeof(record))
        // -- which is exactly what GuiEventWrapper<T, 41> + GetOutputEventQueue()->AddEvent
        // produces (the wave-H BrnOnlineGameRoomPlayerInfo_wH_09.cpp precedent). Written
        // as a helper because UpdateIconManager emits the identical sequence twice.
        //
        // (CgsGui::StateInterface declares OutputGuiEvent<T> but not OutputViewState<T>;
        //  the missing member is a pre-existing tree gap -- CgsGuiStateInterface_
        //  OutputViewState_Inst.cpp names a member that does not exist -- and is reported,
        //  not worked around here.)
        void PostSetHoveredEventIcon(CgsGui::StateInterface* lpStateInterface,
                                     CgsID       lHoveredDriveThruId,
                                     CgsID       lHoveringRivalId,
                                     u32         luHoveredEventId,
                                     const char* lpcLockedIconName)
        {
            GuiEventSetHoveredEventIconPayload lPayload;
            lPayload.mHoveredDriveThruId = lHoveredDriveThruId;
            lPayload.mHoveringRivalId    = lHoveringRivalId;
            lPayload.muHoveredEventId    = luHoveredEventId;
            lPayload.mpcLockedIconName   = lpcLockedIconName;

            CgsGui::GuiEventWrapper<GuiEventSetHoveredEventIconPayload, KI_CHANNEL_VIEW_STATE>
                lRecord(lPayload);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord),
                lRecord.GetChannel(),
                static_cast<s32>(sizeof(lRecord)));
        }

        // Walk the cache's eight online-player records looking for the one driving the
        // given active-race-car index. The X360 inlines both the walk and GuiCache::
        // GetOnlinePlayerInfo: it strides maPlayerInfo by 312 (`addi r10,r10,0x138`) and
        // compares the index word at record+276 WITHOUT touching the record pointer
        // (0x824CBCE8..0x824CBD00), then, on the first match, re-derives &maPlayerInfo[i]
        // and null-checks THAT once (0x824CBD0C..0x824CBD20). Both failure exits -- no
        // match, and match-with-null-record -- run the same `stb 0, macName` code.
        //
        // KNOWN DIVERGENCE, one unreachable-in-practice case: the committed accessor is
        // the only handle on a record, so the index compare here has to go through a
        // non-null pointer, which makes a null record CONTINUE the search where the X360
        // ENDS it. The two differ only if two slots carry the same active-race-car index
        // and the first one's record is null. Left as-is rather than reached around,
        // because GuiCache exposes no per-slot index accessor to compare against.
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
        FindOnlinePlayerByActiveRaceCarIndex(const GuiCache* lpGuiCache,
                                             s32 liActiveRaceCarIndex)
        {
            for (s32 liSlot = 0; liSlot < KI_MAX_ONLINE_PLAYERS; ++liSlot)
            {
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                    lpGuiCache->GetOnlinePlayerInfo(liSlot);

                if (lpPlayerInfo != 0 &&
                    static_cast<s32>(lpPlayerInfo->meActiveRaceCarIndex) == liActiveRaceCarIndex)
                {
                    return lpPlayerInfo;
                }
            }
            return 0;
        }
    }

    // ================================================================================
    //  Update  @ 0x824DD6D8  (the state machine's per-frame virtual)
    //
    //  The screen's per-frame pump: reset the shared icon manager's used-icon count,
    //  drain the state in-queue (controller input, the map cursor axis, the GuiCache
    //  hand-off, sat-nav icon refreshes, road-rule and challenged-event responses),
    //  giving the main map and the crash-nav panel a look at every event on the way
    //  past, then run the map itself and finish the one-shot component set-up as soon
    //  as the cache reports every expected apt component initialised.
    // ================================================================================
    void CrashNavMap::Update()
    {
        // X360 `stw r10(=0), 0x990(r11)` -- the count is rebuilt from scratch by
        // UpdateIconManager later in the frame.
        if (mpIconManager != 0)
            mpIconManager->miNumUsedIcons = 0;

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT_PRESSED:
                // vtable +0x24 -- the derived screens' input maps.
                HandleCrashNavInputPressed(lpEvent);
                break;

            case KI_EVENT_CONTROLLER_INPUT_RELEASED:
                // vtable +0x28.
                HandleCrashNavInputReleased(lpEvent);
                break;

            case KI_EVENT_CONTROLLER_AXIS:
                // The cursor cannot move before the cache has arrived (MoveCursor
                // dereferences it), so the X360 gates the call on mpGuiCache.
                if (mpGuiCache != 0)
                    MoveCursor(lpEvent);
                break;

            case KI_EVENT_GUI_CACHE:
                {
                    // The in-queue hands the state the HEADER-STRIPPED payload; id 64's
                    // payload is a bare GuiCache pointer (X360 `lwz r11, 0(r29)` -- a
                    // console u32, a real pointer on the host).
                    const GuiCachePayload* lpPayload =
                        static_cast<const GuiCachePayload*>(lpEvent);

                    if (lpPayload->mpGuiCache == 0)
                    {
                        // Streamed diagnostic; the message names GenericHudState::Update
                        // (copy-paste in the original source) -- kept verbatim. Non-fatal.
                        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                        CgsDev::StrStream lStrStream(lacMessageBuffer,
                                                     CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                        lStrStream << "Invalid cache in GenericHudState::Update";
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(),
                                                   KAC_ASSERT_FILE, 330);
                        CgsDev::Assert::EndAssert();
                    }

                    // Only the FIRST cache event is acted on: once mpGuiCache is latched
                    // the whole arm is skipped (`lwz r11,0x48` / `bne` @0x824DD918).
                    if (mpGuiCache == 0)
                    {
                        mpGuiCache = lpPayload->mpGuiCache;
                        ResetIconManager(lpEvent);

                        CGS_ASSERT(mpIconManager != 0, "NULL != mpIconManager");  // cpp:339

                        mpIconManager->SetIconsVisible(true);

                        // The X360 stack-builds the OutputGuiEvent wrapper record inline:
                        // { payload size 1, type 328, payload offset 12 } posted at 16
                        // bytes on channel 40. It never writes the payload byte itself.
                        GuiEventCrashNavCacheAcceptedPayload lCacheAccepted;
                        CgsGui::GuiEventWrapper<GuiEventCrashNavCacheAcceptedPayload,
                                                KI_CHANNEL_GUI_EVENT> lRecord(lCacheAccepted);
                        mpStateInterface->GetOutputEventQueue()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lRecord),
                            lRecord.GetChannel(),
                            static_cast<s32>(sizeof(lRecord)));
                    }
                }
                break;

            case KI_EVENT_UPDATE_SATNAV:
                if (mpIconManager != 0)
                {
                    mpIconManager->UpdateSatNavInfo(
                        reinterpret_cast<const GuiEventUpdateSatNav*>(lpEvent));
                }
                break;

            case KI_EVENT_CHALLENGED_EVENT_DATA_RESPONSE:
                {
                    CGS_ASSERT(lpEvent != 0, "lpEventData");   // cpp:375 (non-fatal)

                    const GuiEventChallengedEventDataResponsePayload* lpResponse =
                        static_cast<const GuiEventChallengedEventDataResponsePayload*>(lpEvent);

                    // 64-bit compare (see the ASM note in the banner): the response is
                    // only relevant while the cursor is still on the event that asked
                    // for it. muHoveredEventID widens to 64 bits for the compare exactly
                    // as the X360's `mr r10, r30` after an `lwz` does.
                    const u32 luHoveredEventId = muHoveredEventID;
                    if (lpResponse->mu64EventID == luHoveredEventId)
                    {
                        CrashNavPanel::ChallengedEventScore lScore;
                        lScore.muScoreOverride = lpResponse->muScoreOverride;
                        std::memcpy(lScore.maReserved_04, lpResponse->maReserved_0C,
                                    sizeof(lScore.maReserved_04));

                        mCrashNavPanel.SetEventPanelData(luHoveredEventId, &lScore, true);
                    }
                }
                break;

            case KI_EVENT_ROAD_RULE_DATA:
                UpdateRoadRule(lpEvent);
                break;

            case KI_EVENT_ROAD_RULE_BATCH_DATA_RESPONSE:
                CGS_ASSERT(lpEvent != 0, "lpRoadRules");            // cpp:362 (non-fatal)
                CGS_ASSERT(mpIconManager != 0, "mpIconManager");    // cpp:363 (non-fatal)
                mpIconManager->SetRoadRuleBatchData(
                    reinterpret_cast<const GuiEventRoadRuleBatchDataResponse*>(lpEvent));
                break;

            default:
                break;
            }

            // EVERY event -- handled or not -- is also offered to the map component and
            // to the panel. A panel that answers true has changed its icon filter.
            mMainMapComponent.RecvEvent(lpEvent, liEventId);
            if (mCrashNavPanel.RecEvent(lpEvent, liEventId, liEventSize))
                SetFilterFromPanel();
        }

        // `lbz r11, 0x44(r31)` / `cmplwi cr6, r11, 1` -- an explicit compare against 1,
        // not a plain zero test.
        if (mbIsScreenLoaded)
            UpdateMainMap();

        CheckForLoadComplete();

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:417 (non-fatal)

        // One-shot: the frame the cache first reports every expected apt component
        // initialised, wire the components up and paint the button prompts.
        if (!mbItemsLoaded && mbIsScreenLoaded &&
            mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            mbItemsLoaded = true;   // stored BEFORE the virtual call, per the asm
            SetupComponents();      // vtable +0x30
            SetFilterFromPanel();
            UpdateButtonPrompts();
        }

        mCrashNavPanel.Update();
    }

    // ================================================================================
    //  UpdateIconManager  @ 0x824CBA70  (cpp:1119 region)
    //
    //  Drive the shared map-icon manager for this frame and resolve what the cursor is
    //  hovering over. Feeds the manager the current zoom-derived icon scale and event
    //  context, then -- unless the map is mid-zoom with the cursor already locked --
    //  collects every on-screen icon position, snaps the cursor to the nearest one and
    //  latches the hovered thing (local player / rival / road sign / drive-through /
    //  event) into the state's hover members. Whatever the outcome, it publishes the
    //  hover set to the view so the renderer can highlight it.
    // ================================================================================
    void CrashNavMap::UpdateIconManager()
    {
        // The whole body is inside `if (mpIconManager)` on the X360 (0x824CBA8C jumps
        // straight to the epilogue); an early return reads better and is identical.
        if (mpIconManager == 0)
            return;

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1123 (non-fatal)

        // Icons are authored at a reference world scale; the manager needs the ratio
        // against the map's current zoom. X360: `lfs f13, mMainMapComponent+1632` /
        // `lfs f0, flt_82F259E0` (measured 3500.0f -- scratchpad/waveJ/crashnav_consts.txt,
        // and UpdateIconManager is that constant's ONLY reader) / `fdivs` / `stfsx`.
        //
        // The destination +0xA198 is NOT a MapIconManager member: it is
        // RoadSignIconManager::mfZoomFactor (DWARF BrnRoadSignIconManager.h:273) inside the
        // embedded mRoadSignIconManager, which BrnMapIconManager.h documents at its
        // SetZoomFactor declaration -- the X360 fully inlined that forwarding call, leaving
        // only the single stfsx. Going through the declared method restores the real call.
        // The source is MainMapComponent::mfWorldZoomScaleFactor (DWARF BrnMainMap.h:220,
        // X360 comp+1632); it is private with no DWARF accessor, so this class is a friend.
        mpIconManager->SetZoomFactor(
            KF_EVENT_ICON_REFERENCE_SCALE / mMainMapComponent.mfWorldZoomScaleFactor);

        // Two more straight member stores (`stb ...,0x998` and `stbx ...,0xAA18`); the
        // X360 re-loads mpIconManager before each one.
        mpIconManager->mi8CurrentEventIndex   = mi8CurrentEventIndex;
        mpIconManager->mbIsDisplayingEventInfo = mbIsInEvent;

        if (mbItemsLoaded)
            mpIconManager->Update();

        // While the map is animating a zoom the cursor must not re-snap -- it stays glued
        // to whatever it was already locked onto, re-projected each frame. Panning is
        // excluded because the cursor is being driven by the stick there.
        if (mMainMapComponent.IsZooming() && mbIsCursorLockedToIcon &&
            meCursorMode != E_CURSORMODE_PANNING)
        {
            mCursor.SetPosition(
                MapTransform::WorldToDevice(LaneAsVector3(mLockedIconInfo.GetPositionLane()),
                                            false),
                false);

            PostSetHoveredEventIcon(mpStateInterface, mHoveredDriveThruID, mHoveringRivalId,
                                    muHoveredEventID, mpLockedIconName);
            return;   // the X360 tail-returns here; no UpdateButtonPrompts on this path
        }

        // X360 r25: set once the snapped-icon block is entered and cleared again by the
        // road-sign arm (which owns mpLockedIconName). Every other arm therefore drops the
        // road-sign name at the end of the block. Modelled as a named bool in place of the
        // decompiler's v9 / LABEL_57 goto web.
        bool lbClearLockedIconName = false;

        switch (meCursorMode)
        {
        case E_CURSORMODE_NONE:
            // First frame on the map: park the cursor on the player and start selecting.
            PlaceCursorOnPlayer();
            meCursorMode = E_CURSORMODE_SELECTING_ICONS;
            break;

        case E_CURSORMODE_SELECTING_ICONS:
        case E_CURSORMODE_ZOOMEDOUT:
            {
                Vector2 lav2IconPositions[KI_MAX_SNAP_LOCATIONS];
                s32     liNumIcons = 0;
                mpIconManager->GetSatNavIconPositions(lav2IconPositions, &liNumIcons);

                const GuiCursor::SnapResults lSnapResults =
                    mCursor.UpdateToSnapLocations(lav2IconPositions,
                                                  static_cast<u32>(liNumIcons), false);

                if (liNumIcons == 0)
                {
                    // Nothing on the map at all.
                    if (meEventIconDisplayType !=
                        GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_OFFLINE_EVENTS)
                    {
                        mCrashNavPanel.ShowBlank();
                    }
                    else
                    {
                        muHoveredEventID = 0;
                        mCrashNavPanel.SetEventPanelData(0, 0, false);
                    }
                }
                else if (lSnapResults.luLockedIndex ==
                         GuiCursor::SnapResults::KI_INVALID_LOCK_INDEX)
                {
                    // The cursor drifted off every icon. Only an unsnapped cursor lets go.
                    if (!mCursor.GetAlwaysSnap() && mbIsCursorLockedToIcon)
                    {
                        mbIsCursorLockedToIcon = false;
                        mpLockedIconName       = 0;
                        muHoveredEventID       = 0;
                    }
                }
                else if (!mbIsCursorLockedToIcon || mCursor.GetAlwaysSnap())
                {
                    const s32 liSnappedIndex = static_cast<s32>(lSnapResults.luLockedIndex);

                    lbClearLockedIconName  = true;
                    mbIsCursorLockedToIcon = true;

                    if (liSnappedIndex == liNumIcons - 1)
                    {
                        // GetSatNavIconPositions always appends the local player's icon
                        // last, so the final index is the player.
                        mLockedIconInfo.SetIconType(
                            GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR);
                        mLockedIconInfo.SetPositionLane(mpGuiCache->GetWorldCameraPosition());
                        mbLocalPlayerSelected = true;
                        muHoveredEventID      = 0;
                        mHoveredDriveThruID   = 0;
                        mHoveringRivalId      = mpGuiCache->GetLocalPlayerCarId();

                        // The X360 schedules this test below the stores above (the cache
                        // is dereferenced by the lvx either way); non-fatal, so the
                        // position is behaviourally irrelevant -- kept where the asm has it.
                        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1222

                        // Name the player from the online roster, matched on the local
                        // player's active-race-car index.
                        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
                            lpPlayerInfo = FindOnlinePlayerByActiveRaceCarIndex(
                                mpGuiCache, mpGuiCache->GetPlayerActiveRaceCarIndex());

                        if (lpPlayerInfo != 0)
                            mPlayerName.Construct(lpPlayerInfo->mPlayerName.macName);
                        else
                            mPlayerName.macName[0] = '\0';   // X360 `stb r30, 0x60C8`
                    }
                    else if (mbSelectRivals)
                    {
                        mbLocalPlayerSelected = false;

                        const GuiEventUpdateSatNav::SatNavIconInfo* lpRivalIcon =
                            mpIconManager->GetRivalIconAtIndex(liSnappedIndex);

                        mLockedIconInfo.SetPositionLane(lpRivalIcon->GetPositionLane());

                        // A networked rival (an online match is being started) gets the
                        // network-rival icon and a real gamertag; anyone else is a plain
                        // offline rival with no name.
                        if (mpGuiCache->IsOnlineStartInProgress())
                        {
                            mLockedIconInfo.SetIconType(
                                GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL);

                            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1252

                            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
                                lpPlayerInfo = FindOnlinePlayerByActiveRaceCarIndex(
                                    mpGuiCache, lpRivalIcon->GetActiveRaceCarIndex());

                            if (lpPlayerInfo != 0)
                            {
                                // MEASURED: the X360 calls the collation-table compare and
                                // DISCARDS its answer -- there is no branch on r3 between
                                // this call and the Construct below (0x824CBE18..0x824CBE24).
                                // Kept because the binary makes the call.
                                LobbyNameCmp(mPlayerName.macName,
                                             lpPlayerInfo->mPlayerName.macName);
                                mPlayerName.Construct(lpPlayerInfo->mPlayerName.macName);
                            }
                            else
                            {
                                mPlayerName.macName[0] = '\0';
                            }
                        }
                        else
                        {
                            mLockedIconInfo.SetIconType(
                                GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_RIVAL);
                            mPlayerName.macName[0] = '\0';
                        }

                        mHoveringRivalId = lpRivalIcon->GetCgsId();
                    }
                    else if (mbUseRoadSigns)
                    {
                        // The road-sign arm owns mpLockedIconName, so it opts out of the
                        // end-of-block clear (X360 `mr r25, r30` -- unconditionally, before
                        // the lookup).
                        lbClearLockedIconName = false;

                        const char* lpcRoadSignName =
                            mpIconManager->GetRoadSignNameAtIndex(liSnappedIndex);

                        // Pointer identity, not strcmp: the manager hands back the same
                        // interned string for the same sign (X360 `cmplw`).
                        if (mpLockedIconName != lpcRoadSignName)
                        {
                            mpLockedIconName      = lpcRoadSignName;
                            muHoveredEventID      = 0;
                            mHoveredDriveThruID   = 0;
                            mHoveringRivalId      = 0;
                            mbLocalPlayerSelected = false;

                            // Ask the game side for this road's rule scores. The road id is
                            // the sign's name parsed as a decimal and SIGN-extended to 64
                            // bits (X360 `extsw`).
                            GuiEventRoadRuleDataRequest lRequest;
                            const s64 li64RoadId =
                                static_cast<s64>(std::atoi(lpcRoadSignName));
                            std::memcpy(lRequest.maData, &li64RoadId, sizeof(li64RoadId));

                            CgsGui::GuiEventWrapper<GuiEventRoadRuleDataRequest,
                                                    KI_CHANNEL_GUI_EVENT> lRecord(lRequest);
                            mpStateInterface->GetOutputEventQueue()->AddEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                lRecord.GetChannel(),
                                static_cast<s32>(sizeof(lRecord)));

                            // A road sign has no world record of its own, so the "locked"
                            // position is wherever the cursor is sitting.
                            mLockedIconInfo.SetPositionLane(
                                LaneAsVector4(MapTransform::DeviceToWorld(mCursor.GetPosition())));
                            mLockedIconInfo.SetIconType(
                                GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_ROADSIGN);
                        }
                    }
                    else if (meEventIconDisplayType ==
                             GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT)
                    {
                        // No event icons are being drawn at all, so every index in the set
                        // is a drive-through / junkyard -- no count check needed.
                        if (mbSelectDriveThrus)
                        {
                            const GuiEventUpdateSatNav::SatNavIconInfo* lpIcon =
                                mpIconManager->GetDriveThroughOrJunkyardAtIndex(liSnappedIndex);

                            muHoveredEventID      = 0;
                            mHoveringRivalId      = 0;
                            mbLocalPlayerSelected = false;
                            mHoveredDriveThruID   = lpIcon->GetCgsId();

                            mLockedIconInfo.SetPositionLane(lpIcon->GetPositionLane());
                            // @0x824CBEFC: `bl SatNavIconInfo::GetIconType` on the icon the
                            // manager just handed back (IDA truncates the symbol to
                            // "SatNavI"); the byte lands in mLockedIconInfo's own +0x28.
                            mLockedIconInfo.SetIconType(lpIcon->GetIconType());
                        }
                        else
                        {
                            PlaceCursorOnPlayer();
                        }
                    }
                    else if (mbSelectDriveThrus &&
                             liSnappedIndex < mpIconManager->GetDriveThroughAndJunkyardCount())
                    {
                        // Drive-throughs occupy the front of the index space. (Byte-identical
                        // instruction sequence to the arm above -- the X360 emitted it twice,
                        // at 0x824CBED4 and 0x824CBF98; kept written out twice so no
                        // unattested helper has to be added to the class.)
                        const GuiEventUpdateSatNav::SatNavIconInfo* lpIcon =
                            mpIconManager->GetDriveThroughOrJunkyardAtIndex(liSnappedIndex);

                        muHoveredEventID      = 0;
                        mHoveringRivalId      = 0;
                        mbLocalPlayerSelected = false;
                        mHoveredDriveThruID   = lpIcon->GetCgsId();

                        mLockedIconInfo.SetPositionLane(lpIcon->GetPositionLane());
                        // @0x824CBFC0 -- the second call site of the same accessor.
                        mLockedIconInfo.SetIconType(lpIcon->GetIconType());
                    }
                    else
                    {
                        const u32 luEventId =
                            mpIconManager->GetEventIDAtIndex(liSnappedIndex);

                        if (luEventId != muHoveredEventID)
                        {
                            // Newly hovered event: ask the game side for its challenge
                            // scores. The id is ZERO-extended to 64 bits (X360 `clrldi`).
                            GuiEventChallengedEventDataRequest lRequest;
                            const u64 lu64EventId = luEventId;
                            std::memcpy(lRequest.maData, &lu64EventId, sizeof(lu64EventId));

                            CGS_ASSERT(mpStateInterface != 0, "mpStateInterface");  // cpp:1321

                            CgsGui::GuiEventWrapper<GuiEventChallengedEventDataRequest,
                                                    KI_CHANNEL_GUI_EVENT> lRecord(lRequest);
                            mpStateInterface->GetOutputEventQueue()->AddEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                lRecord.GetChannel(),
                                static_cast<s32>(sizeof(lRecord)));
                        }

                        muHoveredEventID      = luEventId;
                        mHoveredDriveThruID   = 0;
                        mHoveringRivalId      = 0;
                        mbLocalPlayerSelected = false;

                        // The display record's leading 16-byte lane is the junction's
                        // world position (X360 `lvx128 v0, r0, r3` on the returned
                        // pointer, @0x824CBF7C). The committed record names it mv3Position.
                        mLockedIconInfo.SetPositionLane(LaneAsVector4(
                            mpGuiCache->GetProfileEventDisplayInfo(luEventId)->mv3Position));
                        mLockedIconInfo.SetIconType(
                            GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNCTION);
                    }

                    if (lbClearLockedIconName)
                        mpLockedIconName = 0;
                }
            }
            break;

        case E_CURSORMODE_INSPECTING_ICONS:
            // The inspect camera owns the map; nothing to re-snap, just re-publish.
            break;

        case E_CURSORMODE_PANNING:
            mCrashNavPanel.ShowBlank();
            break;

        default:
            {
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer,
                                             CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unhandled cursor mode "
                           << static_cast<s32>(meCursorMode)
                           << " in CrashNavMap::UpdateIconManager()\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), KAC_ASSERT_FILE, 1374);
                CgsDev::Assert::EndAssert();
            }
            break;
        }

        PostSetHoveredEventIcon(mpStateInterface, mHoveredDriveThruID, mHoveringRivalId,
                                muHoveredEventID, mpLockedIconName);

        if (mbItemsLoaded)
            UpdateButtonPrompts();
    }

// -----------------------------------------------------------------------------------
// NOTE: the drive-through latch appears TWICE above. That is deliberate: the X360 emitted
// two byte-identical copies (0x824CBED4 and 0x824CBF98), and folding them into a shared
// private helper would mean minting a member of CrashNavMap that neither the X360 ledger
// nor the DecFIGS DWARF attests. Written out twice, this body needs no addition to the
// owning header at all.
// -----------------------------------------------------------------------------------

}
