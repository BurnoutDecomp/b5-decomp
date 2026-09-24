#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsID / CgsIDCompress
#include "GameShared/GameClasses/Containers/CgsHash.h"                  // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                    // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // the state in-queue
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log (the deferral gap log)
#include "GameSource/Gui/BrnGuiCache.h"                                 // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                         // GuiOverlayRequest / E_GUIFLOW_HUD
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"                       // BrnFlapt::FlaptManager
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                       // BrnFlapt::FileRef
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"             // MovieClipInstance::ResetTimeline
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h" // AttachToTextFieldComponent
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData

#include <cstdio>    // std::snprintf (the one-shot deferral log)

// includes folded in from the BrnRaceMainHudState_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the two streamed asserts)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // Start/StopMonitor
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / CgsModule::Event
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // the ShowHide / ticker / overlay records
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"                // FreeburnChallengeManager (state + host reads)
#include "GameSource/Gui/BrnGuiPerfmons.h"                                // GuiPerfmons::miHudStateUpdate
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SnPrintf
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTriggerPayload (event 21, typed)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // CgsLanguage::LanguageManager
#include "SharedClasses/DataLists/ChallengeList.h"                        // BrnResource::ChallengeList
#include "SharedClasses/DataLists/ChallengeListEntry.h"                   // BrnResource::ChallengeListEntry(Action)
#include <cstring>   // std::strstr / std::strcmp / std::strncpy / std::memset

// Reconstructed from BURNOUT_X360_ARTIST.XEX -- BrnGui::RaceMainHudState, the RACE_MAIN
// slot of the BrnHudFlow 14-state pool (the in-event main HUD). Landed here:
//   RaceMainHudState::SetExpectedComponent(const char*)  @ 0x82473698
//   the .rdata resource-tuple table + its count           @ 0x82F25F88 / @ 0x82F25F84
//
// E1 WAVE 2026-08-26 -- THE ODR FORK IS RETIRED. This TU used to declare its own
// `struct RaceMainHudState { u8 maHeadReserved[0x3C]; u32 maExpectedComponents[64];
// u32 muExpectedComponentCount; u8 maBodyReserved[0x7910-0x140]; }` in namespace BrnGui
// while BrnHudFlow.cpp compiled against a header that declared no members at all -- two
// definitions of one class, and the fork's constructor zero-filled 0x7910 bytes into a
// State-sized NewPoolState<RaceMainHudState> allocation. The fork is deleted; the one
// definition is BrnRaceMainHudState.h, grown onto the DecFIGS DWARF member set, and the
// bodies below address the real members by name.
//
// The constructor @0x82508110 moved to the header as an inline body: it is the compiler's
// own sub-object-vtable chain (no RaceMainHudState POD is written on console) and
// BrnHudFlow.cpp -- which IS on the build -- placement-news the state, so its definition
// has to be visible whether or not this TU is mounted. See the header for the citation.

namespace BrnGui
{
    namespace
    {
        // ---- the three output channels (same ids the FBurn sibling names) -----------
        const s32 KI_CHANNEL_GUI_OUT    = 40;  // GuiEventOut
        const s32 KI_CHANNEL_VIEW_STATE = 41;  // GuiOutViewState

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ARTIST event-64 payload (the cache hand-off event UpdateSetupState waits on).
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        // 16-byte GuiEvent<N> command { 1, N, 12, flag } -- the shared state-channel
        // record (BrnFBurnMainHudState.cpp:66 is the same helper, file-local there too).
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;
            u8 maPad[3];
            explicit GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel, u8 lu8Flag = 0)
        {
            GuiCommandEvent16<N> lEvent(lu8Flag);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
        }

        // The 12-byte id-213 show/hide PAYLOAD (GuiEventShowHideSatNav's real body:
        // meMapType / mfFadeTime / mbShow -- BrnGuiDemangledEventTypes.h:744). Both
        // UpdateSetupState arms build {E_MAPTYPE_GPS(1), 0.0f, false}: `stw r30(=1)` at
        // payload+0, `stfs flt_82001CC0(=0.0f)` at payload+4, `stb r29(=0)` at payload+8
        // (@0x8247A108..0x8247A114 and @0x8247A148..0x8247A15C).
        struct SatNavShowHidePayload
        {
            s32 miMapType;
            f32 mfFadeTime;
            u8  mu8Show;
            u8  mau8Pad[3];
            SatNavShowHidePayload(s32 liMapType, f32 lfFadeTime, u8 lu8Show)
                : miMapType(liMapType), mfFadeTime(lfFadeTime), mu8Show(lu8Show)
            { mau8Pad[0] = mau8Pad[1] = mau8Pad[2] = 0; }
        };

        // The 24-byte wire record StateInterface::OutputViewState<GuiEventShowHideSatNav>
        // @0x82476DD8 emits: { 12, 213, 12 } + the 12-byte payload, channel 41. Built by
        // hand for the same reason BrnFBurnMainHudState.cpp:97 does -- the wrapped-channel
        // template is header-only and the record is byte-identical.
        struct GuiShowHideEvent24 : public CgsGui::GuiEvent<213>
        {
            SatNavShowHidePayload mPayload;
            GuiShowHideEvent24(const SatNavShowHidePayload& lPayload)
                : CgsGui::GuiEvent<213>(12, 12), mPayload(lPayload) {}
        };

