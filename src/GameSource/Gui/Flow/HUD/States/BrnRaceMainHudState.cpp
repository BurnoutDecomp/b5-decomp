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
            GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
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
        // channel 40, 24 bytes. The payload type is spelled as its bare CgsID here because
        // BrnGuiOverlaysDirector.h (which carries the real GuiOverlayWaitFinishRequest with
        // its Construct) and BrnGuiDemangledEventTypes.h (pulled in transitively through
        // BrnBoostMessageManager.h) are mutually-exclusive includes -- both define that
        // type. CgsIDCompress IS the whole attested Construct body.
        struct GuiOverlayWaitFinishWire : public CgsGui::GuiEvent<188>
        {
            u32   muPad0C;      // +0x0C (the 8-aligned payload slot)
            CgsID mOverlayId;   // +0x10
            explicit GuiOverlayWaitFinishWire(CgsID lOverlayId)
                : CgsGui::GuiEvent<188>(8, 16), muPad0C(0), mOverlayId(lOverlayId) {}
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
        void LogDeferredComponent(const char* lpcWhat)
        {
            char lac[128];
            std::snprintf(lac, sizeof(lac), "[RaceMainHud] deferred component call: %s\n", lpcWhat);
            CgsDev::Log::WriteToLog(lac);
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

        // [FLAG deferred] PlayerPositionTableComponent::Construct @0x82421A18 / ::Prepare
        // @0x8242AA90 are declared in BrnPlayerPositionTable.h but have no bodies, so
        // calling them would leave the mount unlinkable. The X360 calls both
        // UNCONDITIONALLY (they are not flag-gated), and for STUNT_ATTACK the component is
        // never ticked afterwards, so a faithful Construct/Prepare pair is all that is
        // missing. DELETE-WHEN: BrnPlayerPositionTable.cpp bodies the two.
        LogDeferredComponent("PlayerPositionTableComponent::Construct/Prepare");

        mFriendsList.Construct(macFriendListName, mpStateInterface, 0);
        mFriendsListChangeIcon.Construct(macFriendsListChangeIconName, mpStateInterface, 0);
        mFriendsListChangeIcon.Prepare(macFriendsListChangeIconName, lFile);
        // [FLAG deferred] FriendsListComponent::Prepare @0x8242B188 -- declared
        // (BrnFriendsList.h:92), no body. DELETE-WHEN: BrnFriendsList.cpp bodies it.
        LogDeferredComponent("FriendsListComponent::Prepare");

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
                // [FLAG deferred] FriendsListComponent::Close @0x824397E8 -- declared
                // (BrnFriendsList.h:95), no body.
                // DELETE-WHEN: BrnFriendsList.cpp bodies Close.
                LogDeferredComponent("FriendsListComponent::Close");
            }
        }

        if (mbPreEventOverlay && mbOverlayInProgress)
        {
            const s32 leMode = meModeOverlayDisplayed;
            mbOverlayInProgress = false;
            CGS_ASSERT(KAPC_PRE_EVENT_OVERLAYS[leMode] != 0,
                       "KAPC_PRE_EVENT_OVERLAYS[meModeOverlayDisplayed]");   // cpp:1668 (non-gating)

            // GuiOverlayWaitFinishRequest::Construct is one CgsIDCompress; the record is
            // { 8, 188, 16, <pad>, id } posted on the gui-out channel at 24 bytes.
            GuiOverlayWaitFinishWire lWire(
                CgsIDCompress(KAPC_PRE_EVENT_OVERLAYS[meModeOverlayDisplayed]));
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