        void PostShowHideSatNav24(CgsGui::StateInterface* lpInterface,
                                  const SatNavShowHidePayload& lPayload)
        {
            GuiShowHideEvent24 lEvent(lPayload);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_VIEW_STATE, 24);
        }

        // OnLeave's 16-byte type-2 id-536 record. The X360 builds the trailing pair as two
        // bytes in a scratch half-word and stores it with one `sth` (@0x82479AC0..0x82479ADC:
        // `stb r27(=0)` / `stb r29(=1)` / `lhz` / `sth r11, +0xC`), so the payload is a
        // {0, 1} byte pair at +0x0C, not a 16-bit 256.
        struct GuiEvent536 : public CgsGui::GuiEvent<536>
        {
            u8 mu8A;   // +0x0C == 0
            u8 mu8B;   // +0x0D == 1
            u8 mau8Pad[2];
            GuiEvent536() : CgsGui::GuiEvent<536>(2, 12), mu8A(0), mu8B(1)
            { mau8Pad[0] = mau8Pad[1] = 0; }
        };

        // The OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> wire record OnLeave
        // stack-builds inline (@0x82479894..0x824798B8): { 8, 188, 16, <pad>, CgsID },
        // channel 40, 24 bytes.
        struct GuiOverlayWaitFinishWire : public CgsGui::GuiEvent<188>
        {
            u32                         muPad0C;    // +0x0C (the 8-aligned payload slot)
            GuiOverlayWaitFinishRequest mRequest;   // +0x10

            explicit GuiOverlayWaitFinishWire(const char* lpcOverlayName)
                : CgsGui::GuiEvent<188>(8, 16), muPad0C(0)
            {
                mRequest.Construct(lpcOverlayName);
            }
        };

        // The OutputGuiEvent<BrnGui::GuiOverlayRequest> wire record (@0x82436BE0):
        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes. Same shape
        // the BrnInGame.cpp precedent posts.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C
            GuiOverlayRequest mRequest;   // +0x10
            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0) {}
        };

        // The pre-event overlay's district message-parameter ids (@0x82F27718, image-read;
        // 18 entries, then a second "_LC" table follows at index 18). Kept file-local: the
        // X360 asserts never name this table, and it is NOT BrnWorld::KAPC_DISTRICT_NAMES
        // (which carries debug names, not localisation ids).
        const char* const KAPC_DISTRICT_OVERLAY_MESSAGE_IDS[18] =
        {
            "NHD_OV",  "NHD_WA", "NHD_TB", "NHD_BS", "NHD_ES", "NHD_HP",
            "NHD_HH",  "NHD_RRC","NHD_SB", "NHD_PV", "NHD_PW", "NHD_CS",
            "NHD_LP",  "NHD_SV", "NHD_DT", "NHD_RC", "NHD_MC", "NHD_WF",
        };

        // ---- component deferral log (the FBurn LogDeferredComponent idiom) ----------
        // Each site below keeps the X360 control flow and logs the gap ONCE instead of
        // inventing a body for a callee whose X360 function is not reconstructed yet.
        //
        // NOT IN THE X360 BINARY -- this helper is ours (progress/identity.json has no
        // LogDeferredComponent and neither does any ARTIST export); there is no asm to
        // match it against. Waves S3 and S4 each landed their own copy of it in their
        // partfiles, and when issue #20 folded the family into this TU the three copies
        // were one name over two different bodies. THE ONE-SHOT 16-SLOT SHAPE WINS: it is
        // what this very comment already claimed ("logs the gap ONCE") and what wS3/wS4
        // both wrote, and their call sites are per-frame (UpdateRunning @0x8247E898,
        // Update, HandleTrigger) where the unconditional variant that stood here floods
        // the log. The only edit to the surviving text is dropping wS3/wS4's "(wave SN)"
        // tag -- one TU can carry only one of them, and the wave is provenance of our
        // reconstruction, not of the console.
        void LogDeferredComponent(const char* lpacComponent)
        {
            static const char* sapcNames[16];
            for (s32 li = 0; li < 16; ++li)
            {
                if (sapcNames[li] == lpacComponent)
                    return;
                if (sapcNames[li] == 0)
                {
                    sapcNames[li] = lpacComponent;
                    char lac[160];
                    std::snprintf(lac, sizeof(lac),
                                  "[RaceMainHud] %s -- component TU deferred.\n",
                                  lpacComponent);
                    CgsDev::Log::WriteToLog(lac);
                    return;
                }
            }
        }

        // ---- GuiCache boundary (X360 cache members BrnGuiCache.h has not named yet) --
        // Same discipline as BrnFBurnMainHudState.cpp's boundary block: one named leaf per
        // un-named console field, so the eventual cache carve lands in exactly these spots.

        // (2026-08-27 verify round: the friends-list-overlay and in-event leaves that stood
        // here are RETIRED -- this same wave carved and named both cache bytes, so the code
        // below reads mpCache->IsFriendsListOpen() (+0xB86C) and
        // mpCache->IsEventPreparedForModeStart() (+0xA014) directly, matching the wS2/wS3
        // partfiles. Their own DELETE-WHEN conditions were already met at review time.)

        // [FLAG PC-platform leaf] the road-rule-shot arm's own gate byte (X360
        // cache+0xAC59, inside BrnGuiCache.h's mPad_AC4C[14]). UpdateSetupState @0x8247A314
        // gates the whole opponent-record search + RoadRuleShotComponent setup on it.
        // Reported false, which parks the arm; the three fields it would then need
        // (+0xAC50 CgsID road id, +0xAC58 time-rule bool, +0xAC5B snap-vs-show bool) are
        // all in the same un-named pad, so there is nothing honest to read yet.
        // DELETE-WHEN: BrnGuiCache.h carves mPad_AC4C[14] into its four X360 fields.
        bool GuiCache_RoadRuleShotPending(const GuiCache* /*lpGuiCache*/)
        {
            return false;
        }
    }

    // =======================================================================
    //  The static .rdata resource table (values read from the XEX image at
    //  0x82F25F88, count at 0x82F25F84; the name after each id is
    //  off_82F278E0[id] from the same image -- the table the FBurnMainHudState
    //  42-entry recovery used).
    // =======================================================================
    // 21 entries: the B5RaceHud apt movie plus the aux component imports the in-event HUD
    // mounts. Type 7 == E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE, type 11 ==
    // E_GUI_RESOURCETYPE_LOCALISED_TEXT. Against the freeburn state's 42-entry list this
    // one adds B5CompassComponent (24) and B5ShowtimeComponents (88) and drops the whole
    // freeburn menu/ticker/mugshot block -- the in-event HUD's own surface.
    const CgsGui::sResourceTuple RaceMainHudState::maResourcesToLoad[] =
    {
        { 192u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5RaceHud
        {  32u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // Timer
        {  23u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5NorthIndicatorComponent
        {  24u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CompassComponent
        {  37u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5HudMessage
        {  27u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // CountdownIcon
        { 200u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5SatNavComponent
        {  25u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // SatNavDistance
        {  26u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // SatNavStatic
        { 199u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT   },  // SatNavMap
        { 201u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT   },  // SatNavMask
        {  60u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // BoostMessage
        {  56u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5Triggers
        {  62u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5PositionIndicatorComponent
        {  64u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // DistrictIcon
        {  65u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // DistrictMarker
        {  73u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5RaceEventInfo
        {  75u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5PositionTableComponent
        {  33u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5RoadRuleComponent
        {  88u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ShowtimeComponents
        {  90u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5VersionTextComponent
    };
    const u32 RaceMainHudState::muNumResourcesToLoad = 21;

    // =======================================================================
    //  SetExpectedComponent  @ 0x82473698
    // =======================================================================
    // Append the hash of an expected APT-component name to the table. Asserts there is
    // room (count < 0x40 -- BrnRaceMainHudState.h:634, the assert's line argument
    // r5 = 0x27A), hashes the NUL-terminated name (length excludes the NUL), stores at
    // this[15 + count] (byte +0x3C + count*4 == mauExpectedComponentIds[count]) and
    // increments the count. The hash stays in r3 at return.
    u32 RaceMainHudState::SetExpectedComponent(const char* lpcName)
    {
        CGS_ASSERT(muNumExpectedComponents < KU_MAX_INIT_COMPONENTS_NUM,
                   "No space for new expected component");

        const char* lpc = lpcName;
        while (*lpc) ++lpc;
        u32 luHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpcName), static_cast<int>(lpc - lpcName));   // length excludes the NUL

        mauExpectedComponentIds[muNumExpectedComponents] = luHash;
        ++muNumExpectedComponents;
        return luHash;
    }

    // =======================================================================
    //  The remaining .rdata tables + constants (A3 wave 2026-08-27)
    // =======================================================================
    // The 76 observed event ids @0x8205AC08 (read straight out of the XEX image; the run
    // ends exactly where maIconIdentifiers begins at 0x8205AD38, i.e. 76 words wide --
    // which independently confirms the `li r5, 0x4C` count both Register/UnRegister pass).
    // See the header for why this is 76 and not the DWARF's 77.
    const s32 RaceMainHudState::maiEventToObserve[] =
    {
          6,   7,  21, 199, 200, 154, 156, 234,
        177, 179, 180, 224, 205, 148, 158, 226,
        227,  64, 206, 108, 377, 365, 239, 403,
        379, 218, 367, 368, 382, 383, 384, 385,
        386, 387, 388, 389, 390, 391, 364, 394,
        401, 400, 398, 320, 291, 333, 338, 339,
        335, 336, 340, 341, 343, 347, 325, 101,
        102, 103, 104,  94,  95, 106, 283, 582,
        583, 584, 221, 222, 182, 183, 574, 576,
        578, 573, 579, 581,
    };
    const s32 RaceMainHudState::miNumEventsObserved = 76;

    // The apt/flapt component names OnEnter hands to each embedded component. Every string
    // below is the X360 literal at its call site, and every one of them fits its DWARF
    // array bound EXACTLY (13/21/11/15/10/13/9/21/23/11/20/18/12/11/18/12/15/15/24/11/21/
    // 15/15) -- which is itself a check that the header's static block is the right shape.
    const char  RaceMainHudState::macEventInfoName[13]                    = "EventInfo_mc";
    const char  RaceMainHudState::KAC_SAT_NAV_ANIMATOR_NAME[21]           = "SatNavComponent_anim";
    const char  RaceMainHudState::macPaybackName[11]                      = "Payback_mc";
    const char  RaceMainHudState::macHudMessagesName[15]                  = "hudMessages_mc";
    const char  RaceMainHudState::macDistrictMarkerName[10]               = "marker_mc";
    const char  RaceMainHudState::macBoostManagerComponentName[13]        = "BoostManager";
    const char  RaceMainHudState::macEventCountdownName[9]                = "event_mc";
    const char  RaceMainHudState::macPositionIndicatorName[21]            = "PositionIndicator_mc";
    const char  RaceMainHudState::macPlayerPositionTableName[23]          = "PlayerPositionTable_mc";
    const char  RaceMainHudState::macFriendListName[11]                   = "friendList";
    const char  RaceMainHudState::macFriendsListChangeIconName[20]        = "FriendListChange_mc";
    const char  RaceMainHudState::macGeneralTransitionComponentName[18]   = "EventHud_Animator";
    const char  RaceMainHudState::macRoadRuleComponentName[12]            = "RoadRule_mc";
    const char  RaceMainHudState::macMugShotComponentName[11]             = "MugShot_mc";
    const char  RaceMainHudState::macMugshotDIARRHiderComponentName[18]   = "MugshotDIARR_anim";
    const char  RaceMainHudState::KAC_MUGSHOT_COMPONENT_GAMERTAG_NAME[12] = "Gamertag_mc";
    const char  RaceMainHudState::KAC_BOUNCE_BOOST_NAME[15]               = "ShowtimeButton";
    const char  RaceMainHudState::KAC_IDENT_ANIMATOR_NAME[15]             = "Ident_Animator";
    const char  RaceMainHudState::KAC_ONLINE_TIMEOUT_TIMER_NAME[24]       = "OnlineEventTimeout_anim";
    const char  RaceMainHudState::KAC_COMPASS_COMPONENT_NAME[11]          = "Compass_mc";
    const char  RaceMainHudState::KAC_CHALLENGE_COMPONENT_NAME[21]        = "FreeburnChallenge_mc";
    const char  RaceMainHudState::KAC_CHALLENGE_SELECTOR_COMPONENT_NAME[15] = "FBChallenge_mc";
    const char  RaceMainHudState::KAC_CHALLENGE_ON_COMPONENT_NAME[15]     = "ChallengeOn_mc";

    // @0x82F26228 -- a POINTER constant, not an array (OnEnter loads it with
    // `lwz r4, off_82F26228@l(r27)` @0x824795E4). Image-read: "RaceRoadRuleShot_mc".
    const char* RaceMainHudState::KPC_ROAD_RULE_SHOT_COMPONENT_NAME = "RaceRoadRuleShot_mc";

    // @0x8205AD38 -- the countdown icon frame labels, indexed by EventCountdownState
    // (DONE/GO/ONE/TWO/THREE/IDLE). Image-read.
    const char* RaceMainHudState::maIconIdentifiers[6] =
    {
        "invisible",   // E_EVENT_COUNTDOWN_STATE_DONE
        "go",          // E_EVENT_COUNTDOWN_STATE_GO
        "one",         // E_EVENT_COUNTDOWN_STATE_ONE
        "two",         // E_EVENT_COUNTDOWN_STATE_TWO
        "three",       // E_EVENT_COUNTDOWN_STATE_THREE
        "invisible",   // E_EVENT_COUNTDOWN_STATE_IDLE
    };

    // @0x82F261E0 -- 18 entries indexed by GsmIO::EGameModeType. Image-read: only the five
    // offline race-style modes (0 RACE, 1 FACE_OFF, 5 BURNING_ROUTE, 6 ELIMINATOR,
    // 8 MARKED_MAN) carry "OffSplshRace"; mode 7 (STUNT_ATTACK) carries the EMPTY string
    // (0x820046A7, the same "" every SetItem clear uses) and the remaining twelve are NULL
    // -- which is why both call sites assert the entry before using it.
    const char* RaceMainHudState::KAPC_PRE_EVENT_OVERLAYS[18] =
    {
        "OffSplshRace",   //  0 E_MODE_RACE
        "OffSplshRace",   //  1 E_MODE_FACE_OFF
        0,                //  2 E_MODE_OFFLINE_SHOWTIME
        0,                //  3 E_MODE_ROAD_RAGE
        0,                //  4 E_MODE_PURSUIT
        "OffSplshRace",   //  5 E_MODE_BURNING_ROUTE
        "OffSplshRace",   //  6 E_MODE_ELIMINATOR
        "",               //  7 E_MODE_STUNT_ATTACK
        "OffSplshRace",   //  8 E_MODE_MARKED_MAN
        0, 0, 0, 0, 0, 0, 0, 0, 0,   //  9..17 (online + traffic attack)
    };

    // The two debug component-override statics (DWARF .cpp:346/:347; X360 byte_82FB3C94 /
    // off_82FB3C98). Both live in .bss -- ForceReenter is their only writer -- so they
    // start cleared and UpdateSetupState's override block is retail-dead.
    bool        RaceMainHudState::msbDEBUG_OverrideNormalCptStates = false;
    const bool* RaceMainHudState::mspbDEBUG_ComponentEnabledStates = 0;

    // =======================================================================
    //  OnEnter  @ 0x82478EF8
    // =======================================================================
    // Reset the phase machine + the cache pointer, resolve and show the FLAPT persistent
    // HUD's "RaceMainHUD_mc" clip, clear the whole 25-flag block (plus the countdown /
    // overlay / mugshot / black-bar scalars that live between the components), register the
    // 76 observed events, then Construct+Prepare EVERY embedded component unconditionally
    // -- the flags only decide what gets DRIVEN later, never what gets built. Finally raise
    // mbFirstFrame / mbInRaceHud and post the three entry records
    // ({1,94,12,false} ch40 -> {1,580,12,false} ch40 -> {1,96,12,true} ch40).
    //
    // Unlike the FBurn sibling, RACE_MAIN does NOT post 215 or 308 here: it defers 214/215
    // to UpdateSetupState and the 215 show to RevealHud.
    void RaceMainHudState::OnEnter()
    {
        meInternalState = E_RACEINTERNALSTATE_SETUPSTATE;   // stw r30, 0x38(r31)
        mpCache         = 0;                                // stw r30, 0x140(r31)

        CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
        CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");   // CgsGuiStateInterface.h:344
        BrnFlapt::FlaptManager* lpFlaptManager = lpAccessPointers->GetFlaptManager();
        CGS_ASSERT(lpFlaptManager != 0, "NULL != mpFlaptManager");       // CgsGuiShared.h:194

        BrnFlapt::FileRef lFile;
        lpFlaptManager->GetFile(&lFile, 0);
        BrnFlapt::MovieClipRef lRootClip;
        lFile.GetRootMovieClip(&lRootClip);
        lRootClip.FindChildMovieClip(&mMainHUDMovieclip, "RaceMainHUD_mc");
        mMainHUDMovieclip.SetVisible(true);
        CGS_ASSERT(mMainHUDMovieclip.IsValid(), "mpMovieClipInst");      // BrnFlaptMovieClipRef.h:272
        mMainHUDMovieclip.mpMovieClipInst->ResetTimeline();

        // The reset run @0x8247901C..0x82479090: one `stb r30(=0)` per flag byte across
        // +0x150..+0x168 (exactly 25 -- see the header's FLAG), with mbHudVisible (+0x5F54)
        // and mfEventCountdownTimer (+0x1220, `stfs flt_82001CC0` == 0.0f) interleaved by
        // the scheduler. The console emits +0x15A/+0x15B/+0x15C and +0x167/+0x168 out of
        // ascending order; every store writes 0 to a distinct byte with no intervening
        // read, so they are listed in offset order here.
        mbHudVisible                   = false;   // +0x5F54
        mfEventCountdownTimer          = 0.0f;    // +0x1220
        mbSatNav                       = false;   // +0x150
        mbSatNavStatic                 = false;   // +0x151
        mbHudMessages                  = false;   // +0x152
        mbBoostBar                     = false;   // +0x153
        mbBoostMessages                = false;   // +0x154
        mbPreRaceCountdown             = false;   // +0x155
        mbPreRaceCountdownRenders      = false;   // +0x156
        mbEventInfo                    = false;   // +0x157
        mbDistrictMarker               = false;   // +0x158
        mbPlayerPositionTable          = false;   // +0x159
        mbFriendsList                  = false;   // +0x15A
        mbAboveCarIcons                = false;   // +0x15B
        mbRoadRuleComponent            = false;   // +0x15C
        mbPreEventOverlay              = false;   // +0x15D
        mbMugShotComponent             = false;   // +0x15E
        mbPaybackComponent             = false;   // +0x15F
        mbShowTimeBar                  = false;   // +0x160
        mbB5Ident                      = false;   // +0x161
        mbBurnoutSkillz                = false;   // +0x162
        mbOnlineTimeoutTimer           = false;   // +0x163
        mbCompass                      = false;   // +0x164
        mbFreeburnChallengeButtonStart = false;   // +0x165
        mbFreeburnChallengeSelector    = false;   // +0x166
        mbFreeburnChallengeTicker      = false;   // +0x167
        mbFreeburnChallengeOnComponent = false;   // +0x168

        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // X360 @0x824790EC: GuiModuleSerialiser::GetStaticLayout(accessPointers+0x14)
        // ->EndMessage() -- close any replay static-layout message the previous state left
        // open. [FLAG deferred] the tree's CgsGui::GuiAccessPointers carries no serialiser
        // member (it stops at mpGDMReceiverQueue), and BrnReplays::GuiModuleSerialiser has
        // no shared header; the layout stream is host-side inert. Identical deferral to
        // BrnFBurnMainHudState.cpp's OnEnter.
        // DELETE-WHEN: GuiAccessPointers grows mpSerialiser + GetSerialiser().

        // ---- component construction, in X360 call order ------------------------------
        // EventInfoComponent::Construct @0x82421160 / ::Prepare @0x82412C00 -- the DERIVED
        // pair, exactly as the console calls them (OnEnter pcode:
        // `EventInfoComponent::Construct(this+368, "EventInfo_mc", iface, 0);
        //  EventInfoComponent::Prepare(this+368, "EventInfo_mc", file);`). Both are bodied
        // in BrnEventInfo.cpp (2026-08-27 EI wave), which is therefore a mandatory paired
        // mount with this TU.
        mEventInfoComponent.Construct(macEventInfoName, mpStateInterface, 0);
        mEventInfoComponent.Prepare(macEventInfoName, lFile);

        mSatNavAnimationComponent.Construct(KAC_SAT_NAV_ANIMATOR_NAME, mpStateInterface, 0);
        // X360 @0x8247915C: Construct(this+0x6A0, iface, 0, 0) -- track-player mode, no
        // parent name (the same shape the FBurn sibling uses).
        mSatNavComponent.Construct(mpStateInterface, 0,
                                   SatNavComponent::E_SAT_NAV_MODE_TRACK_PLAYER);

        mPaybackComponent.Construct(macPaybackName, mpStateInterface, 0);

        mHudMessageComponent.Construct(macHudMessagesName, mpStateInterface, 0);
        {
            // X360 @0x824791A4..0x82479204: the asserting access chain, then the by-value
            // queue at cache+0x4080 handed to the component.
            CgsGui::GuiAccessPointers* lpQueueAccess = mpStateInterface->GetAccessPointers();
            CGS_ASSERT(lpQueueAccess != 0, "mpAccessPointers != NULL");   // CgsGuiStateInterface.h:344
            GuiCache* lpQueueCache = lpQueueAccess->GetGuiCache();
            CGS_ASSERT(lpQueueCache != 0, "mpGuiCache");                  // CgsGuiShared.h:201
            mHudMessageComponent.SetInGameMessagesQueue(lpQueueCache->GetInGameMessagesQueue());
        }
        mHudMessageComponent.Prepare(macHudMessagesName, lFile);

        mDistrictMarker.Construct(macDistrictMarkerName, mpStateInterface, 0);
        mDistrictMarker.Prepare(macDistrictMarkerName, lFile);

        mbFirstFrame = true;   // stb r29(=1), 0x1074(r31) @0x82479268 -- TRUE, not false
        mBoostMessageManager.Construct(macBoostManagerComponentName, mpStateInterface, 0);
        mBoostMessageManager.Prepare(macBoostManagerComponentName, lFile);

        meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_IDLE;   // stw 5, 0x121C(r31)
        // The ONLY non-null parent-name argument in the whole function: `addi r6, r11,
        // off_8205AD38@l` @0x8247928C passes the ADDRESS of maIconIdentifiers (the array),
        // not its first element. FlaptIconComponent::Construct ignores the argument.
        mEventCountdownIcon.Construct(macEventCountdownName, mpStateInterface, maIconIdentifiers);
        mEventCountdownIcon.Prepare(macEventCountdownName, lFile, 0);

        mPositionIndicatorComponent.Construct(macPositionIndicatorName, mpStateInterface, 0);

        // Called unconditionally (not flag-gated); the table is only ticked on online modes.
        mPlayerPositionTable.Construct(macPlayerPositionTableName, mpStateInterface, 0);
        mPlayerPositionTable.Prepare(macPlayerPositionTableName, lFile);

        mFriendsList.Construct(macFriendListName, mpStateInterface, 0);
        mFriendsListChangeIcon.Construct(macFriendsListChangeIconName, mpStateInterface, 0);
        mFriendsListChangeIcon.Prepare(macFriendsListChangeIconName, lFile);
        mFriendsList.Prepare(macFriendListName, lFile);

        PostCommand16<94>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        // The two "EventHud_Animator" halves. NOTE the divergence from the FBurn sibling,
        // which passes 0 as the flapt animator's debug name: RACE_MAIN passes the string to
        // BOTH (`addi r4, r11, aEventhudAnimat@l` feeds both vcalls).
        mGeneralTransitionComponentApt.Construct(macGeneralTransitionComponentName,
                                                 mpStateInterface, 0);
        mGeneralTransitionComponentFlapt.Construct(macGeneralTransitionComponentName,
                                                   mpStateInterface, 0);
        mGeneralTransitionComponentFlapt.Prepare(macGeneralTransitionComponentName, lFile, 0);

        // The 5th argument is 1 -- the B5RaceHud apt mount level (FBurn passes 1 too).
        mRoadRuleComponent.Construct(macRoadRuleComponentName, mpStateInterface, 0, 1);
        mRoadRuleComponent.Prepare(macRoadRuleComponentName, lFile);

        mfOverlayRemovalTime   = 0.0f;   // stfs f31(=0.0f), 0x6490(r31)
        mbOverlayInProgress    = false;  // stb  r30(=0),    0x6494(r31)
        meModeOverlayDisplayed = -1;     // stw  r11(=-1),   0x6498(r31)

        mMugShotComponent.Construct(macMugShotComponentName, mpStateInterface, 0);
        mMugShotComponent.Prepare(macMugShotComponentName, lFile, 0);

        // X360 @0x824794BC..0x82479524: the three gamertag handle words are cleared, then
        // AttachToTextFieldComponent resolves "MugShot_mc"/"Gamertag_mc" -> "Gamertag_txt"
        // and its three words are copied back over them. ("Gamertag_txt" is a bare literal:
        // KAC_MUGSHOT_COMPONENT_GAMERTAG_NAME is the 12-byte "Gamertag_mc".)
        mMugshotOpponentGamertag = BrnFlapt::TextFieldRef();
        {
            BrnFlapt::TextFieldRef lTextField;
            mMugshotOpponentGamertag = *AttachToTextFieldComponent(
                &lTextField, "Gamertag_txt", KAC_MUGSHOT_COMPONENT_GAMERTAG_NAME,
                macMugShotComponentName, lFile);
        }

        mMugshotDIARRHiderComponent.Construct(macMugshotDIARRHiderComponentName,
                                              mpStateInterface, 0);
        mMugshotDIARRHiderComponent.Prepare(macMugshotDIARRHiderComponentName, lFile, 0);

        mShowtimeBounceBoostButton.Construct(KAC_BOUNCE_BOOST_NAME, mpStateInterface, 0);
        mShowtimeBounceBoostButton.Prepare(KAC_BOUNCE_BOOST_NAME, lFile);

        mIdentAnimator.Construct(KAC_IDENT_ANIMATOR_NAME, mpStateInterface, 0);
        mIdentAnimator.Prepare(KAC_IDENT_ANIMATOR_NAME, lFile, 0);

        mfBlackBarsCurrentValue = 0.0f;   // stfs f31(=0.0f), 0x6578(r31)

        mRoadRuleShotComponent.Construct(KPC_ROAD_RULE_SHOT_COMPONENT_NAME, mpStateInterface, 0);
        mRoadRuleShotComponent.Prepare(KPC_ROAD_RULE_SHOT_COMPONENT_NAME, lFile);

        // [FLAG deferred] OnlineTimeoutComponent::Construct @0x82424760 / ::Prepare
        // @0x824248E0 -- BrnOnlineTimeoutTimerComponent.h models only the recovered
        // Show/Transin/Transout slice and declares neither, and its base hierarchy is not
        // reconstructed, so there is no honest call to make. Unconditional on console.
        // DELETE-WHEN: BrnOnlineTimeoutTimerComponent.{h,cpp} grow the two bodies.
        LogDeferredComponent("OnlineTimeoutComponent::Construct/Prepare");

        // The 4th argument is -1 -- the compass's parent apt layer.
        mCompass.Construct(KAC_COMPASS_COMPONENT_NAME, mpStateInterface, 0, -1);
        mCompass.Prepare(KAC_COMPASS_COMPONENT_NAME, lFile);

        mChallengeComponent.Construct(KAC_CHALLENGE_COMPONENT_NAME, mpStateInterface, 0);
        mChallengeSelectorComponent.Construct(KAC_CHALLENGE_SELECTOR_COMPONENT_NAME,
                                              mpStateInterface, 0);
        mChallengeOnComponent.Construct(KAC_CHALLENGE_ON_COMPONENT_NAME, mpStateInterface, 0);
        mChallengeOnComponent.Prepare(KAC_CHALLENGE_ON_COMPONENT_NAME, lFile, 0);

        mbChallengeOnShowing = false;   // stb r30(=0), 0x7908(r31)
        PostCommand16<580>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        mbInRaceHud = true;             // stb 1, 0x144(r31)
        PostCommand16<96>(mpStateInterface, KI_CHANNEL_GUI_OUT, 1);
    }

    // =======================================================================
    //  OnLeave  @ 0x82479770
    // =======================================================================
    // Park the phase machine in IDLE first, close the friends list, finish any pre-event
    // overlay handshake, unregister the 76 events, hide the persistent HUD clip, run the
    // transition/district-marker "transout" set, then post the teardown record chain
    // (PlayAptMovie("",1) -> the 24-byte id-213 hide -> 214 -> 94 -> 536) and clear the
    // expected-apt-component list.
    void RaceMainHudState::OnLeave()
    {
        const bool lbFriendsList = mbFriendsList;   // lbz 0x15A BEFORE the state store
        meInternalState = E_RACEINTERNALSTATE_IDLE;

        if (lbFriendsList)
        {
            CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
            CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");   // CgsGuiStateInterface.h:344
            GuiCache* lpGuiCache = lpAccessPointers->GetGuiCache();
            CGS_ASSERT(lpGuiCache != 0, "mpGuiCache");                       // CgsGuiShared.h:201
            if (lpGuiCache->IsFriendsListOpen())   // X360 OnLeave: `ori r10,r10,0xB86C ; lbzx`
            {
                mFriendsList.Close();
            }
        }

        if (mbPreEventOverlay && mbOverlayInProgress)
        {
            const s32 leMode = meModeOverlayDisplayed;
            mbOverlayInProgress = false;
            CGS_ASSERT(KAPC_PRE_EVENT_OVERLAYS[leMode] != 0,
                       "KAPC_PRE_EVENT_OVERLAYS[meModeOverlayDisplayed]");   // cpp:1668 (non-gating)

            // The record is { 8, 188, 16, <pad>, id } posted on the gui-out channel at
            // 24 bytes; Construct compresses the name into the id word.
            GuiOverlayWaitFinishWire lWire(KAPC_PRE_EVENT_OVERLAYS[meModeOverlayDisplayed]);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT, 24);
        }

        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mMainHUDMovieclip.SetVisible(false);

        mGeneralTransitionComponentFlapt.Run("transout");
        mDistrictMarker.mCountyContainerMovie.SetState("transout");
        mDistrictMarker.mDistrictContainerMovie.SetState("transout");

        // X360 @0x82479928: `lwz r11, 4(this+0x170)` == mEventInfoComponent's base
        // mAptRef.mpMovieClipInst -- reset the panel's timeline when it is bound.
        if (mEventInfoComponent.GetMovieClipRef().IsValid())
        {
            mEventInfoComponent.GetMovieClipRef().mpMovieClipInst->ResetTimeline();
        }

        mCompass.SetVisibility(false, false);
        mbBounceBoostPromptVisible = false;   // stb r27(=0), 0x653C(r31)
        // The empty-string clear (unk_820046A7 == ""); 15 == the "no button" glyph id on
        // both flanks, remap off.
        mShowtimeBounceBoostButton.SetItem(
            "",
            static_cast<FlaptButtonIconComponent::EPadButton>(15),
            static_cast<FlaptButtonIconComponent::EPadButton>(15),
            false);

        // Unmount the HUD apt movie at level 1 (name "", level 1). The X360 stack-builds
        // GuiEventPlayAptMovie by hand -- `stw r30(=&"")` at +0x0C then `stw r29(=1)` at
        // +0x10 -- i.e. name then level, exactly what PlayAptMovie writes. (Hex-Rays prints
        // the pair swapped; the asm at 0x824799A8/0x824799B8 is unambiguous.) Posted through
        // the interface so the 8-byte host name pointer is not truncated.
        mpStateInterface->PlayAptMovie("", 1);

        // The 24-byte id-213 hide, then the same 12-byte payload handed to the component.
        {
            const SatNavShowHidePayload lHide(1, 0.0f, 0);
            PostShowHideSatNav24(mpStateInterface, lHide);
            if (mbSatNav)
            {
                mSatNavComponent.RecvEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lHide), 213);
                mSatNavComponent.Destruct();
            }
        }

        PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
        PostCommand16<94>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);
        {
            GuiEvent536 lEvent;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT, 16);
        }

        if (mbRoadRuleComponent)
        {
            mRoadRuleComponent.EndTimers();   // lbz 0x15C @0x82479AF4 -- see the header FLAG
        }

        if (mpCache != 0)
        {
            mpCache->ClearExpectedAptComponentList(E_GUIFLOW_HUD);
        }
        for (u32 lu = 0; lu < KU_MAX_INIT_COMPONENTS_NUM; ++lu)
        {
            mauExpectedComponentIds[lu] = 0;
        }
        muNumExpectedComponents = 0;
        mbInRaceHud             = false;
    }

    // =======================================================================
    //  SetExpectedAptComponentList  @ 0x824749B0
    // =======================================================================
    // Clear the flow-1 watcher, wipe the 64-slot hash table, then install EXACTLY ONE
    // expected component: `addi r4, r31, 0x5E94` @0x824749F4 == this + 24212 ==
    // mGeneralTransitionComponentApt.macName (CgsGui::GuiComponent: vptr +0x00,
    // macName[128] +0x04, so the apt animator at +0x5E90 puts its name at +0x5E94), i.e.
    // the hash of "EventHud_Animator".
    //
    // THIS IS THE ONE BEHAVIOURAL DIFFERENCE FROM THE FBURN SIBLING, whose list is empty:
    // RACE_MAIN's WF_INIT phase blocks until the apt reports EventHud_Animator initialised.
    // If GuiCache::AreAllAptComponentsInitialised(E_GUIFLOW_HUD) never clears with a
    // one-entry list, the HUD loads and never reveals -- check that before blaming a
    // black screen on anything else.
    void RaceMainHudState::SetExpectedAptComponentList()
    {
        mpCache->ClearExpectedAptComponentList(E_GUIFLOW_HUD);

        for (u32 lu = 0; lu < KU_MAX_INIT_COMPONENTS_NUM; ++lu)
        {
            mauExpectedComponentIds[lu] = 0;
        }
        muNumExpectedComponents = 0;

        SetExpectedComponent(mGeneralTransitionComponentApt.GetName());

        CGS_ASSERT(muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM,
                   "muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM");   // cpp:3527

        mpCache->SetExpectedAptComponentList(E_GUIFLOW_HUD, mauExpectedComponentIds,
                                             muNumExpectedComponents);
    }

    // =======================================================================
    //  UpdateSetupState  @ 0x82479B48
    // =======================================================================
    // Drain the in-queue for the cache hand-off (event 64) and idle in SETUP until it
    // arrives; then drive the whole 25-flag block from the game mode, apply the debug
    // override, and push the per-flag setup (sat-nav, hud messages, position table, road
    // rules, the pre-event overlay and the road-rule-shot panel).
    bool RaceMainHudState::UpdateSetupState()
    {
        CGS_ASSERT(mpCache == 0, "mpCache == NULL");   // cpp:1886 (non-gating)

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue != 0)
        {
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
                 lpEvent != 0;
                 liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
            {
                if (liEventId == 64)
                {
                    GuiCache* lpCache =
                        reinterpret_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                    CGS_ASSERT(lpCache != 0,
                               "Invalid cache in RaceMainHudState::UpdateSetupState");   // cpp:1897
                    mpCache = lpCache;
                }
            }
        }

        if (mpCache == 0)
            return false;   // RACE_MAIN idles in SETUP until the cache event arrives

        const s32 leGameModeType = mpCache->GetGameMode();   // cache +0x9E58

        // The X360 body is a goto ladder: every case writes its own overrides and then
        // jumps into a shared fall-through tail at one of nine entry points (the Hex-Rays
        // listing's LABEL_25..LABEL_33). Reproduced as an entry index plus the same nine
        // blocks in order -- identical control flow, without goto-into-switch.
        s32 liLadderEntry = 25;
        switch (leGameModeType)
        {
        case 0:    // E_MODE_RACE
        case 1:    // E_MODE_FACE_OFF
        case 5:    // E_MODE_BURNING_ROUTE
        case 6:    // E_MODE_ELIMINATOR
            liLadderEntry = 25;
            break;

        case 2:    // E_MODE_OFFLINE_SHOWTIME
            mbPreRaceCountdown   = false;
            mbDistrictMarker     = false;
            mbSatNav             = false;
            mbSatNavStatic       = false;
            mbOnlineTimeoutTimer = false;
            mbAboveCarIcons      = true;
            mbShowTimeBar        = true;
            mbCompass            = false;
            liLadderEntry = 28;
            break;

        case 3:    // E_MODE_ROAD_RAGE
            mbPreRaceCountdown        = false;
            mbPlayerPositionTable     = false;
            mbRoadRuleComponent       = false;
            mbMugShotComponent        = false;
            mbPaybackComponent        = false;
            mbPreRaceCountdownRenders = true;
            mbDistrictMarker          = true;
            mbSatNav                  = true;
            mbSatNavStatic            = true;
            mbAboveCarIcons           = true;
            mbOnlineTimeoutTimer      = true;
            mbShowTimeBar             = false;
            mbCompass                 = false;
            liLadderEntry = 32;
            break;

        case 4:    // E_MODE_PURSUIT
            mbPreRaceCountdown        = false;
            mbPlayerPositionTable     = false;
            mbAboveCarIcons           = false;
            mbOnlineTimeoutTimer      = false;
            mbShowTimeBar             = false;
            mbPreRaceCountdownRenders = true;
            mbDistrictMarker          = true;
            mbSatNav                  = true;
            mbSatNavStatic            = true;
            mbCompass                 = true;
            liLadderEntry = 30;
            break;

        case 7:    // E_MODE_STUNT_ATTACK
            mbPreRaceCountdown        = false;
            mbPreRaceCountdownRenders = false;
            mbPlayerPositionTable     = false;
            mbAboveCarIcons           = false;
            mbRoadRuleComponent       = false;
            mbDistrictMarker          = true;
            mbSatNav                  = true;
            mbSatNavStatic            = true;
            mbOnlineTimeoutTimer      = true;
            mbMugShotComponent        = false;
            mbPaybackComponent        = false;
            mbShowTimeBar             = false;
            mbCompass                 = false;
            liLadderEntry = 32;
            break;

        case 8:    // E_MODE_MARKED_MAN
            mbOnlineTimeoutTimer = false;
            liLadderEntry = 26;
            break;

        case 9:    // E_MODE_TRAFFIC_ATTACK
            mbCompass            = false;
            mbOnlineTimeoutTimer = true;
            liLadderEntry = 27;
            break;

        case 10:   // online race
            mbShowTimeBar             = false;
            mbPreRaceCountdown        = true;
            mbPreRaceCountdownRenders = true;
            mbDistrictMarker          = true;
            mbSatNav                  = true;
            mbSatNavStatic            = true;
            mbPlayerPositionTable     = true;
            mbAboveCarIcons           = true;
            mbOnlineTimeoutTimer      = true;
            mbMugShotComponent        = true;
            mbPaybackComponent        = true;
            mbCompass                 = true;
            liLadderEntry = 31;
            break;

        case 11:   // online road rage
        case 13:   // online burning home run
            mbShowTimeBar             = false;
            mbCompass                 = false;
            mbPreRaceCountdown        = true;
            mbPreRaceCountdownRenders = true;
            mbDistrictMarker          = true;
            mbSatNav                  = true;
            mbSatNavStatic            = true;
            mbPlayerPositionTable     = true;
            mbAboveCarIcons           = true;
            mbOnlineTimeoutTimer      = true;
            mbMugShotComponent        = true;
            mbPaybackComponent        = true;
            liLadderEntry = 31;
            break;

        case 12:   // online stunt run
        case 14:
        case 17:
            mbRoadRuleComponent       = false;
            mbPaybackComponent        = false;
            mbShowTimeBar             = false;
            mbCompass                 = false;
            mbPreRaceCountdown        = true;
            mbPreRaceCountdownRenders = true;
            mbDistrictMarker          = true;
            mbSatNav                  = true;
            mbSatNavStatic            = true;
            mbPlayerPositionTable     = true;
            mbAboveCarIcons           = true;
            mbOnlineTimeoutTimer      = true;
            mbMugShotComponent        = true;
            liLadderEntry = 32;
            break;

        case 15:   // online freeburn lobby -- the ONLY mode that skips LABEL_32, so its
                   // BurnoutSkillz / challenge-ticker / challenge-on flags survive
            mbPreRaceCountdown             = false;
            mbPreRaceCountdownRenders      = false;
            mbShowTimeBar                  = false;
            mbCompass                      = false;
            mbDistrictMarker               = true;
            mbSatNav                       = true;
            mbSatNavStatic                 = true;
            mbPlayerPositionTable          = true;
            mbAboveCarIcons                = true;
            mbOnlineTimeoutTimer           = true;
            mbRoadRuleComponent            = true;
            mbMugShotComponent             = true;
            mbPaybackComponent             = true;
            mbBurnoutSkillz                = true;
            mbFreeburnChallengeTicker      = true;
            mbFreeburnChallengeOnComponent = true;
            liLadderEntry = 33;
            break;

        case 16:   // online showtime
            mbPreRaceCountdown    = false;
            mbDistrictMarker      = false;
            mbOnlineTimeoutTimer  = false;
            mbCompass             = false;
            mbSatNav              = true;
            mbSatNavStatic        = true;
            mbPlayerPositionTable = true;
            mbAboveCarIcons       = true;
            mbShowTimeBar         = true;
            liLadderEntry = 29;
            break;

        default:
            // The X360 streams "Invalid game mode ( <n> ) for this state - should be an on
            // or offline race \n" through the assert StrStream, then falls into LABEL_25.
            CGS_ASSERT(false,
                       "Invalid game mode for this state - should be an on or offline race");   // cpp:1925
            liLadderEntry = 25;
            break;
        }

        if (liLadderEntry <= 25) { mbOnlineTimeoutTimer = true; }                       // LABEL_25
        if (liLadderEntry <= 26) { mbCompass = true; }                                  // LABEL_26
        if (liLadderEntry <= 27)                                                        // LABEL_27
        {
            mbPreRaceCountdown = true;
            mbDistrictMarker   = true;
            mbSatNav           = true;
            mbSatNavStatic     = true;
            mbAboveCarIcons    = false;
            mbShowTimeBar      = false;
        }
        if (liLadderEntry <= 28) { mbPlayerPositionTable = false; }                     // LABEL_28
        if (liLadderEntry <= 29) { mbPreRaceCountdownRenders = false; }                 // LABEL_29
        if (liLadderEntry <= 30)                                                        // LABEL_30
        {
            mbPaybackComponent = false;
            mbMugShotComponent = false;
        }
        if (liLadderEntry <= 31) { mbRoadRuleComponent = true; }                        // LABEL_31
        if (liLadderEntry <= 32)                                                        // LABEL_32
        {
            mbFreeburnChallengeOnComponent = false;
            mbFreeburnChallengeTicker      = false;
            mbBurnoutSkillz                = false;
        }
        // LABEL_33 -- always executed. NOTE mbPreEventOverlay is cleared on EVERY path, so
        // the pre-event overlay arm below is retail-dead unless the debug override is on.
        mbHudMessages                  = true;
        mbBoostBar                     = true;
        mbBoostMessages                = true;
        mbEventInfo                    = true;
        mbFriendsList                  = true;
        mbPreEventOverlay              = false;
        mbB5Ident                      = true;
        mbFreeburnChallengeButtonStart = false;
        mbFreeburnChallengeSelector    = false;

        // The ForceReenter debug override (byte_82FB3C94 / off_82FB3C98). The index order is
        // the console's, not the member order; indices 0 and 18 are unused.
        if (msbDEBUG_OverrideNormalCptStates)
        {
            const bool* lpbStates          = mspbDEBUG_ComponentEnabledStates;
            mbHudMessages                  = lpbStates[1];
            mbBoostBar                     = lpbStates[2];
            mbBoostMessages                = lpbStates[3];
            mbPreRaceCountdown             = lpbStates[4];
            mbPreRaceCountdownRenders      = lpbStates[5];
            mbEventInfo                    = lpbStates[6];
            mbDistrictMarker               = lpbStates[7];
            mbSatNav                       = lpbStates[8];
            mbSatNavStatic                 = lpbStates[9];
            mbPlayerPositionTable          = lpbStates[10];
            mbFriendsList                  = lpbStates[11];
            mbAboveCarIcons                = lpbStates[12];
            mbOnlineTimeoutTimer           = lpbStates[13];
            mbRoadRuleComponent            = lpbStates[14];
            mbPreEventOverlay              = lpbStates[15];
            mbMugShotComponent             = lpbStates[16];
            mbPaybackComponent             = lpbStates[17];
            mbShowTimeBar                  = lpbStates[19];
            mbCompass                      = lpbStates[20];
        }

        const bool lbSatNav = mbSatNav;   // read BEFORE the two dependent clears
        if (!mbSatNav)
            mbSatNavStatic = false;
        if (!mbPreRaceCountdown)
            mbPreRaceCountdownRenders = false;

        // Both arms post the SAME record -- {E_MAPTYPE_GPS, 0.0f, show=false}; the sat-nav
        // arm additionally binds the component and hands it the same payload.
        const SatNavShowHidePayload lSatNavRecord(1, 0.0f, 0);
        if (lbSatNav)
        {
            mSatNavComponent.SetEventType(
                static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                    mpCache->GetGameMode()));
            mSatNavComponent.SetCachePointer(mpCache);
            PostShowHideSatNav24(mpStateInterface, lSatNavRecord);
            mSatNavComponent.RecvEvent(
                reinterpret_cast<const CgsModule::Event*>(&lSatNavRecord), 213);

            // X360 @0x8247A12C: `lwz r11, 0x4060(cache)` (== mpMapIconManager) then
            // `stwx r29(=0), r11, 0xAA08` -- a zero store 0xAA08 bytes into the map-icon
            // manager. [FLAG deferred] BrnGui::MapIconManager is not reconstructed to that
            // depth and nothing in the tree names the field, so guessing a member here
            // would be a fabricated write into a live object.
            // DELETE-WHEN: the MapIconManager recon names +0xAA08.
        }
        else
        {
            PostShowHideSatNav24(mpStateInterface, lSatNavRecord);
        }

        if (mbHudMessages)
        {
            mHudMessageComponent.SetController(mpCache->GetHudMessageController());
            mHudMessageComponent.SetDirector(mpCache->GetHudMessageDirector());
            mHudMessageComponent.SetGameMode(
                static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(leGameModeType));
        }

        PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
        if (mbPlayerPositionTable)
        {
            mPlayerPositionTable.SetCache(mpCache);
        }
        PostCommand16<215>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);

        if (mbRoadRuleComponent)
        {
            mRoadRuleComponent.SetCachePointer(mpCache);
            mRoadRuleComponent.InitialiseMode();
        }

        // The pre-event overlay arm. Retail-dead (LABEL_33 always clears the flag);
        // transcribed for the override path. Gate byte = cache +0xA014, named 2026-08-27.
        if (mbPreEventOverlay && mpCache->IsEventPreparedForModeStart())
        {
            const s32 liDestinationDistrict = mpCache->GetEventDestinationDistrict();
            CGS_ASSERT(KAPC_PRE_EVENT_OVERLAYS[leGameModeType] != 0,
                       "KAPC_PRE_EVENT_OVERLAYS[leGameModeType]");   // cpp:2615 (non-gating)

            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct(KAPC_PRE_EVENT_OVERLAYS[leGameModeType]);
            // The X360 adds the SAME district parameter twice (@0x8247A2CC / @0x8247A2DC),
            // both with id 2 -- faithful, not a transcription slip.
            lWire.mRequest.AddMessageParam(
                2, KAPC_DISTRICT_OVERLAY_MESSAGE_IDS[liDestinationDistrict]);
            lWire.mRequest.AddMessageParam(
                2, KAPC_DISTRICT_OVERLAY_MESSAGE_IDS[liDestinationDistrict]);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(GuiOverlayRequestWire)));

            mbOverlayInProgress    = true;
            meModeOverlayDisplayed = leGameModeType;
            mfOverlayRemovalTime   = mpCache->GetTime() + 2.0f;   // flt_8205AE74 == 2.0f
        }

        // The road-rule-shot panel. Gated on a CACHE byte (+0xAC59), NOT on a component
        // flag -- which is why RoadRuleShotComponent cannot be flag-deferred at the mount.
        if (GuiCache_RoadRuleShotPending(mpCache))
        {
            // The inlined find-record-by-race-car scan: eight 312-byte online player
            // records from cache+0xAC80, comparing each record's meActiveRaceCarIndex
            // (+276 == the console's +0x114 into the +0xAD94 lane) against
            // meRoadRuleShotOpponentARCI (+0xAC48). Same shape as the one
            // RoadRuleShotComponent::Snap @0x82415620 inlines.
            const s32 liOpponentCar = mpCache->GetRoadRuleShotOpponentARCI();
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpOpponent = 0;
            for (s32 liPlayer = 0; liPlayer < 8; ++liPlayer)
            {
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpInfo =
                    mpCache->GetOnlinePlayerInfo(liPlayer);
                if (lpInfo->meActiveRaceCarIndex == liOpponentCar)
                {
                    lpOpponent = lpInfo;
                    break;
                }
            }
            CGS_ASSERT(lpOpponent != 0, "lpOpponentStatusData != NULL");   // cpp:2636 (non-gating)

            // [FLAG deferred] SetupComponent's remaining three arguments come out of the
            // un-named cache pad mPad_AC4C[14]: r7 is an `ldx` (8-byte CgsID) at +0xAC50,
            // r8 a byte at +0xAC58, and the Snap-vs-Show pick a byte at +0xAC5B. Only
            // r5 (+0xAC5A) has a named accessor today. This whole arm is therefore parked
            // by GuiCache_RoadRuleShotPending above rather than half-fed.
            // DELETE-WHEN: BrnGuiCache.h carves mPad_AC4C[14] / mPad_AC5B[1].
            LogDeferredComponent("RoadRuleShotComponent::SetupComponent (cache +0xAC50/58/59/5B)");
        }

        return true;
    }
}

// ============================================================================
// FOLDED FROM BrnRaceMainHudState_wS2.cpp (wave S2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// wave-S2 partfile 01 of BrnRaceMainHudState.cpp -- the RACE_MAIN phase machine
//
//   BrnGui::RaceMainHudState::Update          @0x82481898 (cpp:1855)
//   BrnGui::RaceMainHudState::UpdateLoading   @0x8247A410
//   BrnGui::RaceMainHudState::UpdateWFInit    @0x82480200 (cpp:2819)
//   BrnGui::RaceMainHudState::UpdatePermenant @0x824806E8 (cpp:3620 / 3702 / 3737)
//   BrnGui::RaceMainHudState::RevealHud       @0x8247A4E0
//
// Every one of the five was re-verified against
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x<ADDR>.json -- the `name` field of each JSON
// matches the symbol claimed above -- and every body below is transcribed from the raw
// DISASSEMBLY, arbitrated over Hex-Rays wherever the two disagree (they disagree three
// times; each is called out at its site).
//
// The sibling BrnRaceMainHudState.cpp keeps the class's other bodies (OnEnter, OnLeave,
// UpdateSetupState, UpdateRunning, SetExpectedComponent/SetExpectedAptComponentList,
// SetupEventInfo, the countdown pair, the freeburn tickers) and the static .rdata
// resource table. Nothing in this partfile redeclares any of that; the shared class
// definition is GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h.
//
// WHAT THIS PARTFILE IS FOR. RACE_MAIN's phase machine is NOT a copy of the freeburn
// one. Three differences are load-bearing and each is preserved verbatim:
//   1. Update CASCADES. FBurnMainHudState::Update advances at most one phase per frame
//      (BrnFBurnMainHudState.cpp:542). RACE_MAIN's switch FALLS THROUGH -- SETUPSTATE ->
//      LOADING -> WF_INIT -> RUNNING can all execute in a single frame (the console's own
//      `goto LABEL_3/4/5` chain @0x824818EC..0x8248194C). Copying the freeburn shape here
//      would change the reveal timing by up to three frames.
//   2. UpdatePermenant is GATED (`if (meInternalState != IDLE)`, @0x824819E8); the
//      freeburn state calls its own unconditionally.
//   3. UpdateWFInit blocks on a NON-EMPTY expected-apt-component list. RACE_MAIN's
//      SetExpectedAptComponentList installs exactly one hash -- "EventHud_Animator" --
//      where the freeburn state installs an empty list, so AreAllAptComponentsInitialised
//      here is a real handshake, not a formality. If the apt side never reports that
//      component the HUD loads and never reveals; that is what the [hud-reveal] diagnostic
//      below exists to distinguish from a black screen.
// ===================================================================================


namespace BrnGui
{
    namespace
    {
// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_CHANNEL_VIEW_STATE was dropped here -- this TU defines it once, above)

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

// (fold: an identical definition of GuiCommandEvent16 was dropped here -- this TU defines it once, above)

// (fold: an identical definition of PostCommand16 was dropped here -- this TU defines it once, above)

        // The 24-byte road-rule bring-up record { 8, 327, 16 } + a zeroed 8-byte tail.
        // X360 @0x82480460..0x82480490: `li r11,8 -> var_70`, `li r11,0x147 -> var_6C`,
        // `li r11,0x10 -> var_68`, `std r23(==0) -> var_60`, size 0x18, channel 0x28.
        // FLAG (console-uninitialised word): the console never writes var_64 (payload +12),
        // so its byte-4..7 are whatever the caller's frame held. Zeroed here -- the same
        // choice BrnFBurnMainHudState.cpp:717 already made for the identical record.
        struct GuiEvent327 : public CgsGui::GuiEvent<327>
        {
            s32 miPadA;   // +0x0C -- console-uninitialised (see FLAG above)
            s32 miPadB;   // +0x10 -- console `std r23` low word (0)
            s32 miPadC;   // +0x14 -- console `std r23` high word (0)
            GuiEvent327() : CgsGui::GuiEvent<327>(8, 16), miPadA(0), miPadB(0), miPadC(0) {}
        };

        // (2026-08-27 verify round: the TU-local KAPC_PRE_EVENT_OVERLAYS[18] copy that stood
        // here is DELETED -- it was DEAD by name lookup: inside a RaceMainHudState member,
        // the unqualified name binds the CLASS STATIC declared in BrnRaceMainHudState.h:257
        // and defined in BrnRaceMainHudState.cpp:~340, so an edit here would silently have
        // had no effect. One definition now, the class-static one; both copies held the same
        // image-verified data @0x82F261E0.)

        // The freeburn-challenge selector actions UpdatePermenant's case-573 arm switches on
        // (`lwz r11, 8(r28)`, switch 4 cases @0x824808A0). Only action 2 does anything here.
        const s32 KI_SELECTOR_ACTION_START_TICKER = 2;
    }

    // =======================================================================
    //  Update  @ 0x82481898
    // =======================================================================
    // The cascading phase machine, bracketed by the "HUD state Update" CPU monitor.
    //
    // dword_82F27640 is GuiPerfmons::miHudStateUpdate, not a bare global: GuiPerfmons::
    // Initialise @0x824EF050 stores AddMonitor's handle to 0x82F2763C right after loading
    // "        Gui - HudFlow Update" (@0x824EF19C/0x824EF1C4) and to 0x82F27640 right after
    // loading "          HUD state Update" (@0x824EF1CC/0x824EF1F0) -- so +0x82F27640 is the
    // second of that pair. BrnGuiPerfmons.cpp:90 registers the same monitor.
    //
    // ⭐ THE FALLTHROUGH IS THE POINT. Each case re-stores its own enum value FIRST and then
    // falls into the next case on a true return, so one Update() call can walk the whole
    // ladder. Written as C++ fallthrough because that is literally the console's control
    // flow (`beq loc_824819E4` on false, straight-line otherwise); an if/else chain would
    // read the same but hide why the re-stores exist -- they are what lands the new phase.
    void RaceMainHudState::Update()
    {
        CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miHudStateUpdate);

        switch (meInternalState)
        {
        case E_RACEINTERNALSTATE_SETUPSTATE:
            meInternalState = E_RACEINTERNALSTATE_SETUPSTATE;   // @0x824818EC/F4
            if (!UpdateSetupState())
                break;
            // fall through -- @0x82481904 `beq` only on false
        case E_RACEINTERNALSTATE_LOADING:
            meInternalState = E_RACEINTERNALSTATE_LOADING;      // @0x82481908/10
            if (!UpdateLoading())
                break;
            // fall through -- @0x82481920
        case E_RACEINTERNALSTATE_WF_INIT:
            meInternalState = E_RACEINTERNALSTATE_WF_INIT;      // @0x82481924/2C
            if (!UpdateWFInit())
                break;
            // fall through -- @0x8248193C
        case E_RACEINTERNALSTATE_RUNNING:
            meInternalState = E_RACEINTERNALSTATE_RUNNING;      // @0x82481940/48
            UpdateRunning();
            break;
        case E_RACEINTERNALSTATE_IDLE:
            meInternalState = E_RACEINTERNALSTATE_IDLE;         // @0x82481954/58
            break;
        default:
            {
                // The streamed assert @0x82481960..0x824819E0: the message text, then the
                // offending enum value, then FireAssert at cpp:1855 (`li r5, 0x73F`).
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Should never call update in the following state";
                lStrStream << static_cast<s32>(meInternalState);
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    lacMessage,
                    "..\\..\\..\\GameSource\\Gui/Flow/HUD/States/BrnRaceMainHudState.cpp",
                    1855);
                CgsDev::Assert::EndAssert();
            }
            break;
        }

        // GATED, unlike the freeburn state's unconditional call (@0x824819E4..0x824819F4).
        if (meInternalState != E_RACEINTERNALSTATE_IDLE)
            UpdatePermenant();

        // The pumps read without consuming; the state clears its in-queue at frame end
        // (`lwz r3, 0x18(r29)` == mpInGuiEventQueue, @0x824819F8).
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue != 0)
            lpInQueue->Clear();

        CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miHudStateUpdate);
    }

    // =======================================================================
    //  UpdateLoading  @ 0x8247A410
    // =======================================================================
    // Four statements, and the console asserts NOTHING here (unlike the freeburn twin at
    // BrnFBurnMainHudState.cpp:674, which does assert its cache) -- UpdateSetupState has
    // already refused to advance until mpCache is non-null, so this phase cannot run without
    // one. Kept assert-free to match.
    bool RaceMainHudState::UpdateLoading()
    {
        // @0x8247A424..0x8247A440: r4 = &maResourcesToLoad (unk_82F25F88),
        // r5 = muNumResourcesToLoad (dword_82F25F84 == 21). Hex-Rays drops both operands and
        // prints a one-argument call; the asm is authoritative.
        if (!mpCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
            return false;

        if (mbSatNav)
            mSatNavComponent.LoadResources();   // @0x8247A45C (this + 0x6A0)

        // Mount the HUD apt movie at level 1. The console builds the 20-byte { 8, 18, 12,
        // name, 1 } record by hand (@0x8247A464..0x8247A4B0, channel 0x29) with the name
        // taken from off_82F27BE0[0]; that pointer is verified as "B5RaceHud" both by IDA's
        // own operand comment here and by an image read of 0x82F27BE0. Posted through
        // StateInterface::PlayAptMovie rather than a hand-rolled record for the reason
        // BrnFBurnMainHudState.cpp:684 documents: the record's name field is an 8-byte
        // pointer on x64, so a hardcoded 20-byte post truncates the trailing level number.
        //
        // ⚠️ SEAM: the freeburn state mounts the SAME movie at the SAME level. The HUD flow
        // must have left FBURN_MAIN before RACE_MAIN gets here or the level-1 mount collides.
        mpStateInterface->PlayAptMovie("B5RaceHud", 1);

        SetExpectedAptComponentList();   // @0x8247A4B8 -- installs the "EventHud_Animator" hash
        return true;
    }

    // =======================================================================
    //  UpdateWFInit  @ 0x82480200
    // =======================================================================
    bool RaceMainHudState::UpdateWFInit()
    {
        // THE gate. One hash in the list ("EventHud_Animator"), so this really does wait.
        if (!mpCache->AreAllAptComponentsInitialised(E_GUIFLOW_HUD))
            return false;

        if (mbPaybackComponent)
            mPaybackComponent.Initialize(mpCache);                       // @0x82480244

        // The sat-nav animator is parked visible/invisible UNCONDITIONALLY -- the flag only
        // picks which label, it does not gate the call (@0x82480248..0x8248027C).
        mSatNavAnimationComponent.AddOutputAptViewState(
            "apt_Transition", mbSatNav ? "visible" : "invisible", false);

        if (mbPlayerPositionTable)
            mPlayerPositionTable.SetupGameMode();                        // @0x82480290

        if (mbOnlineTimeoutTimer && mpCache->IsOnlineTimeoutPending())   // @0x824802A8 (+0x13B5C)
            mOnlineTimeoutTimer.Show();

        if (mbFreeburnChallengeButtonStart)
        {
            // @0x824802D0..0x82480320. Hex-Rays renders GetFreeburnChallengeManager with no
            // `this`; the asm passes mpCache in r3. The three reads off the returned manager
            // are `lwz r10, 4(r3)` (meInternalState, tested against the {2,3,4} set) and
            // `lbz r11, 0x18(r3)` (mbIsLocalHost, tested == 1). The START button shows only
            // when the manager is active, NOT already running, and this machine is the host.
            const FreeburnChallengeManager* lpManager = mpCache->GetFreeburnChallengeManager();
            if (lpManager->IsActive() && !lpManager->IsRunning() && lpManager->IsLocalHost())
                mChallengeComponent.Show();
        }

        if (mbFreeburnChallengeSelector)
        {
            // @0x82480338 `stw r3, 0x78FC(r31)` -- +0x78FC is mChallengeSelectorComponent
            // (+0x67A0) + 0x115C, i.e. ChallengeSelector::mpChallengeList. The console
            // inlined SetChallengeList; restored here as the real call.
            mChallengeSelectorComponent.SetChallengeList(mpCache->GetFreeburnChallengeList());
        }

        if (mbFreeburnChallengeTicker)
        {
            // @0x8248034C..0x82480398 -- same manager state word, three-way.
            const FreeburnChallengeManager* lpManager = mpCache->GetFreeburnChallengeManager();
            if (lpManager->IsActive())
                StartFreeburnChallengeTicker();
            else if (lpManager->IsNotActive())
                StartFreeburnChallengeNotActiveTicker();
        }

        if (mbEventInfo)
            SetupEventInfo();                                            // @0x824803AC

        // ---- THE REVEAL LADDER (@0x824803B0..0x82480448) --------------------------------
        // mpCache+0xA014 == mbEventPreparedForModeStart, read as a byte through the
        // materialised offset pair `ori r26, r10, 0xA014 ; lbzx r10, r11, r26`.
        const bool lbInEvent = mpCache->IsEventPreparedForModeStart();

        bool lbRevealNow  = false;
        bool lbImmediate  = false;
        if (!lbInEvent)
        {
            // Not in an event: the countdown is over before it started.
            meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_DONE;   // @0x824803D0
            if (mbPreRaceCountdownRenders)
                mEventCountdownIcon.SetState("invisible");                 // vslot +0xC @0x824803E8
            lbRevealNow = true;
            lbImmediate = true;
        }
        else if (!mbPreRaceCountdown)
        {
            // In an event with no countdown widget: tell the view the countdown is finished
            // ({ 1, 236, 12 }, 16 bytes, channel 0x28 @0x82480404..0x82480428) and transition
            // the HUD in rather than snapping it on.
            //
            // FLAG (console-uninitialised byte): the console writes only the three header
            // words of that record -- its flag byte at +12 is left holding whatever the frame
            // had. Sent as 0 here; the record's consumer reads the id, not the flag.
            PostCommand16<236>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);
            lbRevealNow = true;
            lbImmediate = false;
        }
        else if (mpCache->IsOnlineStartInProgress())                       // @0x82480434 (+0x4B4C)
        {
            lbRevealNow = true;
            lbImmediate = true;
        }
        // else: in an event, countdown armed, offline start -- NO reveal here. The HUD waits
        // for UpdateEventCountdown's "GO" arm to call RevealHud(false).

        // [hud-reveal] RACE_MAIN. NOT X360 -- the PC-side twin of the freeburn state's
        // engine-state diagnostic (BrnFBurnMainHudState.cpp:743). This ladder has four
        // outcomes and three of them look identical from outside (a HUD that is simply not
        // there yet), so print which arm was taken. The fourth -- "waiting for GO" -- is the
        // one that legitimately leaves the screen bare, and the one worth telling apart from
        // an apt-init hang before anyone starts bisecting a black screen.
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[hud-reveal] RACE_MAIN UpdateWFInit inEvent=" << (lbInEvent ? 1 : 0)
                << " preRaceCountdown=" << (mbPreRaceCountdown ? 1 : 0)
                << " onlineStart=" << (mpCache->IsOnlineStartInProgress() ? 1 : 0)
                << (lbRevealNow
                        ? (lbImmediate ? " -> RevealHud(IMMEDIATE)\n" : " -> RevealHud(TRANSIN)\n")
                        : " -> DEFERRED, waiting for the countdown GO\n");
        }

        if (lbRevealNow)
            RevealHud(lbImmediate);                                        // @0x82480448

        if (mbRoadRuleComponent)
        {
            // The road-rule bring-up post, then the show-time latch, then the replay of any
            // rule the cache already has live (@0x82480460..0x82480518).
            GuiEvent327 lEvent;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT, 24);

            // @0x82480494..0x824804B4: `stb r11, 0x646C(r31)` -- +0x646C is
            // mRoadRuleComponent (+0x5F60) + 0x50C == RoadRuleComponent::mbInShowTime, with
            // r11 == 1 only for game modes 2 (offline showtime) and 16 (online showtime).
            // The console inlined SetIfInShowTime; restored here as the real call.
            const s32 liGameMode = mpCache->GetGameMode();
            mRoadRuleComponent.SetIfInShowTime(liGameMode == 2 || liGameMode == 16);

            // The replay sweep. The console runs it as a do/while with the bound assert
            // INSIDE the loop and the `< 2` re-test after it (@0x824804CC..0x82480518), so
            // index 2 is reached, asserted on, and then rejected -- an off-by-one the retail
            // build ships. Reproduced with the assert on the post-increment value, which is
            // what the console tests (`cmpwi r30, 2 ; ble` -> assert when > 2 is false...
            // i.e. it fires only if the index ever exceeded 2, which it cannot here).
            for (s32 leEnumIndex = 0; leEnumIndex < 2; ++leEnumIndex)
            {
                if (mpCache->IsRoadRuleActive(leEnumIndex))
                {
                    mRoadRuleComponent.HandleRoadRuleBegin(
                        static_cast<BrnStreetData::ScoreType>(leEnumIndex));
                }
                CGS_ASSERT(leEnumIndex + 1 <= 2, "leEnumIndex <= E_SCORE_TYPE_COUNT");   // BrnChallengeData.h:56
            }
        }

        // @0x8248051C..0x8248054C: assert the cache, then store the gameplay-HUD gate byte
        // through it REGARDLESS (the assert is non-gating on console).
        CGS_ASSERT(mpCache != 0, "mpCache");                               // cpp:2819
        mpCache->SetGameplayHudActive(true);                               // stb 1 @cache+0x407C

        // @0x82480550..0x82480590: in an event (byte == 1) AND mode 4 (E_MODE_PURSUIT) ->
        // post { 1, 433, 12 } on channel 0x28. Same console-uninitialised flag byte as the
        // 236 post above.
        if (mpCache->IsEventPreparedForModeStart() && mpCache->GetGameMode() == 4)
            PostCommand16<433>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        if (mbFriendsList)
        {
            mFriendsList.SetGuiCachePointer(mpCache);                      // @0x824805AC
            if (mpCache->IsFriendsListChangePending())                     // @0x824805BC (+0xB86D, == 1)
                mFriendsListChangeIcon.ShowNow();
            mFriendsList.AttemptStateRestore();                            // @0x824805D4
        }

        if (mbDistrictMarker)
        {
            // @0x824805E4..0x824805EC: `lbz r11, 0x4B4C(cache) ; stb r11, 0x1062(r31)` --
            // +0x1062 is mDistrictMarker (+0xFFC) + 0x66 == DistrictMarkerComponent::mbOnline.
            // The console inlined SetOnline; restored here as the real call.
            mDistrictMarker.SetOnline(mpCache->IsOnlineStartInProgress());
        }

        if (mbPaybackComponent)
        {
            // @0x824805FC..0x82480620. Gate byte first, then the two argument words; a type
            // word of 3 is skipped outright (that arm never calls).
            if (mpCache->IsPaybackAvailable())
            {
                const s32 liPaybackType = mpCache->GetPaybackAvailableType();
                if (liPaybackType != 3)
                {
                    mPaybackComponent.ShowAvailableInstantly(
                        static_cast<BrnNetwork::EPaybackType>(liPaybackType),
                        static_cast<::EActiveRaceCarIndex>(mpCache->GetPaybackVictimRaceCarIndex()));
                }
            }
        }

        // @0x82480624..0x82480640 -- clear the showtime bounce-boost prompt. unk_820046A7 is
        // the shared empty string (image read at 0x820046A7 == ""), and both glyph arguments
        // are 15 == FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE.
        mbBounceBoostPromptVisible = false;
        mShowtimeBounceBoostButton.SetItem("",
                                           FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                           FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                           false);

        if (mbCompass)
        {
            // @0x82480658..0x82480690. The "lpGuiCache" assert at BrnCompassComponent.h:208
            // belongs to the INLINED CompassComponent::SetGuiCachePointer, not to this
            // function -- its file/line argument names the compass header. Restored as the
            // real call (the assert travels with it).
            mCompass.SetGuiCachePointer(mpCache);
            mCompass.SetVisibility(true, false);
        }

        if (mbFreeburnChallengeSelector)
        {
            // @0x824806A0..0x824806C8. Same shape: the "lpGuiCache" assert at
            // BrnChallengeSelector.h:277 is the inlined ChallengeSelector::
            // SetGuiCachePointer's own, and +0x7900 is that component's mpGuiCache (+0x1160).
            mChallengeSelectorComponent.SetGuiCachePointer(mpCache);
        }

        return true;
    }

    // =======================================================================
    //  RevealHud  @ 0x8247A4E0
    // =======================================================================
    // One-shot on mbHudVisible. The parameter is the DWARF's `lbReveal`, but every use is
    // "snap rather than transition", so it is spelled lbImmediate at the definition: it
    // picks "visible" over "transin" and nothing else.
    void RaceMainHudState::RevealHud(bool lbImmediate)
    {
        if (mbHudVisible)                                  // @0x8247A4F0 (+0x5F54)
            return;
        mbHudVisible = true;

        // THE ENGINE GATE -- the same cache word (+0x4B20) the freeburn state documents at
        // BrnFBurnMainHudState.cpp:735. 0 == E_ENGINE_OFF, 1 == E_ENGINE_ON. With the engine
        // off the whole compose block is skipped and the HUD stays on its invisible frame;
        // note that mbHudVisible has ALREADY been latched by then, so this state never
        // re-tries the compose. That is the console's behaviour, not an oversight to fix.
        if (mpCache->GetPlayerEngineState() == 1)
        {
            const char* const lpcViewState = lbImmediate ? "visible" : "transin";

            // BOTH halves of the "EventHud_Animator" pair: the apt view-state write the
            // movie's ActionScript polls, and the FLAPT goto-and-play.
            mGeneralTransitionComponentApt.AddOutputAptViewState(
                "apt_Transition", lpcViewState, false);                     // @0x8247A548
            mGeneralTransitionComponentFlapt.Run(lpcViewState);             // @0x8247A554

            if (mbBoostBar)
            {
                // @0x8247A564..0x8247A570 -- a single show byte, wrapped onto channel 41.
                GuiEventShowHideBoostBar lShowBoostBar;
                lShowBoostBar.maData[0] = 1;
                mpStateInterface->OutputViewState(lShowBoostBar);
            }

            if (mbSatNav)
            {
                // @0x8247A580..0x8247A5B8. The 12-byte payload is { 1, flt_82001CC0, 1 }:
                // map type 1 (E_MAPTYPE_GPS), fade time 0.0f -- flt_82001CC0 read out of the
                // image, NOT assumed -- and show 1. One record, three consumers: the view
                // channel, the internal-state mirror, and the component itself.
                GuiEventShowHideSatNav lShowSatNav;
                lShowSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS, true, 0.0f);
                mpStateInterface->OutputViewState(lShowSatNav);
                mpStateInterface->OutputInternalState(lShowSatNav);
                mSatNavComponent.RecvEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lShowSatNav), 213);
            }
        }

        // OUTSIDE the engine gate (@0x8247A5BC) -- { 1, 215, 12, 1 }, 16 bytes, channel 0x29.
        // ⭐ [A3 SEAM] +0x15B. This is the ONE consumer of the flag the E1 header carried
        // twice (mbTemporaryReplayIndicator / mbAboveCarIcons at +0x15B/+0x15C, where the
        // console has exactly 25 flag bytes and OnEnter @0x82478EF8 zeroes only
        // +0x150..+0x168). Agent A3 owns that deletion; at the time this partfile was
        // written A3's in-flight edit had already dropped mbTemporaryReplayIndicator from the
        // constructor's initialiser list and kept mbAboveCarIcons, which is also what the s2
        // scout dossier concluded from the flag's mode profile (ON for showtime / road rage /
        // every online mode, OFF for race / face-off / pursuit / burning route / eliminator /
        // stunt attack / marked man / traffic attack). If A3 lands the other name instead,
        // this identifier is the single token that has to change.
        if (mbAboveCarIcons)
            PostCommand16<215>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1);
    }

    // =======================================================================
    //  UpdatePermenant  @ 0x824806E8
    // =======================================================================
    // The SECOND pass over the same in-queue each frame (Update's own phase bodies made the
    // first). Runs in every phase except IDLE.
    void RaceMainHudState::UpdatePermenant()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
            const u8*  lpu8Payload = reinterpret_cast<const u8*>(lpEvent);

            // The console's dispatch is a compare ladder (0x23D / 0x123 / 0x15 / 0x94)
            // followed by an 8-case jumptable over `id - 0x23E` (574..581); flattened here.
            switch (liEventId)
            {
            case 21:
                ProcessAptEvents(lpEvent);                                  // @0x824807DC
                break;

            case 148:
                // @0x8248079C `lbz r11, 0(r28)` -- a BYTE test, not a word. Payload 0 audio-
                // cues and then pauses; anything else is ignored entirely.
                if (lpu8Payload[0] == 0)
                {
                    // X360 @0x824807C0: OutputGuiEvent<GuiAudioEvent> with { 2, 0, -1 } and a
                    // zeroed qword tail. FLAG deferred: the GuiAudioEvent record's field
                    // layout is not homed (BrnGuiDemangledEventTypes.h carries only an opaque
                    // 24-byte placeholder), and BrnFBurnMainHudState.cpp:1252 already parked
                    // the identical post for the identical reason. The PAUSE below is the
                    // load-bearing half and is NOT deferred.
                    // DELETE-WHEN: BrnGui::GuiAudioEvent gets real fields.
                    SendStateEvent("PAUSE");
                }
                break;

            case 291:
            case 320:
                SendStateEvent("PAUSE");                                    // @0x824807C4 (shared arm)
                break;

            case 377:
                // @0x824807F4 `lwz r11, 0(r28)` -- a WORD here (contrast case 148's byte).
                if (lpiPayload[0] == 0 || lpiPayload[0] == 2)
                {
                    if (mbFriendsList)
                        mFriendsList.SaveCurrentState();                    // @0x82480818
                    // @0x8248081C..0x82480898: switch on meGameModeType - 7, 11 cases; the
                    // jumptable's cases 0/5/7/10 (== modes 7, 12, 14, 17) take START_CSTNT,
                    // every other mode takes START_CRASH.
                    switch (mpCache->GetGameMode())
                    {
                    case 7:    // E_MODE_STUNT_ATTACK
                    case 12:
                    case 14:
                    case 17:
                        // ⚠️ A stunt-run crash hands off to CRASHEDSTNT, which is still a
                        // stub in BrnHudStatesLinkStubs.cpp -- so on the stunt-race bring-up
                        // path this transition currently lands nowhere. The console call is
                        // kept EXACTLY as-is: the hole is in the destination state, not here,
                        // and swapping in START_CRASH to "make it work" would hide it.
                        SendStateEvent("START_CSTNT");
                        break;
                    default:
                        SendStateEvent("START_CRASH");
                        break;
                    }
                }
                break;

            case 573:
                // @0x8248089C `lwz r11, 8(r28)` -- the selector action word, 4 cases.
                // Actions 0, 1 and 3 do nothing; action 2 shares the 576 arm; anything else
                // asserts with the value streamed in hex.
                if (lpiPayload[2] == KI_SELECTOR_ACTION_START_TICKER)
                {
                    if (mbFreeburnChallengeTicker)
                        StartFreeburnChallengeTicker();
                }
                else if (lpiPayload[2] < 0 || lpiPayload[2] > 3)
                {
                    char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                    lStrStream << "Unknown freeburn challenge selector action ";
                    lStrStream << lpiPayload[2];   // console formats it "0x%X" (off_82F31944)
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        lacMessage,
                        "..\\..\\..\\GameSource\\Gui/Flow/HUD/States/BrnRaceMainHudState.cpp",
                        3702);
                    CgsDev::Assert::EndAssert();
                }
                break;

            case 574:
                CGS_ASSERT(lpEvent != 0, "lpChallengeEvent");               // cpp:3620 (li r5, 0xE24)
                // @0x824809D4 `lbz r11, 8(r28)` -- a BYTE at +8 here, where case 573 read a
                // word at the same offset. Only a ZERO byte falls into the 576 arm.
                if (lpu8Payload[8] == 0 && mbFreeburnChallengeTicker)
                    StartFreeburnChallengeTicker();
                break;

            case 576:
                if (mbFreeburnChallengeTicker)                              // @0x824809E0
                    StartFreeburnChallengeTicker();
                break;

            case 578:
            case 579:
                // @0x824809F8 (case 578) and @0x82480A68 (case 579) build the SAME
                // ticker-clear record -- byte pair { 0, 1 } -- at two different stack slots
                // and share one post (@0x82480A88). Hex-Rays renders the pair as two
                // differently-shaped writes into one __int64 local; the asm shows two
                // identical `stb r26(==0) ; stb 1` pairs.
                // ⭐ 2026-08-27 verify round: the console queues the FULL 16-byte
                // {2,536,12}+{0,1} wire on CHANNEL 40 (`li r5, 0x28 ; li r6, 0x10`), NOT the
                // raw 2-byte GuiEventTickerClearMessages through OutputGuiEvent (which
                // direct-passes and would land 2 bytes on channel 536 -- a record the ticker
                // consumer never sees, so clears would silently drop and challenge lines
                // would accumulate). TU-local wire struct per the partfile precedent
                // (wS4's GuiTickerClearWire536 / BrnRaceMainHudState.cpp's GuiEvent536).
                if (mbFreeburnChallengeTicker)                              // both gate on +0x167
                {
                    struct GuiTickerClearWire536 : public CgsGui::GuiEvent<536>
                    {
                        u8 mbForceFadeOut;            // +0x0C == 0
                        u8 mbDeleteChallengeMessages; // +0x0D == 1
                        u8 mau8Pad[2];
                        GuiTickerClearWire536()
                            : CgsGui::GuiEvent<536>(2, 12)
                            , mbForceFadeOut(0), mbDeleteChallengeMessages(1)
                        { mau8Pad[0] = mau8Pad[1] = 0; }
                    };
                    GuiTickerClearWire536 lClear;
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lClear), 40, 16);
                }
                break;

            case 581:
                // @0x82480A18..0x82480A64 -- the manager's own state has to be active too.
                if (mbFreeburnChallengeTicker)
                {
                    if (mpCache->GetFreeburnChallengeManager()->IsActive())
                        StartFreeburnChallengeTicker();
                }
                break;

            default:
                break;
            }
        }

        // ---- the pre-event overlay expiry (@0x82480AAC..0x82480B30) ---------------------
        // Retail-dead as shipped: UpdateSetupState clears mbPreEventOverlay on EVERY mode
        // path, so this arm only runs under the debug component-override table. Transcribed
        // anyway -- it is the only recovered consumer of KAPC_PRE_EVENT_OVERLAYS.
        if (mbPreEventOverlay && mbOverlayInProgress
            && mpCache->GetTime() > mfOverlayRemovalTime)
        {
            mbOverlayInProgress = false;                                    // @0x82480AE0
            CGS_ASSERT(KAPC_PRE_EVENT_OVERLAYS[meModeOverlayDisplayed] != 0,
                       "KAPC_PRE_EVENT_OVERLAYS[meModeOverlayDisplayed]");   // cpp:3737 (li r5, 0xE99)

            GuiOverlayWaitFinishRequest lRequest;
            lRequest.Construct(KAPC_PRE_EVENT_OVERLAYS[meModeOverlayDisplayed]);
            mpStateInterface->OutputGuiEvent(lRequest);
        }

        if (mbFriendsList)
        {
            if (mpCache != 0)
            {
                // @0x82480B40..0x82480B70: `stb r11, 0x25F1(r29)` -- +0x25F1 is mFriendsList
                // (+0x1D58) + 0x899 == FriendsListComponent::mabEntryFlags[1], set for game
                // modes 15 and 16 only. No accessor exists for that byte in the ledger, so
                // BrnFriendsList.h grants this state friendship (the same pattern
                // BrnDistrictMarker.h and BrnRoadRuleComponent.h use for the freeburn state).
                // FLAG: the byte's semantics are unrecovered -- it is set, not interpreted.
                const s32 liGameMode = mpCache->GetGameMode();
                mFriendsList.mabEntryFlags[1] =
                    static_cast<u8>((liGameMode == 15 || liGameMode == 16) ? 1 : 0);
            }
            mFriendsList.UpdateAptVariables();                              // @0x82480B78
        }
    }
}

// ============================================================================
// FOLDED FROM BrnRaceMainHudState_wS3.cpp (wave S3) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// BrnRaceMainHudState_wS3.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX -- part-file 2 of the BrnRaceMainHudState TU.
// The sibling part-files are BrnRaceMainHudState.cpp (the .rdata resource table +
// SetExpectedComponent) and the other _wS* part-files of the same wave; NONE of the four
// bodies below is defined anywhere else.
//
//   BrnGui::RaceMainHudState::UpdateRunning            @ 0x8247E898  (781 pseudocode lines)
//   BrnGui::RaceMainHudState::SetupEventInfo           @ 0x82474A60
//   BrnGui::RaceMainHudState::UpdateEventCountdown     @ 0x8247A608
//   BrnGui::RaceMainHudState::ConcludeEventCountdown   @ 0x824748F0
//
// Every offset in the X360 pseudocode below was resolved against BrnRaceMainHudState.h's
// member map (a1+320 == mpCache, a1+368 == mEventInfoComponent, a1+4616 == mEventCountdownIcon,
// a1+4636 == meCurrentEventCountdownState, a1+4640 == mfEventCountdownTimer, ...). Access is
// BY NAME throughout.
//
// ⭐ THE FLAG-BLOCK SHIFT (s2 scout, 2026-08-26). The retail X360 object carries TWENTY-FIVE
// enable bytes at +0x150..+0x168, not the 26 the committed header spells: OnEnter @0x82478EF8
// emits exactly 25 consecutive `stb r30, 0x150(r31)` .. `stb r30, 0x168(r31)`, and this
// function pins three of them at instruction level -- `lbz r11, 0x15C(r31)` gates
// `addi r3,r31,0x5F60 ; bl RoadRuleComponent__HandleLeaveRoadEvent` (so +0x15C IS
// mbRoadRuleComponent, not mbAboveCarIcons), `lbz 0x15F` gates PaybackComponent and
// `lbz 0x163/0x164/0x165/0x166/0x167/0x168` gate OnlineTimeout / Compass / the three
// freeburn-challenge widgets / the challenge-on component. This file therefore spells the
// flags with the CORRECTED names from the scout's table. It compiles against the header
// either way (every name used here exists in both spellings); the byte a flag lands on only
// matters once the header drops the 26th entry, which is the conductor's paired edit --
// see the request list returned with this file.
//
// ⭐ (f) THERE IS EXACTLY ONE EventInfoComponent CALL IN THIS FUNCTION. The whole 63-case
// switch carries NO arm for GuiEventCurrentStatus(492), GuiAttackScoreUpdate(428),
// GuiEventScoreUpdate(424) or GuiEventTimeInfo: the stunt score / multiplier / combo / timer
// readout is PULLED out of GuiCache by EventInfoComponent::Update @0x82435430 (whose only
// xref to UpdateStuntAttack @0x82429C08 is itself), never pushed through this state's GUI
// event queue. The single call is the per-frame tick at 0x8247FFCC:
//     lbz  r11, 0x157(r31)                ; mbEventInfo
//     addi r3, r31, 0x170                 ; &mEventInfoComponent
//     lwz  r4, 0x140(r31)                 ; mpCache
//     bl   BrnGui__EventInfoComponent__Update
//
// COMPONENT DEFERRALS. Same rule and same idiom as the sibling BrnFBurnMainHudState.cpp: an
// arm whose component TU is not on the build (tools/build/build_game_exe.bat) or whose method
// has no declaration yet keeps the X360 gate and control flow verbatim and logs the gap once
// instead of inventing a body. Deferred here: PlayerPositionTableComponent, PaybackComponent,
// OnlineTimeoutComponent, ChallengeSelector, the five FriendsListComponent
// entry points that have no body anywhere, HandleMugshotEvent (not declared -- it is in the
// header's RESIDUE block) and the freeburn challenge-on arm. For E_MODE_STUNT_ATTACK (7)
// UpdateSetupState turns the gate byte OFF for every one of those except the friends list,
// so none of them executes on the stunt-race bring-up path.

namespace BrnGui
{
    namespace
    {
// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KI_CHANNEL_VIEW_STATE was dropped here -- this TU defines it once, above)
        const s32 KI_CHANNEL_INTERNAL_STATE = 42;  // the internal-state mirror channel

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The black-bar / letterbox threshold the case-221 arm compares against
        // (X360 flt_82002138, read out of the XEX image).
        const f32 KF_BLACK_BARS_THRESHOLD = 0.0099999998f;

// (fold: an identical definition of GuiCommandEvent16 was dropped here -- this TU defines it once, above)

// (fold: an identical definition of PostCommand16 was dropped here -- this TU defines it once, above)

        // 20-byte GuiEvent<465> road-rule crash-score record { 8, 465, 12, 0, f32 } -- the
        // per-frame post the road-rule tick makes (X360 @0x8248007C..0x824800B8: the payload
        // pair is stack-built as {0, mfCurrentCrashScore} and copied with one `std`).
        struct GuiRoadRuleCrashEvent20 : public CgsGui::GuiEvent<465>
        {
            s32 miReserved;   // +0x0C (the console's `stw r18` zero)
            f32 mfCrashScore; // +0x10
            explicit GuiRoadRuleCrashEvent20(f32 lfCrashScore)
                : CgsGui::GuiEvent<465>(8, 12), miReserved(0), mfCrashScore(lfCrashScore) {}
        };

        // 24-byte OutputGuiEvent<BrnGui::GuiAudioEvent> WRAPPER record -- header
        // { sizeof(payload)=24, type=456, payload offset=16 } then the 24-byte payload. The
        // countdown's four audio posts differ only in the third payload word (3/2/1/0), which
        // is the countdown step. Shape verbatim from the committed
        // BrnInGameMessagesComponent.cpp:160 GuiAudioEventRecord40 (the same X360 record).
        // ⚠ FLAG payload field NAMES: BrnGui::GuiAudioEvent is still `u8 maPayload[12]` in
        // BrnGuiDemangledEventTypes.h, so the leading words have no recovered names and are
        // spelled by role here rather than forked into that header.
        struct alignas(8) GuiAudioEventRecord40
        {
            s32   miOutEventSize;     // +0x00 = 24
            s32   miOutEventType;     // +0x04 = 456
            s32   miOutEventOffset;   // +0x08 = 16
            s32   miHeaderPad;        // +0x0C (uninitialised on the console)
            s32   miAudioParam0;      // +0x10 = 0 on all four countdown posts
            s32   miAudioParam1;      // +0x14 = 6 (the countdown audio bank)
            s32   miAudioParam2;      // +0x18 = the countdown step 3/2/1/0
            s32   miPayloadPad;       // +0x1C (uninitialised on the console)
            CgsID mAudioId;           // +0x20 = 0

            GuiAudioEventRecord40(s32 liParam0, s32 liParam1, s32 liParam2)
                : miOutEventSize(24), miOutEventType(456), miOutEventOffset(16), miHeaderPad(0)
                , miAudioParam0(liParam0), miAudioParam1(liParam1), miAudioParam2(liParam2)
                , miPayloadPad(0), mAudioId(0)
            {
            }
        };

        void PostCountdownAudio(CgsGui::StateInterface* lpInterface, s32 liStep)
        {
            // X360 @0x8247A734/A79C/A834/A8FC: { 0, 6, liStep } + an 8-byte zero tail.
            GuiAudioEventRecord40 lAudio(0, 6, liStep);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lAudio), KI_CHANNEL_GUI_OUT, 40);
        }

        // ---- GuiCache boundary (the far cache fields this state reads that BrnGuiCache.h
        // does not expose) -------------------------------------------------------------
        // Same idiom as BrnFBurnMainHudState.cpp's cache boundary (:116-:215): one helper per
        // X360 cache field, each carrying the offset so the eventual cache-member naming (or
        // a `friend struct RaceMainHudState;` grant next to the existing friends at
        // BrnGuiCache.h:992) lands exactly here.

        // ---- the cache fields that DO have accessors now -----------------------------
        // The wave's cache carve (BrnGuiCache.h) publishes the in-event gate as
        // IsEventPreparedForModeStart() @+0xA014,
        // GetPlayerRacePosition() @+0x4B24, IsPlayerRacePositionOverridden() @+0x4B25,
        // IsFriendsListOpen() @+0xB86C and GetSatNavZoomLevel() @+0x803C, so this file reads
        // them by name and only the two below still need a stand-in.

        // ⚠ FLAG accessor leaf: the "local player is the online host" byte. The member IS
        // named (GuiCache::mbIsOnlineHost @+0xB864, BrnGuiCache.h:1665) but is private and
        // this class is not one of the cache's friends, so the read is stood in for here
        // rather than forking a second copy of the member.
        // DELETE-WHEN: BrnGuiCache.h publishes `bool IsOnlineHost() const` (or grants
        // `friend struct RaceMainHudState;` beside the friends at :992); the body then
        // becomes `return lpGuiCache->mbIsOnlineHost;`.
        bool GuiCache_IsOnlineHost(const GuiCache* /*lpGuiCache*/)
        {
            return false;
        }

        // ⚠ FLAG accessor leaf: the sat-nav zoom-level WRITE. GuiCache publishes the
        // read (GetSatNavZoomLevel() @+0x803C, BrnGuiCache.h:529) and the "zoom out" step
        // (ZoomSatNavOut()), but the case-6 arm also stores 0 straight into the member and
        // there is no setter for that half.
        // DELETE-WHEN: BrnGuiCache.h publishes `void SetSatNavZoomLevel(s32)`; the body then
        // becomes `lpGuiCache->SetSatNavZoomLevel(liLevel);`.
        void GuiCache_SetSatNavZoomLevel(GuiCache* /*lpGuiCache*/, s32 /*liLevel*/)
        {
        }

// (fold: an identical definition of LogDeferredComponent was dropped here -- this TU defines it once, above)
    }

    // =======================================================================
    //  UpdateRunning  @ 0x8247E898 -- the RUNNING per-frame event dispatch + tick tail
    // =======================================================================
    void RaceMainHudState::UpdateRunning()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

        // X360 head @0x8247E8C0..0x8247E8E4 -- the per-frame sat-nav pre-pass. Both words are
        // INSIDE the component: its player-info binding (component +0x130 == state +0x7D0)
        // and, through its icon manager (component +0x254 == state +0x8F4), the manager's
        // used-icon count (+0x990). The event pump repopulates both during this same frame.
        // ⚠ PAIRED EDIT: needs `friend struct RaceMainHudState;` beside the existing
        // `friend struct FBurnMainHudState;` in BrnSatNavComponent.h:150 and
        // BrnMapIconManager.h:254 -- the freeburn HUD makes the identical pair of stores.
        if (mbSatNav)
        {
            mSatNavComponent.mpPlayerInfo = 0;
            if (mSatNavComponent.mpIconManager != 0)
                mSatNavComponent.mpIconManager->miNumUsedIcons = 0;
        }

        for (; lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
            switch (liEventId)
            {
            case 6:      // controller input pressed
                if (mbFriendsList)
                {
                    mFriendsList.HandleControllerInput(lpiPayload);
                }
                if (mbBurnoutSkillz && lpiPayload[1] == 38 &&
                    !mpCache->IsFriendsListOpen())
                {
                    PostCommand16<543>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                    GuiEventRoadRuleModeRequest lModeRequest;
                    lModeRequest.maData[0] = 0; lModeRequest.maData[1] = 0;
                    lModeRequest.maData[2] = 0; lModeRequest.maData[3] = 0;
                    lModeRequest.maData[4] = 0; lModeRequest.maData[5] = 0;
                    lModeRequest.maData[6] = 0; lModeRequest.maData[7] = 0;
                    mpStateInterface->OutputGuiEvent(lModeRequest);
                }
                if (mbFreeburnChallengeButtonStart)
                    mChallengeComponent.HandleButtonPress(lpiPayload[1]);
                {
                    // X360 @0x8247EFB4: `lwz r11, 4(GetFreeburnChallengeManager(mpCache))`
                    // tested against 3 and 4 -- the manager's own IsRunning/IsShowingResults
                    // pair, folded inline by the compiler.
                    const FreeburnChallengeManager* lpManager =
                        mpCache->GetFreeburnChallengeManager();
                    const bool lbChallengeLive =
                        lpManager != 0 && (lpManager->IsRunning() || lpManager->IsShowingResults());
                    if (lbChallengeLive && !mpCache->IsFriendsListOpen() &&
                        lpiPayload[1] == 38)
                    {
                        PostCommand16<544>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                    }
                }
                if (lpiPayload[1] == 53)
                {
                    // The sat-nav zoom toggle, live only in E_MODE_ONLINE_BURNING_HOME_RUN(13).
                    if (mpCache->GetGameMode() == 13)
                    {
                        if (mpCache->GetSatNavZoomLevel() == 1)
                            GuiCache_SetSatNavZoomLevel(mpCache, 0);
                        else
                            mpCache->ZoomSatNavOut();   // X360 renders this `this`-less (hazard 4)
                    }
                }
                break;
            case 7:      // controller input released
                if (mbFreeburnChallengeButtonStart)
                    mChallengeComponent.HandleButtonRelease(lpiPayload[1]);
                break;
            case 94:
                if (mbFriendsList)
                    mFriendsListChangeIcon.Hide();
                break;
            case 95:
                if (mbFriendsList)
                {
                    mFriendsList.EndWait();
                }
                break;
            case 101:
                if (mbFriendsList)
                    mFriendsList.SetTotalFriends(lpiPayload[0]);
                break;
            case 102:
                if (mbFriendsList)
                {
                    mFriendsList.ProcessNewEntryData(lpEvent);
                }
                break;
            case 103:
                if (mbFriendsList)
                    mFriendsList.RequestRefreshedData();
                break;
            case 104:
                if (mbFriendsList && mpCache->IsOnlineStartInProgress() &&
                    GuiCache_IsOnlineHost(mpCache))
                {
                    mFriendsList.ReshowShortcuts();
                }
                break;
            case 106:
                if (mbFriendsList && !mpCache->IsFriendsListOpen())
                    mFriendsListChangeIcon.AnimateIn();
                break;
            case 108:
                if (mbOnlineTimeoutTimer)
                {
                    // FLAG deferred: OnlineTimeoutComponent::SetTime @0x824157B0 is neither
                    // declared in BrnOnlineTimeoutTimerComponent.h nor on the build.
                    LogDeferredComponent("OnlineTimeoutComponent::SetTime");
                }
                break;
            case 154:
                if (mbHudMessages)
                    mHudMessageComponent.AddMessage(lpEvent);
                break;
            case 156:
                if (mbHudMessages)
                    mHudMessageComponent.TerminateMessages();
                break;
            case 177:
                if (mbPaybackComponent)
                {
                    // FLAG deferred: BrnPaybackComponent.cpp is not on the build
                    // (BeginAwardAnimation(payload[2], payload[1]) @0x8243E148).
                    LogDeferredComponent("PaybackComponent::BeginAwardAnimation");
                }
                break;
            case 179:
            case 180:
                if (mbPaybackComponent)
                {
                    // FLAG deferred: PaybackComponent::BecomeInvisible @0x8241FFE8 -- TU off the build.
                    LogDeferredComponent("PaybackComponent::BecomeInvisible");
                }
                break;
            case 182:   // hide the event HUD
            {
                mGeneralTransitionComponentApt.AddOutputAptViewState("apt_Transition", "invisible", false);
                mGeneralTransitionComponentFlapt.Run("invisible");
                if (lpiPayload[0] != 1)
                {
                    GuiEventShowHideBoostBar lBoostBar;
                    lBoostBar.maData[0] = 0;
                    mpStateInterface->OutputViewState(lBoostBar);
                }
                break;
            }
            case 183:   // show the event HUD
            {
                mGeneralTransitionComponentApt.AddOutputAptViewState("apt_Transition", "visible", false);
                mGeneralTransitionComponentFlapt.Run("visible");
                GuiEventShowHideBoostBar lBoostBar;
                lBoostBar.maData[0] = 1;
                mpStateInterface->OutputViewState(lBoostBar);
                break;
            }
            case 199:
            case 200:
                UpdateSatNav(lpEvent, liEventId);
                break;
            case 205:   // show/hide satnav passthrough: view record + the satnav mirror
            {
                GuiEventShowHideSatNav lShowHide;
                lShowHide.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS,
                                    *reinterpret_cast<const u8*>(lpEvent) != 0, 0.0f);
                mpStateInterface->OutputViewState(lShowHide);
                if (mbSatNav)
                {
                    mSatNavComponent.RecvEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lShowHide), 213);
                }
                break;
            }
            case 206:
                ProcessBoostInfo(lpEvent);
                break;
            case 221:   // the black-bars / letterbox amount (f32)
            {
                // X360 @0x8247F1B0: the whole arm is gated on NOT being in an event.
                if (!mpCache->IsEventPreparedForModeStart())
                {
                    const f32 lfBlackBars = *reinterpret_cast<const f32*>(lpEvent);
                    if (mfBlackBarsCurrentValue != lfBlackBars)
                    {
                        u8 lu8Show;
                        if (lfBlackBars >= KF_BLACK_BARS_THRESHOLD)
                        {
                            mGeneralTransitionComponentApt.AddOutputAptViewState(
                                "apt_Transition", "invisible", false);
                            mGeneralTransitionComponentFlapt.Run("invisible");
                            GuiEventShowHideBoostBar lBoostBar;
                            lBoostBar.maData[0] = 0;
                            mpStateInterface->OutputViewState(lBoostBar);
                            lu8Show = 0;
                        }
                        else
                        {
                            mGeneralTransitionComponentApt.AddOutputAptViewState(
                                "apt_Transition", "visible", false);
                            mGeneralTransitionComponentFlapt.Run("visible");
                            GuiEventShowHideBoostBar lBoostBar;
                            lBoostBar.maData[0] = 1;
                            mpStateInterface->OutputViewState(lBoostBar);
                            lu8Show = 1;
                        }
                        GuiEventShowHideSatNav lShowHide;
                        lShowHide.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS,
                                            lu8Show != 0, 0.0f);
                        mpStateInterface->OutputViewState(lShowHide);
                        mpStateInterface->OutputInternalState(lShowHide);
                        if (mbSatNav)
                        {
                            mSatNavComponent.RecvEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lShowHide), 213);
                        }
                        mfBlackBarsCurrentValue = *reinterpret_cast<const f32*>(lpEvent);
                    }
                }
                break;
            }
            case 222:   // the PP toggle
                if (mbB5Ident)
                {
                    CGS_ASSERT(lpEvent != 0, "lpPPToggle");   // cpp:1301 (non-gating)
                    if (lpiPayload[0] == 1)
                        mIdentAnimator.Run("transIn");
                    else
                        mIdentAnimator.Run("invisible");
                }
                break;
            case 226:
                PostCommand16<60>(mpStateInterface, KI_CHANNEL_VIEW_STATE);
                break;
            case 227:
                PostCommand16<61>(mpStateInterface, KI_CHANNEL_VIEW_STATE);
                break;
            case 234:
                UpdateEventCountdown(lpEvent);
                break;
            case 239:
                if (mbPlayerPositionTable)
                {
                    // FLAG deferred: BrnPlayerPositionTable.cpp is not on the build
                    // (UpdatePositionDetails @0x82441260).
                    LogDeferredComponent("PlayerPositionTableComponent::UpdatePositionDetails");
                }
                break;
            case 325:
                if (mbMugShotComponent)
                {
                    // FLAG deferred: HandleMugshotEvent @0x82475CD0 is in the header's
                    // RESIDUE block -- its GuiMugshotControlEvent parameter type is
                    // ODR-forked between GameBridgeNetworkToX.h and
                    // BrnGuiDemangledEventTypes.h, so it has no declaration to call.
                    LogDeferredComponent("RaceMainHudState::HandleMugshotEvent");
                }
                break;
            case 333:   // GuiEventRoadRuleEnter
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.HandleEnterRoadEvent(
                        reinterpret_cast<const GuiEventRoadRuleEnter*>(lpEvent));
                break;
            case 335:   // road-rule begin { ScoreType }
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.HandleRoadRuleBegin(
                        static_cast<BrnStreetData::ScoreType>(lpiPayload[0]));
                mbBounceBoostPromptNeeded = false;   // X360: the store is OUTSIDE the gate
                break;
            case 336:   // GuiEventRoadRuleEnd
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.HandleRoadRuleEnd(
                        reinterpret_cast<const GuiEventRoadRuleEnd*>(lpEvent));
                mbBounceBoostPromptNeeded = false;
                break;
            case 338:   // rule-time update { f32 time, .., f32 crashTarget, s32 multiplier }
                if (mbRoadRuleComponent)
                {
                    const f32* lpfPayload = reinterpret_cast<const f32*>(lpEvent);
                    mRoadRuleComponent.UpdateCurrentTime(lpfPayload[0]);
                    // X360 @0x8247F55C..: the crash-target pair rides the same record; a
                    // changed multiplier nudges the target by +0.01 so the eased readout
                    // re-renders. This is RoadRuleComponent::UpdateCurrentCrash inlined --
                    // it is spelled as the two named stores because that method has no body
                    // in the tree, exactly as BrnFBurnMainHudState.cpp:1050 spells it.
                    // ⚠ PAIRED EDIT: needs `friend struct RaceMainHudState;` beside
                    // `friend struct FBurnMainHudState;` at BrnRoadRuleComponent.h:51.
                    const s32 liNewMultiplier = lpiPayload[4];
                    mRoadRuleComponent.mfTargetCrashScore = lpfPayload[3];
                    if (mRoadRuleComponent.miCrashMultiplier != liNewMultiplier)
                    {
                        mRoadRuleComponent.miCrashMultiplier  = liNewMultiplier;
                        mRoadRuleComponent.mfTargetCrashScore = lpfPayload[3] + KF_BLACK_BARS_THRESHOLD;
                    }
                }
                break;
            case 339:   // GuiEventRoadRuleUpdateTargetScores
                if (mbRoadRuleComponent)
                {
                    CGS_ASSERT(lpEvent != 0, "lpRRTargetUpdate");   // cpp:1018 (non-gating)
                    mRoadRuleComponent.HandleRoadRuleTargetUpdate(
                        reinterpret_cast<const GuiEventRoadRuleUpdateTargetScores*>(lpEvent));
                }
                break;
            case 340:   // road-rule leave { CgsID }
                // Hazard 4: the asm is `addi r3,r31,0x5F60 ; ld r4,0(r29)` -- ONE 64-bit
                // payload load, not the two s32s Hex-Rays renders.
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.HandleLeaveRoadEvent(
                        *reinterpret_cast<const CgsID*>(lpEvent));
                mbBounceBoostPromptNeeded = false;
                break;
            case 341:   // GuiEventRoadRuleUpcomingRoads
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.HandleUpcomingRoadEvent(
                        reinterpret_cast<const GuiEventRoadRuleUpcomingRoads*>(lpEvent));
                break;
            case 343:   // road-rule mode change { EActiveRoadRule }
                if (mbRoadRuleComponent)
                    mRoadRuleComponent.SwitchModes(
                        static_cast<BrnGameState::EActiveRoadRule>(lpiPayload[0]));
                break;
            case 379:   // the HUD transin / transout pair (player engine state)
            {
                CGS_ASSERT(static_cast<u32>(lpiPayload[0]) < 2u,
                           "( GuiPlayerEngineEvent::E_ENGINE_OFF == lpEngineChange->meNewEngineState )"
                           " || ( GuiPlayerEngineEvent::E_ENGINE_ON == lpEngineChange->meNewEngineState )");   // cpp:1159
                GuiEventShowHideSatNav lShowHide;
                if (lpiPayload[0] == 0)
                {
                    mGeneralTransitionComponentApt.AddOutputAptViewState("apt_Transition", "transout", false);
                    mGeneralTransitionComponentFlapt.Run("transout");
                    PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
                    lShowHide.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS, false, 0.0f);
                    mpStateInterface->OutputViewState(lShowHide);
                    mpStateInterface->OutputInternalState(lShowHide);
                }
                else if (lpiPayload[0] == 1)
                {
                    mGeneralTransitionComponentApt.AddOutputAptViewState("apt_Transition", "transin", false);
                    mGeneralTransitionComponentFlapt.Run("transin");
                    PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1);
                    lShowHide.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS, true, 0.0f);
                    mpStateInterface->OutputViewState(lShowHide);
                    mpStateInterface->OutputInternalState(lShowHide);
                }
                // X360 @0x8247F318: the satnav mirror is OUTSIDE the if/else.
                if (mbSatNav)
                {
                    mSatNavComponent.RecvEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lShowHide), 213);
                }
                break;
            }
            case 218:
            case 364: case 365: case 367: case 368:
            case 382: case 383: case 384: case 385: case 386: case 387:
            case 388: case 389: case 390: case 391: case 394:
            case 400: case 401:
                // X360 LABEL_120 -- the whole boost-message event family routes through the
                // manager with the LIVE event id. Note there is NO mbBoostMessages gate here;
                // only the per-frame Update below is gated.
                CGS_ASSERT(mpCache != 0, "mpCache != NULL");   // cpp:839 (non-gating)
                mBoostMessageManager.RecvEvent(lpEvent, liEventId, mpCache);
                break;
            case 398:
                mbBounceBoostPromptNeeded = (lpiPayload[0] != 0);
                break;
            case 573:   // freeburn challenge selector action
                if (lpiPayload[2] == 2 || lpiPayload[2] == 3)
                {
                    if (mbFreeburnChallengeSelector)
                    {
                        // FLAG deferred: BrnChallengeSelector.cpp / _wL_01.cpp are not on the
                        // build and their mount has three unresolved residuals
                        // (ChallengeList::GetChallengeCount, ChallengeListEntry::
                        // GetNumPlayers / GetDescriptionStringID). Action 2 =
                        // SetAvailableChallenges(cache->muChallengeSlotMirror) +
                        // SelectAvailableChallengeByID(*payload, false); action 3 = Hide().
                        LogDeferredComponent("ChallengeSelector::(action 2/3)");
                    }
                }
                else if (lpiPayload[2] > 3)
                {
                    CGS_ASSERT(false, "Unknown freeburn challenge selector action");   // cpp:1478 (streamed)
                }
                break;
            case 574:
                CGS_ASSERT(lpEvent != 0, "lpChallengeEvent");   // cpp:1361 (non-gating)
                if (*(reinterpret_cast<const u8*>(lpEvent) + 8) != 0)
                {
                    if (mbFreeburnChallengeButtonStart)
                        mChallengeComponent.Show();
                }
                else if (mbFreeburnChallengeSelector)
                {
                    CGS_ASSERT(mpCache != 0, "mpCache");   // cpp:1379 (non-gating)
                    // FLAG deferred (X360 @0x8247FCDC): SetAvailableChallenges(
                    // mpCache->muChallengeSlotMirror @+0xAC78) then
                    // SelectAvailableChallengeByID(the CgsID at payload+0 -- `ld r4,0(r29)`,
                    // a 64-bit load Hex-Rays renders as the 32-bit v4[1]), lbSelect false.
                    LogDeferredComponent("ChallengeSelector::SelectAvailableChallengeByID");
                }
                break;
            case 576:
                if (mbFreeburnChallengeButtonStart)
                    mChallengeComponent.Hide();
                if (mbFreeburnChallengeSelector)
                {
                    // FLAG deferred: `if (selector.IsVisible()) selector.Hide();`
                    LogDeferredComponent("ChallengeSelector::Hide");
                }
                break;
            case 578:
                if (mbFreeburnChallengeSelector)
                {
                    // FLAG deferred: the same Hide, additionally gated on the cache's
                    // online-host byte (X360 `!*(mpCache + 47204)`).
                    LogDeferredComponent("ChallengeSelector::Hide");
                }
                break;
            case 582:
                if (mbFreeburnChallengeSelector)
                {
                    CGS_ASSERT(lpEvent != 0, "lpShowChallengeSelectorEvent");   // cpp:887 (non-gating)
                    // FLAG deferred: SetAvailableChallenges -> GetAvailableChallengeCount>0
                    // -> Show + SelectAvailableChallengeByID/SelectAvailableChallenge.
                    LogDeferredComponent("ChallengeSelector::Show");
                }
                break;
            case 583:
                if (mbFreeburnChallengeTicker)
                    StartFreeburnChallengeNotActiveTicker();
                break;
            case 584:
                // ⭐ 2026-08-27 verify round: the FULL 16-byte {2,536,12}+{0,1} wire on
                // CHANNEL 40 (console `li r5, 0x28 ; li r6, 0x10`), not the raw 2-byte
                // record through OutputGuiEvent (2 bytes on channel 536 = a clear the
                // ticker consumer never sees). TU-local wire per the partfile precedent.
                if (mbFreeburnChallengeTicker)
                {
                    struct GuiTickerClearWire536 : public CgsGui::GuiEvent<536>
                    {
                        u8 mbForceFadeOut;            // +0x0C == 0
                        u8 mbDeleteChallengeMessages; // +0x0D == 1
                        u8 mau8Pad[2];
                        GuiTickerClearWire536()
                            : CgsGui::GuiEvent<536>(2, 12)
                            , mbForceFadeOut(0), mbDeleteChallengeMessages(1)
                        { mau8Pad[0] = mau8Pad[1] = 0; }
                    };
                    GuiTickerClearWire536 lClear;
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lClear), 40, 16);
                }
                break;
            default:
                break;
            }
        }

        // ---- the per-frame component ticks (X360 @0x8247FE54..0x824801EC) --------------
        if (mbDistrictMarker)
        {
            // The marker's own per-frame method: an ICF fold of an EMPTY body on console
            // (the pseudocode's BaseCollisionGenerator::Destruct @0x8284CB38 decompiles to
            // `{ ; }`) -- hazard 6, do NOT reconstruct a collision call here.
            mDistrictMarker.Update();

            GuiEventChangeDistrict lRecord;
            lRecord.meCounty    = mpCache->GetChangeDistrictCounty();
            lRecord.meDistrict  = mpCache->GetChangeDistrictDistrict();
            lRecord.mu8Consumed = mpCache->IsChangeDistrictConsumed() ? 1 : 0;
            lRecord.maPad[0] = lRecord.maPad[1] = lRecord.maPad[2] = 0;
            if (!lRecord.mu8Consumed ||
                (mbFirstFrame && lRecord.meDistrict != BrnWorld::E_DISTRICT_INVALID))
            {
                mDistrictMarker.SetCounty(static_cast<BrnWorld::ECounty>(lRecord.meCounty));
                mDistrictMarker.SetDistrict(static_cast<BrnWorld::EDistrict>(lRecord.meDistrict));
                lRecord.mu8Consumed = 1;
                mpCache->RecEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord), 169);
                mbFirstFrame = false;
            }
        }

        // The position indicator (X360 @0x8247FEE0..0x8247FF40). r4 carries the cache's
        // position byte into BOTH calls: the zero-position path falls straight through to
        // SetVisible(false), and the loaded path re-reads the component's own pending
        // trans-in latch between SetPosition and SetVisible(true).
        // ⚠ PAIRED EDIT: mPositionIndicatorComponent.mbFirstFrame is private -- needs
        // `friend struct RaceMainHudState;` in BrnPositionIndicator.h (the class has no
        // friend list yet; add one beside the member block).
        {
            const s32 liPosition = mpCache->GetPlayerRacePosition();
            if (liPosition == 0)
            {
                mPositionIndicatorComponent.SetVisible(false);
            }
            else if (mpCache->IsPlayerRacePositionOverridden() ||
                     (mPositionIndicatorComponent.mbFirstFrame && liPosition > 0 && liPosition <= 8))
            {
                mPositionIndicatorComponent.SetPosition(liPosition);
                if (mPositionIndicatorComponent.mbFirstFrame)
                    mPositionIndicatorComponent.SetVisible(true);
            }
        }

        // The showtime bounce-boost help item (X360 @0x8247FF44..0x8247FFC0).
        if (mbShowTimeBar)
        {
            if (mbBounceBoostPromptNeeded)
            {
                if (!mbBounceBoostPromptVisible)
                {
                    mShowtimeBounceBoostButton.SetItem(
                        "$HINT_SHOWTIME_GROUND_BREAK",
                        FlaptButtonIconComponent::E_PADBUTTON_SELECT,
                        FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                        false);
                    mbBounceBoostPromptVisible = true;
                }
            }
            else if (mbBounceBoostPromptVisible)
            {
                // X360 &unk_820046A7 -- the empty string (image byte 0x00), the "clear it" call.
                mShowtimeBounceBoostButton.SetItem(
                    "",
                    FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                    FlaptButtonIconComponent::E_PADBUTTON_INVISIBLE,
                    false);
                mbBounceBoostPromptVisible = false;
            }
        }

        // ⭐ THE ONE EventInfoComponent CALL (X360 @0x8247FFCC). The stunt-run score /
        // multiplier / banked-combo / event-timer readout rides this single per-frame tick;
        // there is no event-switch arm feeding it.
        if (mbEventInfo)
            mEventInfoComponent.Update(mpCache);

        if (mbOnlineTimeoutTimer)
        {
            // FLAG deferred: OnlineTimeoutComponent::Update @0x8242C1E0 is neither declared
            // in BrnOnlineTimeoutTimerComponent.h nor on the build.
            LogDeferredComponent("OnlineTimeoutComponent::Update");
        }
        if (mbSatNav)
            mSatNavComponent.Update();
        if (mbBoostMessages)
        {
            // X360 @0x8248000C: `lfs f1, 0(mpCache)` == GuiCache::mfTimeStep (GetTimeStep),
            // and `lbz r5, 0x160(this)` == mbShowTimeBar -- the showtime flag IS the second
            // argument, so in showtime the manager runs only its showtime ticker.
            mBoostMessageManager.Update(mpCache->GetTimeStep(), mbShowTimeBar);
        }
        if (mbHudMessages)
            mHudMessageComponent.Update();
        if (mbFriendsList)
        {
            mFriendsList.Update();
        }
        if (mbRoadRuleComponent)
        {
            mRoadRuleComponent.Update(mpCache->GetTime());
            // Hazard 5: the operand is an `lvx128 v1, mpCache, 0x4AE0` that Hex-Rays drops
            // entirely -- the world-camera position. Same recipe as
            // BrnFBurnMainHudState.cpp:1290.
            const Vector4& lv4Camera = mpCache->GetWorldCameraPosition();
            Vector3 lv3Camera;
            lv3Camera.x = lv4Camera.x;
            lv3Camera.y = lv4Camera.y;
            lv3Camera.z = lv4Camera.z;
            lv3Camera.w = lv4Camera.w;
            mRoadRuleComponent.UpdateRoadSignDistances(lv3Camera);

            GuiRoadRuleCrashEvent20 lCrash(mRoadRuleComponent.mfCurrentCrashScore);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lCrash), KI_CHANNEL_GUI_OUT, 20);
        }
        if (mbPaybackComponent)
        {
            // FLAG deferred: PaybackComponent::Update @0x8241FF38 -- TU off the build.
            LogDeferredComponent("PaybackComponent::Update");
        }
        if (mbCompass)
        {
            mCompass.Update();
        }
        if (mbFreeburnChallengeOnComponent)
        {
            // FLAG deferred: the challenge-on arm. X360 @0x824800F8..0x824801E4:
            //   assert(cache->mpChallengeManager, "mpChallengeManager", BrnGuiCache.h:2390)
            //   if (manager->IsRunning() || manager->IsShowingResults())
            //       lbShow = (manager->GetCurrentAction()->GetTimeLimit() <= 0.0f);
            //   else lbShow = false;
            //   if (mbChallengeOnShowing != lbShow) {
            //       mbChallengeOnShowing = lbShow;
            //       mChallengeOnComponent.SetState(lbShow ? "transin" : "invisible");
            //   }
            // Deferred because the +0x40 read is ChallengeListEntryAction::GetTimeLimit,
            // which is declared-only in SharedClasses/DataLists/ChallengeListEntry.h (no
            // body anywhere), and GuiCache::mpChallengeManager is private to this class.
            // mbFreeburnChallengeOnComponent is 0 for every offline mode including
            // E_MODE_STUNT_ATTACK, so this arm is dead on the bring-up path.
            LogDeferredComponent("RaceMainHudState::(freeburn challenge-on arm)");
        }

        ConcludeEventCountdown();
    }

    // =======================================================================
    //  SetupEventInfo  @ 0x82474A60
    // =======================================================================
    // Ten lines: assert the cache and the enable flag, publish the live game mode to the
    // event-info panel and -- for every mode except 15 (the freeburn lobby) -- run its
    // "transin". The mode word is GuiCache::meGameModeType (X360 cache+40536).
    void RaceMainHudState::SetupEventInfo()
    {
        CGS_ASSERT(mpCache != 0, "mpCache");           // cpp:3804 (non-gating)
        CGS_ASSERT(mbEventInfo, "mbEventInfo");        // cpp:3805 (non-gating)

        const s32 liGameMode = mpCache->GetGameMode();
        mEventInfoComponent.SetEventType(
            static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(liGameMode));
        if (liGameMode != 15)
            mEventInfoComponent.MoveAnimation("transin");
    }

    // =======================================================================
    //  UpdateEventCountdown  @ 0x8247A608
    // =======================================================================
    // The pre-event 3 / 2 / 1 / GO ladder. The WHOLE body is gated on mbPreRaceCountdown, so
    // for E_MODE_STUNT_ATTACK (mode 7, where UpdateSetupState clears the flag) this is a
    // no-op. meCurrentEventCountdownState is a strictly DESCENDING ratchet: each arm runs only
    // while the state is still above the value it is about to write, so a repeated or
    // out-of-order countdown event cannot walk the icon backwards.
    void RaceMainHudState::UpdateEventCountdown(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event passed to RaceMainHudState::UpdateEventCountdown");   // cpp:3252 (streamed; non-gating)

        if (!mbPreRaceCountdown)
            return;

        const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
        switch (lpiPayload[0])
        {
        case 3:
            if (meCurrentEventCountdownState > E_EVENT_COUNTDOWN_STATE_THREE)
            {
                meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_THREE;
                if (mbPreRaceCountdownRenders)
                    mEventCountdownIcon.SetState("three");
                PostCountdownAudio(mpStateInterface, 3);
            }
            break;
        case 2:
            if (meCurrentEventCountdownState > E_EVENT_COUNTDOWN_STATE_TWO)
            {
                meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_TWO;
                if (mbPreRaceCountdownRenders)
                    mEventCountdownIcon.SetState("two");
                PostCountdownAudio(mpStateInterface, 2);
            }
            break;
        case 1:
            if (meCurrentEventCountdownState > E_EVENT_COUNTDOWN_STATE_ONE)
            {
                meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_ONE;
                if (mbPreRaceCountdownRenders)
                    mEventCountdownIcon.SetState("one");
                CGS_ASSERT(mpCache != 0, "mpCache");   // cpp:3336 (non-gating)
                mfEventCountdownTimer = mpCache->GetTime();
                PostCountdownAudio(mpStateInterface, 1);
            }
            break;
        case 0:
            if (meCurrentEventCountdownState > E_EVENT_COUNTDOWN_STATE_GO)
            {
                meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_GO;
                if (mbPreRaceCountdownRenders)
                    mEventCountdownIcon.SetState("go");

                // ⭐ HAZARD 7 -- an EXACT float sentinel, deliberately not epsilon'd. OnEnter
                // leaves mfEventCountdownTimer at 0.0f, and 0.0f is the "the ONE step never
                // arrived" marker: in that case the timer is back-dated by one second so the
                // reflection below still yields a sane GO deadline. Then the timer is
                // REFLECTED about now (`fmsubs f0, f1, 2.0, f13` == now*2 - then), turning
                // "the moment the countdown reached ONE" into "the moment the GO banner should
                // retire" -- ConcludeEventCountdown waits for it. Do not "fix" either line.
                if (mfEventCountdownTimer == 0.0f)
                    mfEventCountdownTimer = mpCache->GetTime() - 1.0f;
                CGS_ASSERT(mpCache != 0, "mpCache");   // cpp:3369 (non-gating)
                mfEventCountdownTimer = (mpCache->GetTime() * 2.0f) - mfEventCountdownTimer;

                PostCountdownAudio(mpStateInterface, 0);
                RevealHud(false);
                PostCommand16<236>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                PostCommand16<533>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            }
            break;
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            break;
        default:
            CGS_ASSERT(false,
                       "Unexpected Countdown State in RaceMainHudState::UpdateEventCountdown");   // cpp:3392
            break;
        }
    }

    // =======================================================================
    //  ConcludeEventCountdown  @ 0x824748F0
    // =======================================================================
    // Called unconditionally at the end of every UpdateRunning frame. Once the GO banner's
    // reflected deadline passes, retire the icon and disarm the timer with -1.0f (the console's
    // "already concluded" value -- distinct from the 0.0f OnEnter sentinel the GO arm reads).
    void RaceMainHudState::ConcludeEventCountdown()
    {
        if (mbPreRaceCountdown && meCurrentEventCountdownState == E_EVENT_COUNTDOWN_STATE_GO)
        {
            CGS_ASSERT(mpCache != 0, "mpCache");   // cpp:3423 (non-gating)
            if (mpCache->GetTime() >= mfEventCountdownTimer)
            {
                const bool lbRenders = mbPreRaceCountdownRenders;
                meCurrentEventCountdownState = E_EVENT_COUNTDOWN_STATE_DONE;
                if (lbRenders)
                    mEventCountdownIcon.SetState("invisible");
                mfEventCountdownTimer = -1.0f;
            }
        }
    }
}

// ============================================================================
// FOLDED FROM BrnRaceMainHudState_wS4.cpp (wave S4) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// wave-S4 partfile of BrnRaceMainHudState.cpp -- the five RACE_MAIN helper bodies the
// mounted sibling part-files CALL but that were bodied NOWHERE in the tree (the RMH
// verifier's link list). Every one is declared in BrnRaceMainHudState.h:200/201/205/210/211.
//
//   BrnGui::RaceMainHudState::ProcessBoostInfo                      @0x82474550 (DWARF .cpp:2937)
//   BrnGui::RaceMainHudState::ProcessAptEvents                      @0x82474638 (DWARF .cpp:2964)
//   BrnGui::RaceMainHudState::UpdateSatNav                          @0x82474830 (DWARF .cpp:3261)
//   BrnGui::RaceMainHudState::StartFreeburnChallengeTicker          @0x8247A9C0 (DWARF .cpp:4085)
//   BrnGui::RaceMainHudState::StartFreeburnChallengeNotActiveTicker @0x8247AF38 (DWARF .cpp:4185)
//
// Each JSON's `name` field was checked against the symbol claimed above before a line was
// written, and every body is transcribed from the raw DISASSEMBLY -- Hex-Rays is arbitrated
// against wherever the two disagree. It disagrees three times here and each is called out
// at its site:
//   (1) UpdateSatNav / ProcessAptEvents: Hex-Rays renders the SatNavComponent receiver as
//       `v3 + 424` in one function and `v4 + 1696` in the other for the SAME member. The asm
//       is `addi r3, r<this>, 0x6A0` in BOTH -- 424 is the dropped `(_DWORD *)` cast. The
//       member is mSatNavComponent (header PINNED +0x6A0) either way.
//   (2) ProcessAptEvents' mbEventInfo arm calls a function IDA names
//       `CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct`. That address
//       (0x8284CB38) is a bare `blr` with hundreds of xrefs -- the linker's ICF pool for
//       every empty method in the image. The RECEIVER is `this + 0x170` == mEventInfoComponent
//       and the asm sets THREE argument registers (r3 = &mEventInfoComponent, r4 = lpEvent,
//       r5 = mpCache @0x824747C4), so the call is EventInfoComponent::HandleTrigger
//       (DWARF BrnEventInfo.h:429), folded to empty in retail. See its site.
//   (3) The two tickers: Hex-Rays renders the four identical post blocks as straight-line
//       code and hides the `for` they came from; the asm is four byte-identical
//       memcpy+AddEvent groups (0x8247AE58..0x8247AF2C / 0x8247B004..0x8247B0D8).
//
// THE TICKER WIRE. Both tickers post through OutputGuiEvent<T>, whose X360 body stack-builds
// a GuiEventWrapper<T,40> -- { sizeof(T), T's id, 12 } then a byte copy of T -- and queues
// THAT on channel 40. The in-tree StateInterface::OutputGuiEvent template still direct-passes
// (the divergence FLAGged at CgsGuiStateInterface.h:131), so both records are built here and
// posted through GetOutputEventQueue()->AddEvent at their true wire size. That is the
// standing accommodation for this family and it is what the two closest siblings already do:
// BrnRaceMainHudState.cpp:113 (its OnLeave GuiEvent536) and BrnJunctionInfoComponent.cpp:41
// (the 2072-byte id-537 custom-message payload, whose layout attestation -- AddString
// @0x823A6940, types stride 4 @+0, strings stride 512 @+0x10, count @+0x810 -- this file
// reuses verbatim).
// ⚠ NOTE for the conductor: the sibling wS2.cpp:630 / wS3.cpp:658 ticker-CLEAR arms post the
// same id-536 record through `mpStateInterface->OutputGuiEvent(lClear)` instead, which lands
// it on channel 536 at 2 bytes rather than channel 40 at 16. Not edited here (not this file's
// partfile) -- reported instead.
//
// COMPONENT DEFERRALS. Same rule and same one-shot helper as the sibling
// BrnRaceMainHudState_wS3.cpp:183 / BrnFBurnMainHudState.cpp:216: a call whose callee is not
// reachable from the build keeps the console's gate and control flow verbatim and logs the
// gap once instead of inventing a body. FOUR here, each named at its site:
//   * EventInfoComponent::HandleTrigger -- undeclared on the component, and ICF-folded EMPTY
//     in retail, so the deferral costs no behaviour at all.
//   * ChallengeSelector::HandleLoadNotification and
//     PaybackComponent::RespondToTransitionComplete -- both TUs are on disk but NOT on the
//     build (only their BrnHudStatesLinkStubs.cpp Construct scaffolds are), and wS3 already
//     defers every arm of both for exactly this reason. Deferring them TU-wide is what keeps
//     the RACE_MAIN mount linkable.
//   * the local player's completed-challenge bit -- the GUI FreeburnChallengeManager's
//     mCompletedData tail is deliberately unmodelled (BrnGuiFreeburnChallengeManager.h:148,
//     "HONEST BOUNDARY").
//
// ⚠ TWO LINK RESIDUALS THIS FILE ADDS (reported, not papered over -- both are real data
// accessors whose values would be visibly wrong if stood in):
//   * BrnResource::ChallengeListEntry::GetDescriptionStringID() const -- declared-only, NO
//     body anywhere. BrnHudStatesLinkStubs.cpp:107 already names it as the ChallengeSelector
//     mount's residual, and ChallengeListEntry.h:427 documents the fix: it is the identical
//     shape to the already-inline GetTitleStringID, over macDescriptionStringID (+0xA0).
//   * BrnResource::ChallengeListEntryAction::GetTargetValue(s32) const -- FULLY bodied at
//     SharedClasses/DataLists/ChallengeListEntry.cpp:71; that TU is simply not in
//     tools/build/build_game_exe.bat yet.
// (A third, BrnResource::ChallengeListEntry::GetNumPlayers(), links TODAY only to the
// BrnFriendsListLinkGates.cpp:116 gate, which returns 0 and logs -- so the ticker's player
// -count parameter renders "0" until the DataLists body lands. Not this file's gate.)
// ===================================================================================


namespace BrnGui
{
    namespace
    {
// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)

        // ---- the id-536 ticker-clear wire record ------------------------------------
        // { 2, 536, 12 } + the {0,1} byte pair, 16 bytes, channel 40. Identical shape to
        // the sibling BrnRaceMainHudState.cpp:113 (OnLeave posts the same record); kept
        // file-local per partfile, exactly as the GuiCommandEvent16 twins are.
        // Both ticker bodies open with it: @0x8247AA18..0x8247AA44 (active) and
        // @0x8247AF90..0x8247AFBC (not-active) build the pair as two `stb`s into a scratch
        // half-word and store it with one `sth`, so the payload is a {0, 1} BYTE PAIR at
        // +0x0C, not a 16-bit 256. Named after the DWARF's own two fields
        // (BrnGuiEventTypeDefs.h:301/:302 GuiEventTickerClearMessages).
        struct GuiTickerClearWire536 : public CgsGui::GuiEvent<536>
        {
            u8 mbForceFadeOut;            // +0x0C == 0
            u8 mbDeleteChallengeMessages; // +0x0D == 1
            u8 mau8Pad[2];
            GuiTickerClearWire536()
                : CgsGui::GuiEvent<536>(2, 12), mbForceFadeOut(0), mbDeleteChallengeMessages(1)
            { mau8Pad[0] = mau8Pad[1] = 0; }
        };

        void PostTickerClear536(CgsGui::StateInterface* lpInterface)
        {
            GuiTickerClearWire536 lEvent;
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT, 16);
        }

        // ---- the id-537 custom ticker message ---------------------------------------
        // The 0x818-byte payload + its GuiEvent<537> wire header. TU-local model of
        // BrnGui::GuiEventTickerCustomMessage's on-queue record; the canonical
        // BrnGuiDemangledEventTypes.h:256 entry is an opaque 12B-header shape that does NOT
        // match the wire. Layout attested by BrnJunctionInfoComponent.cpp:41 (from AddString
        // @0x823A6940) and corroborated here by BOTH ticker bodies: the console's Construct
        // is inlined as `std 0 ; std 0` over +0x000..+0x00F (the four string types),
        // `memset(base + 0x10, 0, 0x800)` (the four 512-byte strings) and five tail `stb`s at
        // +0x810..+0x814 (the count then the four flags), total 0x818.
        //
        // ⚠ FLAG DWARF-vs-RETAIL (capacities): the DecFIGS DWARF (BrnGuiEventTypeDefs.h:310/
        // :311) says KI_MAX_NUM_STRINGS 4 / KI_MAX_CUSTOMMESSAGE_LENGTH 256 and declares the
        // strings BEFORE the types. Retail X360 is 4 x 512 with the TYPES FIRST -- 4*512 +
        // 4*4 == 0x810, which is exactly where the console's count byte lands. The retail
        // shape is used. DELETE-WHEN: never (this IS the shipped record).
        // The four tail flags carry the DWARF names (BrnGuiEventTypeDefs.h:358..:362) in
        // DWARF declaration order; only their VALUES are X360-attested here.
        struct GuiTickerCustomMessagePayload537
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;
            static const s32 KI_MAX_STRING_LENGTH = 512;

            // -- BrnGuiEventTypeDefs.h:313 (DWARF) --
            enum EStringType
            {
                E_STRINGTYPE_NONE     = 0,
                E_STRINGTYPE_TEXT     = 1,
                E_STRINGTYPE_STRINGID = 2,
                E_STRINGTYPE_NUM      = 3,
            };

            s32  maeStringTypes[KI_MAX_NUM_STRINGS];                     // +0x000
            char maacMessageStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH]; // +0x010
            s8   mi8NumStrings;                                          // +0x810
            u8   mbLoopMessage;                                          // +0x811
            u8   mbTrainingMessage;                                      // +0x812
            u8   mbAllowDuplicates;                                      // +0x813
            u8   mbIsChallengeMessage;                                   // +0x814
            u8   mau8Pad815[3];                                          // +0x815

            // BrnGuiEventTypeDefs.h:327 (DWARF Construct(bool,bool,bool,bool)) -- inlined at
            // both ticker call sites as the zero-seed plus the five tail stores.
            void Construct(bool lbLoop, bool lbTraining, bool lbAllowDuplicates,
                           bool lbIsChallengeMessage)
            {
                std::memset(maeStringTypes, 0, sizeof(maeStringTypes));
                std::memset(maacMessageStrings, 0, sizeof(maacMessageStrings));
                mi8NumStrings        = 0;
                mbLoopMessage        = static_cast<u8>(lbLoop ? 1 : 0);
                mbTrainingMessage    = static_cast<u8>(lbTraining ? 1 : 0);
                mbAllowDuplicates    = static_cast<u8>(lbAllowDuplicates ? 1 : 0);
                mbIsChallengeMessage = static_cast<u8>(lbIsChallengeMessage ? 1 : 0);
                mau8Pad815[0] = mau8Pad815[1] = mau8Pad815[2] = 0;
            }

            // X360 0x823A6940, transcribed (the console's own bounds asserts, then the
            // 512-byte strncpy + type store + count bump). Same body as the committed
            // BrnJunctionInfoComponent.cpp:54 model.
            void AddString(const char* lpString, EStringType leType)
            {
                CGS_ASSERT(mi8NumStrings >= 0, "mi8NumStrings >= 0");                   // h:390
                CGS_ASSERT(mi8NumStrings < KI_MAX_NUM_STRINGS,
                           "mi8NumStrings < KI_MAX_NUM_STRINGS");                       // h:391
                CGS_ASSERT(lpString != 0, "lpString");                                  // h:392
                std::strncpy(maacMessageStrings[mi8NumStrings], lpString,
                             static_cast<size_t>(KI_MAX_STRING_LENGTH));
                maeStringTypes[mi8NumStrings] = static_cast<s32>(leType);
                ++mi8NumStrings;
            }
        };

        // { 0x818, 537, 12, <the message> }, channel 40, 0x824 bytes on the wire.
        struct GuiTickerCustomMessageWire537 : public CgsGui::GuiEvent<537>
        {
            GuiTickerCustomMessagePayload537 mMessage;   // +0x0C
            GuiTickerCustomMessageWire537()
                : CgsGui::GuiEvent<537>(
                      static_cast<u32>(sizeof(GuiTickerCustomMessagePayload537)), 12)
            {
                std::memset(&mMessage, 0, sizeof(mMessage));
            }
        };

        // Both tickers queue the SAME finished record FOUR times (four byte-identical
        // memcpy+AddEvent groups; the console unrolled the loop). Not a Hex-Rays artefact --
        // the asm carries all four, 0x8247AE58..0x8247AF2C and 0x8247B004..0x8247B0D8.
        const s32 KI_TICKER_MESSAGE_POST_COUNT = 4;

        void PostTickerCustomMessage537(CgsGui::StateInterface* lpInterface,
                                        const GuiTickerCustomMessagePayload537& lMessage)
        {
            for (s32 li = 0; li < KI_TICKER_MESSAGE_POST_COUNT; ++li)
            {
                GuiTickerCustomMessageWire537 lWire;
                lWire.mMessage = lMessage;   // the console's `memcpy(dst, src, 0x818)`
                lpInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lWire)));
            }
        }

        // ---- the active ticker's positional-parameter slots -------------------------
        // StartFreeburnChallengeTicker's stack frame carries FOUR (text, format) pairs and
        // hands all four to FormatAndAddText, though liNumParams is only ever 1..3 (one for
        // the challenge's player count plus one per action that has targets, and
        // KI_MAX_ACTIONS_PER_CHALLENGE == 2). The console leaves slot 3 holding whatever the
        // frame last had there -- it aliases the dead id-536 record's payload half-word at
        // sp+0xAC. FLAG PC defensive: the slots are zero-seeded here (FormatTextV never reads
        // past liNumParams, so no queued byte changes); an uninitialised vararg read would be
        // UB on the host. DELETE-WHEN: never.
        const s32 KI_TICKER_MAX_PARAMS    = 4;
        const u32 KU_TICKER_PARAM_TEXT_LEN = 64;   // `li r4, 0x40` into both SnPrintf calls

// (fold: an identical definition of LogDeferredComponent was dropped here -- this TU defines it once, above)
    }

    // =======================================================================
    //  ProcessBoostInfo  @ 0x82474550
    // =======================================================================
    // UpdateRunning's case-206 arm (BrnRaceMainHudState_wS3.cpp:396): hand the boost-type
    // record to the boost-message manager under its own id, which is the latch that tints
    // every message the manager subsequently posts. The mpCache assert is NON-GATING on
    // console (@0x824745FC the store falls through to the call either way).
    void RaceMainHudState::ProcessBoostInfo(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event");                          // cpp:2902

        if (mbBoostMessages)                                                // lbz 0x154
        {
            CGS_ASSERT(mpCache != 0, "mpCache != NULL");                    // cpp:2910
            // @0x8247461C: `li r5, 0xCE` == 206, `addi r3, r27, 0x1078` == &mBoostMessageManager.
            mBoostMessageManager.RecvEvent(lpEvent, 206, mpCache);
        }
    }

    // =======================================================================
    //  ProcessAptEvents  @ 0x82474638
    // =======================================================================
    // UpdatePermenant's case-21 arm (BrnRaceMainHudState_wS2.cpp:531): the apt trigger fan-out.
    // Two typed arms (ONLOAD == 1, TRANSITION_COMPLETE == 4) and then an UNCONDITIONAL tail
    // that runs for EVERY apt event type -- including a SECOND mSatNavComponent.RecvEvent(21)
    // for the type-1 case. That double post is not a transcription slip: the asm issues the
    // identical three-argument call twice, at 0x8247474C (inside the type-1 arm) and again at
    // 0x824747B4 (the tail). Preserved verbatim.
    void RaceMainHudState::ProcessAptEvents(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event passed to RaceMainHudState::ProcessAptEvents");   // cpp:2930

        // The event-21 record on this host IS the native-width GuiEventAptTriggerPayload
        // (CgsAptCommunicator.h) -- the same typed read the sibling FBurn state's
        // ProcessAptEvents already does. The console reads the clip name as "payload word 2"
        // because that is where the 32-bit record's pointer lands; by-name is both the house
        // rule and the x64 fix.
        const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
            reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
        const char* lpacClipName = lpTrigger->mpacComponentName;

        if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
        {
            if (mbSatNav)                                                   // lbz 0x150
                mSatNavComponent.RecvEvent(lpEvent, 21);                    // addi r3, +0x6A0

            // @0x82474750: strstr(Str = the clip name, SubStr = "PositionIndicator_mc") --
            // a SUBSTRING test, not a compare, because the apt name arrives parent-qualified.
            if (std::strstr(lpacClipName, macPositionIndicatorName) != 0)
                mPositionIndicatorComponent.SetLoaded();                    // addi r3, +0x1224

            if (mbFreeburnChallengeSelector)                                // lbz 0x166
            {
                // @0x8247477C: `addi r4, r28, 0x67A4` == mChallengeSelectorComponent + 4 ==
                // the GuiComponent base's macName. The component matches on its OWN resolved
                // name, so the by-name spelling is GetName().
                if (std::strstr(lpacClipName,
                                mChallengeSelectorComponent.GetName()) != 0)
                {
                    // FLAG deferred: ChallengeSelector's TU (BrnChallengeSelector.cpp +
                    // BrnChallengeSelector_wL_01.cpp) is NOT on the build -- only the
                    // BrnHudStatesLinkStubs.cpp Construct scaffold is -- so every out-of-line
                    // method of it is deferred TU-wide by this wave; the sibling
                    // BrnRaceMainHudState_wS3.cpp:639/:648 defers Show/Hide for the same
                    // reason. The gate and the name match above are the console's, verbatim.
                    // DELETE-WHEN: the ChallengeSelector pair mounts (and its scaffold dies);
                    // the line then becomes
                    // `mChallengeSelectorComponent.HandleLoadNotification(lpacClipName);`.
                    LogDeferredComponent("ChallengeSelector::HandleLoadNotification");
                }
            }
        }
        else if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE
                 && mbPaybackComponent)                                     // lbz 0x15F
        {
            // @0x824746F0..0x82474724 is an INLINED strcmp against "Payback_mc" (the
            // byte-at-a-time subtract loop Hex-Rays renders open-coded), not a strstr like
            // the two above -- restored as the call it came from, against the class static
            // macPaybackName rather than a second copy of the literal.
            if (std::strcmp(lpacClipName, macPaybackName) == 0)
            {
                // @0x82474728 `addi r3, r28, 0x920` == &mPaybackComponent.
                // FLAG deferred: BrnPaybackComponent.cpp is NOT on the build (its TU still
                // owes SendAwardTriggerableEvent -- BrnHudStatesLinkStubs.cpp:118 -- and only
                // its Construct scaffold is mounted), and the sibling
                // BrnRaceMainHudState_wS3.cpp defers every PaybackComponent arm for the same
                // reason. UpdateSetupState clears mbPaybackComponent on the stunt-race path,
                // so this arm does not execute on the bring-up route either way.
                // DELETE-WHEN: BrnPaybackComponent.cpp mounts; the line then becomes
                // `mPaybackComponent.RespondToTransitionComplete();`.
                LogDeferredComponent("PaybackComponent::RespondToTransitionComplete");
            }
        }

        // ---- the unconditional tail (@0x8247479C..0x82474820) --------------------------
        if (mbSatNav)                                                       // lbz 0x150
            mSatNavComponent.RecvEvent(lpEvent, 21);

        if (mbEventInfo && mpCache != 0)                                    // lbz 0x157 / lwz 0x140
        {
            // @0x824747D0: r3 = &mEventInfoComponent (this+0x170), r4 = lpEvent,
            // r5 = mpCache -- i.e. EventInfoComponent::HandleTrigger(const GuiEventAptTrigger*,
            // GuiCache*) (DWARF BrnEventInfo.h:429). The branch target IDA labels
            // `BaseCollisionGenerator::Destruct` is 0x8284CB38, a bare `blr`: the image's ICF
            // pool for every empty method, so this call does NOTHING in retail.
            // FLAG deferred: HandleTrigger is not declared on BrnGui::EventInfoComponent in
            // the tree (and its GuiEventAptTrigger parameter type has no committed home), so
            // the gate is kept and the call is logged rather than invented. Behaviourally
            // free -- the console body is empty.
            // DELETE-WHEN: BrnEventInfo.h declares HandleTrigger; the line then becomes
            // `mEventInfoComponent.HandleTrigger(lpTrigger, mpCache);`.
            LogDeferredComponent("EventInfoComponent::HandleTrigger");
        }

        if (mbBoostMessages)                                                // lbz 0x154
        {
            CGS_ASSERT(mpCache != 0, "mpCache != NULL");                    // cpp:3134
            // @0x82474810: `li r5, 0x15` == 21. Id 21 is the apt-trigger record the manager's
            // switch deliberately ignores (its jump table starts at 206) -- the call is made
            // and falls through to default, exactly as shipped.
            mBoostMessageManager.RecvEvent(lpEvent, 21, mpCache);
        }
    }

    // =======================================================================
    //  UpdateSatNav  @ 0x82474830
    // =======================================================================
    // UpdateRunning's shared cases 199/200 arm (BrnRaceMainHudState_wS3.cpp:380): forward the
    // record to the sat-nav component under the ORIGINAL event id, so one body serves both.
    void RaceMainHudState::UpdateSatNav(const CgsModule::Event* lpEvent, s32 liEventId)
    {
        CGS_ASSERT(lpEvent != 0, " invalid event passed ");                 // cpp:3226
        // (the assert string's leading and trailing spaces are the console's, verbatim)

        if (mbSatNav)                                                       // lbz 0x150
            mSatNavComponent.RecvEvent(lpEvent, liEventId);                 // addi r3, +0x6A0
    }

    // =======================================================================
    //  StartFreeburnChallengeTicker  @ 0x8247A9C0
    // =======================================================================
    // Publish the ACTIVE freeburn challenge as a scrolling ticker line: clear whatever the
    // ticker is showing, build the challenge's localised description under the
    // "CHALLENGE_TICKER_STRING_DESCRIPTION" dynamic-string id (its %1..%N positional markers
    // filled with the challenge's player count and each action's target value), then queue a
    // two-part custom message -- "<title>: <description>" -- four times.
    //
    // Callers: UpdateWFInit @0x82480200 (wS2.cpp:271) and UpdateRunning/UpdatePermenant
    // cases 573/574/576/581 (wS2.cpp:591/613/618/642).
    void RaceMainHudState::StartFreeburnChallengeTicker()
    {
        CGS_ASSERT(mbFreeburnChallengeTicker,
                   "mbFreeburnChallengeTicker == true");                    // cpp:4038 (lbz 0x167)

        PostTickerClear536(mpStateInterface);

        // Both asserts below belong to the INLINED accessors, not to this function:
        // "mpChallengeManager" is GuiCache::GetFreeburnChallengeManager's (BrnGuiCache.h:2390)
        // and "meInternalState != E_INTERNAL_STATE_OFF" is GetCurrentChallenge's
        // (BrnGuiFreeburnChallengeManager.h:235). Restored as the calls they came from.
        const FreeburnChallengeManager* lpManager = mpCache->GetFreeburnChallengeManager();
        const BrnResource::ChallengeListEntry* lpChallenge = lpManager->GetCurrentChallenge();

        // ---- the positional parameters (@0x8247AAA8..0x8247AB94) -----------------------
        char lacParamText[KI_TICKER_MAX_PARAMS][KU_TICKER_PARAM_TEXT_LEN];
        CgsLanguage::LanguageManager::ParameterFormatType
             laeParamFormat[KI_TICKER_MAX_PARAMS];
        for (s32 liSlot = 0; liSlot < KI_TICKER_MAX_PARAMS; ++liSlot)
        {
            lacParamText[liSlot][0] = 0;
            laeParamFormat[liSlot]  = CgsLanguage::LanguageManager::E_FORMAT_TEXT;
        }

        // Parameter 0 is always the challenge's player count. @0x8247AAC0 reads the byte at
        // +0xD3 and masks it with `clrlwi r6, r11, 28` (== & 0xF) -- that mask IS
        // GetNumPlayers()'s body (muNumPlayers packs the current count in the low nibble;
        // BrnChallengeManager_wB_03.cpp:118 records the same read).
        CgsCore::SnPrintf(lacParamText[0], KU_TICKER_PARAM_TEXT_LEN, "%d",
                          lpChallenge->GetNumPlayers());
        lacParamText[0][KU_TICKER_PARAM_TEXT_LEN - 1] = 0;
        laeParamFormat[0] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;   // `li r23, 0xB`

        s32 liNumParams = 1;
        for (s32 liActionIndex = 0;
             liActionIndex < lpChallenge->GetNumActions();                  // lbz 0xD4, re-read each pass
             ++liActionIndex)
        {
            // The two loop-body asserts (ChallengeListEntry.h:941/:942) are GetAction's own,
            // inlined; the receiver walks `entry + 0x50 * index` == &maAction[index].
            const BrnResource::ChallengeListEntryAction* lpAction =
                lpChallenge->GetAction(liActionIndex);
            if (lpAction->GetNumTargets() != 0)                             // lbz action+0x30
            {
                CgsCore::SnPrintf(lacParamText[liNumParams], KU_TICKER_PARAM_TEXT_LEN, "%d",
                                  lpAction->GetTargetValue(0));             // lwz action+0x34
                lacParamText[liNumParams][KU_TICKER_PARAM_TEXT_LEN - 1] = 0;
                laeParamFormat[liNumParams] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;
                ++liNumParams;
            }
        }

        // @0x8247ABF8 -- the function IDA leaves as sub_82866450 is
        // LanguageManager::FormatAndAddText(id, source, format, count, ...) (its body is
        // FormatTextV into a 1KB local followed by AddString under the id; the tree already
        // homes it at CgsLanguageManager.h:222). r6 == 9 == E_FORMAT_ID_LOOKUP, i.e. the
        // source is resolved as a loc-string id first. All four (text, format) pairs are
        // pushed; only liNumParams of them are read.
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "CHALLENGE_TICKER_STRING_DESCRIPTION",
            lpChallenge->GetDescriptionStringID(),                          // entry + 0xA0
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
            liNumParams,
            lacParamText[0], laeParamFormat[0],
            lacParamText[1], laeParamFormat[1],
            lacParamText[2], laeParamFormat[2],
            lacParamText[3], laeParamFormat[3]);

        // @0x8247ABFC: `ld r31, 0xC0(r20)` -- a FULL 64-bit CgsID load (mChallengeID), then
        // the list lookup that turns it into a dense challenge index.
        const CgsID lChallengeID = lpChallenge->GetChallengeID();
        const s32   liChallengeIndex =
            mpCache->GetFreeburnChallengeList()->GetChallengeIndex(lChallengeID);

        GuiTickerCustomMessagePayload537 lMessage;
        // @0x8247AC14..0x8247AC48: {loop, training, allowDuplicates, isChallengeMessage} ==
        // {1, 0, 1, 1} -- this one IS a challenge message (contrast the not-active twin).
        lMessage.Construct(true, false, true, true);

        // @0x8247AC40..0x8247ADC4 -- the console inlines
        // CgsContainers::FastBitArray<2000>::IsBitSet over the manager's completed-challenge
        // bit store (`addi r26, r19, 0x7F0` == the LOCAL player's CompletedFburnChallenges
        // inside mCompletedData; `srawi 6 / slwi 3 / ldx` then `1ULL << (index & 63)`),
        // including that template's own range assert -- "Index <n> is out of range (max bits:
        // 2000)", CgsFastBitArray.h:396, streamed in hex. A set bit means the local player has
        // ALREADY completed this challenge, and the ticker then prefixes the line with "[~]".
        // FLAG deferred: BrnGuiFreeburnChallengeManager.h:148 deliberately leaves the
        // mCompletedData tail unmodelled ("HONEST BOUNDARY" -- its real home is the GameState
        // IO header graph), so there is no way to read the bit by name from here and no way
        // to reach it at all without growing that header. Deferred to "not completed", which
        // is the common case and the un-prefixed format; the index is still computed above
        // because FormatAndAddText's line above does not depend on it and the lookup is the
        // half that is recoverable.
        // DELETE-WHEN: BrnGuiFreeburnChallengeManager.h models mCompletedData (or publishes
        // `bool HasLocalPlayerCompleted(s32 liChallengeIndex) const`); this becomes that call.
        LogDeferredComponent("FreeburnChallengeManager::mCompletedData (completed-challenge bit)");
        const bool lbAlreadyCompleted = false;
        (void)liChallengeIndex;

        // @0x8247ADC8..0x8247AE34 -- the separator format. FRENCH (ELanguage 10) puts a space
        // BEFORE the colon; every other language does not. The "[~]" prefix marks a challenge
        // the local player has already completed.
        const bool lbFrenchSpacing =
            mpStateInterface->GetLanguageManager()->GetCurrentLanguage()
                == static_cast<s32>(CgsLanguage::E_LANGUAGE_FRENCH);
        const char* lpacSeparatorFormat;
        if (lbAlreadyCompleted)
            lpacSeparatorFormat = lbFrenchSpacing ? "[~] %1 : %2" : "[~] %1: %2";
        else
            lpacSeparatorFormat = lbFrenchSpacing ? "%1 : %2" : "%1: %2";

        lMessage.AddString(lpacSeparatorFormat,
                           GuiTickerCustomMessagePayload537::E_STRINGTYPE_TEXT);      // li r5, 1
        lMessage.AddString(lpChallenge->GetTitleStringID(),                           // entry + 0xB0
                           GuiTickerCustomMessagePayload537::E_STRINGTYPE_STRINGID);  // li r5, 2
        lMessage.AddString("CHALLENGE_TICKER_STRING_DESCRIPTION",
                           GuiTickerCustomMessagePayload537::E_STRINGTYPE_STRINGID);

        PostTickerCustomMessage537(mpStateInterface, lMessage);
    }

    // =======================================================================
    //  StartFreeburnChallengeNotActiveTicker  @ 0x8247AF38
    // =======================================================================
    // The twin of the body above for the "a challenge is running but this machine is not in
    // it" case: clear the ticker, then queue the single fixed "CHALLENGE_IN_PROGRESS" line
    // four times. No challenge record is read, so no manager/list lookup and no parameters.
    //
    // Callers: UpdateWFInit @0x82480200 (wS2.cpp:273) and UpdateRunning case 583
    // (wS3.cpp:653).
    void RaceMainHudState::StartFreeburnChallengeNotActiveTicker()
    {
        CGS_ASSERT(mbFreeburnChallengeTicker,
                   "mbFreeburnChallengeTicker == true");                    // cpp:4138 (lbz 0x167)

        PostTickerClear536(mpStateInterface);

        GuiTickerCustomMessagePayload537 lMessage;
        // @0x8247AFC4..0x8247AFDC: {loop, training, allowDuplicates, isChallengeMessage} ==
        // {1, 0, 1, 0}. The ONLY difference from the active ticker's seed is the last flag --
        // this generic line is not tied to a challenge, so a ticker clear that deletes
        // challenge messages must not delete it.
        lMessage.Construct(true, false, true, false);
        lMessage.AddString("CHALLENGE_IN_PROGRESS",
                           GuiTickerCustomMessagePayload537::E_STRINGTYPE_STRINGID);  // li r5, 2

        PostTickerCustomMessage537(mpStateInterface, lMessage);
    }
}
