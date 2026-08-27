#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// The embedded HUD components, in the order they appear in the object.
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                        // BrnFlapt::MovieClipRef
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                        // BrnFlapt::TextFieldRef
#include "GameSource/Gui/Flow/HUD/Components/BrnEventInfo.h"                  // BrnGui::EventInfoComponent
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"      // BrnGui::AnimationComponent
#include "GameSource/Gui/SatNav/BrnSatNavComponent.h"                         // BrnGui::SatNavComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnPaybackComponent.h"           // BrnGui::PaybackComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnInGameMessagesComponent.h"    // BrnGui::InGameMessagesComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnDistrictMarker.h"             // BrnGui::DistrictMarkerComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnBoostMessageManager.h"        // BrnGui::BoostMessageManager
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h" // BrnGui::FlaptIconComponent / FlaptAnimatorComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnPositionIndicator.h"          // BrnGui::PositionIndicator
#include "GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionTable.h"        // BrnGui::PlayerPositionTableComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsList.h"                // BrnGui::FriendsListComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsListChangeIcon.h"      // BrnGui::FriendsListChangeIconComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnRoadRuleComponent.h"          // BrnGui::RoadRuleComponent
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptHelpItem.h"   // BrnGui::FlaptHelpItem
#include "GameSource/Gui/Flow/HUD/Components/BrnRoadRuleShotComponent.h"      // BrnGui::RoadRuleShotComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnOnlineTimeoutTimerComponent.h"// BrnGui::OnlineTimeoutComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnCompassComponent.h"           // BrnGui::CompassComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnFreeburnChallengeStartComponent.h" // BrnGui::FreeburnChallengeStartComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnChallengeSelector.h"          // BrnGui::ChallengeSelector

// ===========================================================================
//  BrnGui::RaceMainHudState - the RACE_MAIN slot of the BrnHudFlow 14-state pool: the
//  IN-EVENT main HUD (event info readout, event countdown icon, position table,
//  road rules, mugshots, showtime bounce-boost prompt, freeburn-challenge widgets).
//  Reconstructed from BURNOUT_X360_ARTIST.XEX; class shape / member names / member ORDER
//  from the DecFIGS DWARF (BrnRaceMainHudState.h, 661 lines) verbatim.
//
//  WHY THIS HEADER CARRIES THE MEMBERS (E1 wave 2026-08-26): until this wave the header
//  was a bare `: public CgsGui::State` shell while BrnRaceMainHudState.cpp declared its
//  OWN 0x7910-byte `struct RaceMainHudState` in the same namespace -- a genuine ODR fork.
//  BrnHudFlow::Prepare does `NewPoolState<RaceMainHudState>` (sizeof(T) malloc + placement
//  new), so with the shell header the pool carved a State-sized block while the fork's
//  constructor zero-filled 0x7910 bytes off the end of it. The fork is DELETED and this
//  header is the single definition; NewPoolState now carves the full object.
//
//  X360 OFFSETS. The console object is 0x7940 bytes -- pinned, not inferred:
//  BrnHudFlow::Prepare @0x8251A620 loads `li r4, 0x7940` into the LinearMalloc size
//  argument immediately before the RACE_MAIN slot's Malloc/ctor pair (0x8251A854).
//  Each member below carries its X360 byte displacement. Offsets marked PINNED are read
//  straight out of an X360 body (the citing function is named); offsets marked derived
//  are chained from two pinned neighbours plus the DWARF declaration order and the
//  component's own span, and are documentation only.
//
//  HOST LAYOUT IS NOT BYTE-EXACT, BY DESIGN. Per the x64 gate (and the sibling
//  BrnFBurnMainHudState.h precedent) parity here is semantic-by-named-member: every
//  component whose class is reconstructed is embedded BY VALUE and reached by name, and
//  pointer widening is allowed to move the host offsets. Nothing in this class is
//  addressed by numeric offset.
// ===========================================================================
namespace BrnGui
{
    class GuiCache;   // stored by pointer only (BrnGuiCache.h)

    struct RaceMainHudState : public CgsGui::State
    {
        // DWARF BrnRaceMainHudState.h:101 -- one row of the showtime combo help table.
        struct GuiShowTimeComboItem
        {
            const char* mpcText;            // BrnRaceMainHudState.h:104
            bool        mbNumberInclusive;  // BrnRaceMainHudState.h:105
        };

        // DWARF BrnRaceMainHudState.h:184 -- the phase machine Update() drives.
        enum RaceInternalState
        {
            E_RACEINTERNALSTATE_SETUPSTATE = 0,
            E_RACEINTERNALSTATE_LOADING    = 1,
            E_RACEINTERNALSTATE_WF_INIT    = 2,
            E_RACEINTERNALSTATE_RUNNING    = 3,
            E_RACEINTERNALSTATE_IDLE       = 4,
            E_RACEINTERNALSTATE_COUNT      = 5,
        };

        // DWARF BrnRaceMainHudState.h:196 -- the pre-event countdown ladder.
        enum EventCountdownState
        {
            E_EVENT_COUNTDOWN_STATE_DONE  = 0,
            E_EVENT_COUNTDOWN_STATE_GO    = 1,
            E_EVENT_COUNTDOWN_STATE_ONE   = 2,
            E_EVENT_COUNTDOWN_STATE_TWO   = 3,
            E_EVENT_COUNTDOWN_STATE_THREE = 4,
            E_EVENT_COUNTDOWN_STATE_IDLE  = 5,
            E_EVENT_COUNTDOWN_STATE_COUNT = 6,
        };

        // DWARF BrnRaceMainHudState.h:76.
        typedef FlaptHelpItem ShowtimeBounceBoostHelpItem;

        // DWARF BrnRaceMainHudState.h:223 / :408.
        static const u32 KU_MAX_INIT_COMPONENTS_NUM  = 64;
        static const s32 KI_SHOW_TIME_BAR_TEXT_COUNT = 3;

        // @ 0x82508110 -- the state's default constructor (BrnHudFlow::Prepare calls it
        // through the placement new in NewPoolState). The X360 body is the compiler's own
        // sub-object initialiser: it stores this class's vtable (off_82075394, whose slot 0
        // is RaceMainHudState::OnEnter @0x82478EF8) at +0x000 and then the vtable of every
        // embedded component at its own offset -- i.e. it is the inlined chain of the
        // members' default constructors and nothing else. Its ONLY non-vtable store is
        // `li r30, -1 ; stw r30, 0x1204(r3)`, which lands inside mBoostMessageManager's
        // +0x1078..+0x1208 span and therefore belongs to THAT component's inlined ctor, not
        // to a RaceMainHudState member: no POD of this class is written on console.
        // On the host every embedded component runs its own constructor, reproducing the
        // console effect. FLAG PC defensive: the PODs are additionally zeroed here because
        // the state is carved out of CgsMemory::LinearMalloc, which is not guaranteed
        // zeroed on the host; on console every one of them is rewritten before its first
        // read (OnEnter @0x82478EF8 stores 0 to +0x038, +0x140 and the whole +0x150..+0x168
        // flag block on entry). Defined inline so that the mounted BrnHudFlow.cpp can
        // construct the state whether or not BrnRaceMainHudState.cpp is on the build.
        RaceMainHudState()
            : meInternalState(E_RACEINTERNALSTATE_SETUPSTATE)
            , muNumExpectedComponents(0)
            , mpCache(0)
            , mbInRaceHud(false)
            , mbSatNav(false)
            , mbSatNavStatic(false)
            , mbHudMessages(false)
            , mbBoostBar(false)
            , mbBoostMessages(false)
            , mbPreRaceCountdown(false)
            , mbPreRaceCountdownRenders(false)
            , mbEventInfo(false)
            , mbDistrictMarker(false)
            , mbPlayerPositionTable(false)
            , mbFriendsList(false)
            , mbAboveCarIcons(false)
            , mbRoadRuleComponent(false)
            , mbPreEventOverlay(false)
            , mbMugShotComponent(false)
            , mbPaybackComponent(false)
            , mbShowTimeBar(false)
            , mbB5Ident(false)
            , mbBurnoutSkillz(false)
            , mbOnlineTimeoutTimer(false)
            , mbCompass(false)
            , mbFreeburnChallengeButtonStart(false)
            , mbFreeburnChallengeSelector(false)
            , mbFreeburnChallengeTicker(false)
            , mbFreeburnChallengeOnComponent(false)
            , mbFirstFrame(false)
            , meCurrentEventCountdownState(E_EVENT_COUNTDOWN_STATE_IDLE)
            , mfEventCountdownTimer(0.0f)
            , mbHudVisible(false)
            , mfOverlayRemovalTime(0.0f)
            , mbOverlayInProgress(false)
            , meModeOverlayDisplayed(-1)
            , mbBounceBoostPromptVisible(false)
            , mbBounceBoostPromptNeeded(false)
            , mfBlackBarsCurrentValue(0.0f)
            , mbChallengeOnShowing(false)
        {
            for (u32 lu = 0; lu < KU_MAX_INIT_COMPONENTS_NUM; ++lu)
            {
                mauExpectedComponentIds[lu] = 0;
            }
        }

        virtual void OnEnter();   // @ 0x82478EF8
        virtual void OnLeave();   // @ 0x82479770
        virtual void Update();    // @ 0x82481898

        // @ 0x82508368 - hands the race main-HUD state's static resource list to the
        // loader. VERIFIED against the X360 body (A3 wave 2026-08-27) -- it is five
        // instructions and nothing else:
        //     lis  r11, unk_82F25F88@ha ; addi r11, r11, unk_82F25F88@l
        //     stw  r11, 0(r4)                      ; *lppResourceTuples = maResourcesToLoad
        //     lis  r11, dword_82F25F84@ha ; lwz r11, dword_82F25F84@l(r11)
        //     stw  r11, 0(r5)                      ; *lpuNumberOfResources = muNumResourcesToLoad
        // Deliberately kept INLINE here rather than moved into BrnRaceMainHudState.cpp:
        // BrnHudFlow.cpp is on the build and needs the vtable, and while this TU is
        // unmounted an out-of-line body would add a fourth undefined symbol that
        // BrnHudStatesLinkStubs.cpp does not carry (it stubs only OnEnter/OnLeave/Update).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

        // DWARF BrnRaceMainHudState.cpp:4221 -- the debug component-override re-entry.
        void ForceReenter(bool lbOverride, const bool* lpbComponentEnabledStates);

    private:
        // ---- the out-of-line bodies (X360 addresses; bodies land with the W3 wave) ----
        bool UpdateSetupState();                                  // @ 0x82479B48
        bool UpdateLoading();                                     // @ 0x8247A410
        bool UpdateWFInit();                                      // @ 0x82480200
        void UpdateRunning();                                     // @ 0x8247E898
        void UpdatePermenant();                                   // (DWARF .cpp:3575)
        u32  SetExpectedComponent(const char* lpcName);           // @ 0x82473698
        void SetExpectedAptComponentList();                       // (DWARF .cpp:3511)
        void ClearExpectedComponent();                            // (DWARF .h:657)
        void ProcessBoostInfo(const CgsModule::Event* lpEvent);   // (DWARF .cpp:2937)
        void ProcessAptEvents(const CgsModule::Event* lpEvent);   // (DWARF .cpp:2964)
        void RevealHud(bool lbReveal);                            // @ 0x8247A4E0
        void UpdateHud(const CgsModule::Event* lpEvent);          // (DWARF .h:543)
        void UpdateDirtyTrickFlag(const CgsModule::Event* lpEvent); // (DWARF .h:548)
        void UpdateSatNav(const CgsModule::Event* lpEvent, s32 liEventId); // (DWARF .cpp:3261)
        void UpdateEventCountdown(const CgsModule::Event* lpEvent); // @ 0x8247A608
        void ConcludeEventCountdown();                            // @ 0x824748F0
        void HandleImpactEvent(const CgsModule::Event* lpEvent);  // (DWARF .cpp:3791)
        void SetupEventInfo();                                    // @ 0x82474A60
        void StartFreeburnChallengeTicker();                      // (DWARF .cpp:4085)
        void StartFreeburnChallengeNotActiveTicker();             // (DWARF .cpp:4185)
        void AddFakeEvents();                                     // (DWARF .cpp:4244)
        // RESIDUE -- five more DWARF methods are NOT declared here because their parameter
        // types have no committed home in the tree yet, and declaring them against a
        // stand-in type would fork the signature the way the struct forked the class:
        //   SendLocaliseEvent(const char*, const char*, f32, CgsGui::LocaliseFormat)  .cpp:3491
        //   RunReplayState(BrnGui::GuiEventChangeReplayState::ReplayState)            .cpp:3850
        //   HandleMugshotEvent(const GuiMugshotControlEvent*) / DoMugshot(...)        .cpp:3890/3916
        //   DoRoadRuleShot(const GuiMugshotControlEvent*)                             .cpp:3983
        //   AddCrashCombo(BrnWorld::EComboEntryType, f32)                             .cpp:4058
        // (CgsGui::LocaliseFormat, GuiEventChangeReplayState and BrnWorld::EComboEntryType
        // are absent; GuiMugshotControlEvent exists twice, ODR-forked between
        // GameSource/Game/GameBridgeNetworkToX.h and GameSource/Gui/BrnGuiDemangledEventTypes.h.)

        // ---- the static .rdata tables (DWARF BrnRaceMainHudState.h:217..221) ----------
        // 76 entries; values read from the XEX image at 0x8205AC08 and landed in the .cpp.
        // [FLAG DWARF-vs-RETAIL] the DWARF says 77 (`extern const int32_t[77]
        // maiEventToObserve` / `miNumEventsObserved = 77`); the retail X360 passes 76 --
        // OnEnter @0x82479018 `li r5, 0x4C` into RegisterForEvents and OnLeave @0x824798C4
        // `li r5, 0x4C` into UnRegisterForEvents -- and the .rdata run is exactly 76 words
        // wide (0x8205AC08 .. 0x8205AD38, where maIconIdentifiers begins). The 77 is a
        // PS3-build source-line artifact. DELETE-WHEN: never (retail shape).
        static const s32 maiEventToObserve[];    // 76 entries (DWARF .cpp:33)
        static const s32 miNumEventsObserved;    // == 76      (DWARF .cpp:130)
        // 21 entries; values read from the XEX image, resolved + landed in the .cpp.
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F25F88 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F25F84 (.rdata) == 21

        // ---- the apt/flapt component-name constants (DWARF sizes verbatim) -----------
        static const char  macEventInfoName[13];                     // .h:278
        static const char  KAC_SAT_NAV_ANIMATOR_NAME[21];            // .h:284
        static const char  macPaybackName[11];                       // .h:291
        static const char  macHudMessagesName[15];                   // .h:298
        static const char  macDistrictMarkerName[10];                // .h:305
        static const char  macBoostManagerComponentName[13];         // .h:313
        static const char  macEventCountdownName[9];                 // .h:326
        // .h:329 -- the countdown icon's frame labels, indexed by EventCountdownState:
        // {"invisible","go","one","two","three","invisible"} (@0x8205AD38, image-read).
        // OnEnter also hands the ARRAY ITSELF to mEventCountdownIcon.Construct as the
        // (unused, debug-only) parent-name argument -- `addi r6, r11, off_8205AD38@l`
        // @0x8247928C loads the address, not the first element.
        static const char* maIconIdentifiers[6];                     // .h:329
        // The 18 pre-event overlay ids, indexed by GsmIO::EGameModeType (@0x82F261E0,
        // image-read). Not in the DWARF member list; named by the X360 assert strings
        // "KAPC_PRE_EVENT_OVERLAYS[meModeOverlayDisplayed]" (.cpp:1668, OnLeave) and
        // "KAPC_PRE_EVENT_OVERLAYS[leGameModeType]" (.cpp:2615, UpdateSetupState).
        static const char* KAPC_PRE_EVENT_OVERLAYS[18];
        static const char  macPositionIndicatorName[21];             // .h:338
        static const char  macPlayerPositionTableName[23];           // .h:344
        static const char  macFriendListName[11];                    // .h:351
        static const char  macFriendsListChangeIconName[20];         // .h:352
        static const char  macGeneralTransitionComponentName[18];    // .h:361
        static const char  macRoadRuleComponentName[12];             // .h:376
        static const char  macMugShotComponentName[11];              // .h:396
        static const char  macMugshotDIARRHiderComponentName[18];    // .h:397
        static const char  KAC_MUGSHOT_COMPONENT_GAMERTAG_NAME[12];  // .h:400
        static const char* KAAPC_MUGSHOT_FRAME_LABELS[6][6];         // .cpp:196
        static const char* KAPC_MUGSHOT_STRING_IDS_CAPTURING[6];     // .cpp:260
        static const char* KAPC_MUGSHOT_STRING_IDS_SHOW[6];          // .cpp:270
        static GuiShowTimeComboItem KA_SHOW_TIME_COMBO_DATA[30];     // .cpp:280
        static const char  KAC_BOUNCE_BOOST_NAME[15];                // .h:410
        static const char  KAC_IDENT_ANIMATOR_NAME[15];              // .h:426
        static const char* KPC_ROAD_RULE_SHOT_COMPONENT_NAME;        // .cpp:353
        static const char  KAC_ONLINE_TIMEOUT_TIMER_NAME[24];        // .h:451
        static const char  KAC_COMPASS_COMPONENT_NAME[11];           // .h:457
        static const char  KAC_CHALLENGE_COMPONENT_NAME[21];         // .h:463
        static const char  KAC_CHALLENGE_SELECTOR_COMPONENT_NAME[15];// .h:465
        static const char  KAC_CHALLENGE_ON_COMPONENT_NAME[15];      // .h:473

        // The two debug component-override statics (DWARF .cpp:346/:347) ForceReenter writes.
        static bool        msbDEBUG_OverrideNormalCptStates;
        static const bool* mspbDEBUG_ComponentEnabledStates;

        // ==== members, DWARF declaration order (== X360 layout order) =================
        // The base CgsGui::State occupies guest +0x000..+0x038.

        // PINNED +0x038 -- OnEnter @0x82478EF8 stores 0 here first thing (`stw r30,0x38(r31)`).
        RaceInternalState meInternalState;

        // PINNED +0x03C / +0x13C -- SetExpectedComponent @0x82473698 reads the count from
        // 0x13C and stores the new hash at `this + (count + 0xF)*4` == +0x03C + count*4.
        u32 mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM];
        u32 muNumExpectedComponents;

        GuiCache* mpCache;      // PINNED +0x140 (OnEnter and UpdateSetupState @0x82479B48 both store it)
        bool      mbInRaceHud;  // PINNED +0x144 (OnLeave @0x82479770 stores 0 at 0x144)

        // PINNED +0x148 -- OnEnter writes the two guest words at 0x148/0x14C and calls
        // BrnFlapt::MovieClipRef::SetVisible on this+0x148.
        BrnFlapt::MovieClipRef mMainHUDMovieclip;

        // The 25 component-enable flags. PINNED as a BLOCK at +0x150..+0x168: OnEnter
        // @0x82478EF8 stores 0 to every byte of 0x150..0x168 in one run of EXACTLY 25
        // `stb r30, <off>(r31)` (0x82479020..0x82479090; +0x169 is NEVER written), and
        // UpdateSetupState @0x82479B48 re-drives the same byte range. Individual byte
        // assignment below is that block laid out in DWARF declaration order.
        //
        // [FLAG DWARF-vs-RETAIL 2026-08-27] THE DWARF DECLARES 26 FLAGS; THE RETAIL X360
        // OBJECT HAS 25, so exactly one DWARF name has no retail slot and every name from
        // +0x15B on is otherwise off by one. The dropped name is
        // **mbTemporaryReplayIndicator** (DWARF BrnRaceMainHudState.h:255, between
        // mbFriendsList and mbAboveCarIcons). That is not a judgement call:
        //   (a) OnEnter emits 25 stb, +0x150..+0x168, and +0x169 is never touched;
        //   (b) OnLeave @0x82479770 pins the shifted names at instruction level --
        //         0x82479AF4  lbz  r11, 0x15C(r31)
        //         0x82479B00  addi r3,  r31, 0x5F60          ; == &mRoadRuleComponent
        //         0x82479B04  bl   RoadRuleComponent::EndTimers
        //       and
        //         0x82479814  lbz  r11, 0x15D(r31)           ; gates the
        //         0x82479840  addi r30, r11, off_82F261E0    ; KAPC_PRE_EVENT_OVERLAYS /
        //         0x82479884  bl   GuiOverlayWaitFinishRequest::Construct
        //       so +0x15C == mbRoadRuleComponent and +0x15D == mbPreEventOverlay, i.e.
        //       the block is one byte EARLIER than the 26-name list from +0x15B on.
        //       UpdateWFInit @0x82480200 corroborates independently (its IDA-typed
        //       field_15C -> HandleRoadRuleBegin, field_15F -> PaybackComponent::Initialize,
        //       field_163 -> OnlineTimeoutComponent::Show, field_164 ->
        //       CompassComponent::SetVisibility).
        //   (c) THE TIEBREAK, FROM A SECOND CLASS. The DWARF's BrnFBurnMainHudState.h
        //       declares ELEVEN flags (mbSatNav, mbHudMessages, mbBoostBar, mbBoostMessages,
        //       mbTemporaryReplayIndicator, mbRoadRuleComponent, mbFriendsList,
        //       mbJunctionInfo, mbOdometer, mbB5Ident, mbDistrictMarker) while the retail
        //       FBurn object -- whose offsets BrnFBurnMainHudState.h pinned independently,
        //       from its own X360 bodies -- has TEN, at +0x14C..+0x155, with RoadRule@+0x150,
        //       FriendsList@+0x151, JunctionInfo@+0x152, Odometer@+0x153 and
        //       DistrictMarker@+0x155. Deleting mbTemporaryReplayIndicator -- and ONLY that
        //       name -- makes all ten line up. It is a PS3-build-only flag that never
        //       shipped on X360, in BOTH main-HUD states.
        //       (The s2 dossier's proposed tiebreak "off_82FB3C98[12]'s label string" does
        //        not exist: 0x82FB3C98 is .bss -- it is the ForceReenter override POINTER
        //        mspbDEBUG_ComponentEnabledStates, a RUNTIME bool array, not a name table.
        //        Read back from the XEX image: 40 consecutive zero words. UpdateSetupState's
        //        debug block indexes it as data, `*(a1+347) = *(off_82FB3C98 + 12)`.)
        // DELETE-WHEN: never -- this IS the retail shape. The dropped name is recorded here
        // so a later DWARF-driven pass does not "restore" it and re-break the block.
        bool mbSatNav;                        // +0x150
        bool mbSatNavStatic;                  // +0x151
        bool mbHudMessages;                   // +0x152
        bool mbBoostBar;                      // +0x153
        bool mbBoostMessages;                 // +0x154
        bool mbPreRaceCountdown;              // +0x155
        bool mbPreRaceCountdownRenders;       // +0x156
        bool mbEventInfo;                     // +0x157
        bool mbDistrictMarker;                // +0x158
        bool mbPlayerPositionTable;           // +0x159
        bool mbFriendsList;                   // +0x15A
        // (DWARF BrnRaceMainHudState.h:255 mbTemporaryReplayIndicator -- NOT IN RETAIL.)
        bool mbAboveCarIcons;                 // +0x15B
        bool mbRoadRuleComponent;             // +0x15C
        bool mbPreEventOverlay;               // +0x15D
        bool mbMugShotComponent;              // +0x15E
        bool mbPaybackComponent;              // +0x15F
        bool mbShowTimeBar;                   // +0x160
        bool mbB5Ident;                       // +0x161
        bool mbBurnoutSkillz;                 // +0x162
        bool mbOnlineTimeoutTimer;            // +0x163
        bool mbCompass;                       // +0x164
        bool mbFreeburnChallengeButtonStart;  // +0x165
        bool mbFreeburnChallengeSelector;     // +0x166
        bool mbFreeburnChallengeTicker;       // +0x167
        bool mbFreeburnChallengeOnComponent;  // +0x168

        // PINNED +0x170 -- EventInfoComponent::{Construct,Prepare,SetEventType,Update,
        // MoveAnimation} are all called on this+0x170. THE stunt-run score / multiplier /
        // banked-combo / event-timer readout.
        EventInfoComponent mEventInfoComponent;

        // PINNED +0x610 -- CgsGui::GuiComponent::AddOutputAptViewState on this+0x610; the
        // ctor stores the AnimationComponent vtable (off_82072F68) there.
        AnimationComponent mSatNavAnimationComponent;

        // PINNED +0x6A0 -- SatNavComponent::{Construct,Destruct,LoadResources,RecvEvent,
        // SetCachePointer,SetEventType,Update} on this+0x6A0.
        SatNavComponent mSatNavComponent;

        // PINNED +0x920 -- PaybackComponent::{Initialize,Update,BecomeInvisible,
        // BeginAwardAnimation,ShowAvailableInstantly}; the ctor's off_820740B0 slot 0 is
        // PaybackComponent::Construct.
        PaybackComponent mPaybackComponent;

        // PINNED +0xBB0 -- InGameMessagesComponent::{Construct,Prepare,SetController,
        // SetDirector,SetGameMode,SetInGameMessagesQueue,AddMessage,TerminateMessages,Update}.
        InGameMessagesComponent mHudMessageComponent;

        // PINNED +0xFFC -- DistrictMarkerComponent::{Construct,Prepare,SetCounty,SetDistrict}.
        DistrictMarkerComponent mDistrictMarker;

        // derived +0x1074 (the byte between mDistrictMarker's end and mBoostMessageManager;
        // the same slot the FBurn state uses for its own post-marker flag).
        bool mbFirstFrame;

        // PINNED +0x1078 -- BoostMessageManager::{Construct,Prepare,RecvEvent,Update}.
        BoostMessageManager mBoostMessageManager;

        // PINNED +0x1208 -- the ctor stores the FlaptIconComponent vtable (off_82071638) here.
        // The pre-event 3/2/1/GO icon.
        FlaptIconComponent mEventCountdownIcon;

        // derived +0x121C / +0x1220 (the enum+float pair between the 0x14-byte
        // mEventCountdownIcon and the PINNED mPositionIndicatorComponent at +0x1224).
        EventCountdownState meCurrentEventCountdownState;
        f32                 mfEventCountdownTimer;

        // PINNED +0x1224 -- PositionIndicator::{SetPosition,SetVisible}; the ctor's
        // off_820740B4 slot 0 is PositionIndicator::Construct.
        PositionIndicator mPositionIndicatorComponent;

        // PINNED +0x13DC -- PlayerPositionTableComponent::{Construct,Prepare,SetCache,
        // SetupGameMode,UpdatePositionDetails}.
        PlayerPositionTableComponent mPlayerPositionTable;

        // PINNED +0x1D58 -- FriendsListComponent::{Construct,Prepare,Update,Close,...}.
        FriendsListComponent mFriendsList;
        // PINNED +0x5E78 -- FriendsListChangeIconComponent::{Construct,Prepare,AnimateIn,
        // ShowNow,Hide}. (+0x5E78 - +0x1D58 == 0x4120 == the friends-list span the FBurn
        // state shows between its own +0x1040 and +0x5160.)
        FriendsListChangeIconComponent mFriendsListChangeIcon;

        // PINNED +0x5E90 -- AddOutputAptViewState x6 on this+0x5E90 (same ctor vtable as
        // mSatNavAnimationComponent) / PINNED +0x5F1C -- FlaptAnimatorComponent::Run x8.
        AnimationComponent     mGeneralTransitionComponentApt;
        FlaptAnimatorComponent mGeneralTransitionComponentFlapt;

        // derived +0x5F54 (between the 0x38-byte flapt animator and mRoadRuleComponent).
        bool mbHudVisible;

        // PINNED +0x5F60 -- RoadRuleComponent::{Construct,Prepare,Update,InitialiseMode,
        // SwitchModes,HandleRoadRule*,UpdateRoadSignDistances,...}.
        RoadRuleComponent mRoadRuleComponent;

        // derived +0x6490 / +0x6494 / +0x6498 -- the three scalars between the road-rule
        // component's 0x530-byte span and the PINNED mMugShotComponent at +0x649C.
        f32  mfOverlayRemovalTime;
        bool mbOverlayInProgress;
        // DWARF type is BrnGameState::GameStateModuleIO::EGameModeType. Spelled s32 here,
        // the same way BrnFBurnMainHudState.h spells its own EModeType member: this header
        // is compiled by BrnHudFlow.cpp and pulling the GameState IO header in for one
        // enum would drag the whole game-state vocabulary into the GUI flow. -1 == none.
        s32  meModeOverlayDisplayed;

        // PINNED +0x649C / +0x64B0 -- the ctor stores the FlaptIconComponent vtable
        // (off_82071638) at +0x649C and the FlaptAnimatorComponent vtable (off_82071660)
        // at +0x64B0. DWARF spells these two with the CrashedHudState typedefs
        // (CrashedHudState::MugshotIconComponent == BrnGui::FlaptIconComponent,
        // CrashedHudState::MugshotTextComponent == BrnFlapt::TextFieldRef); the tree's
        // BrnCrashedHudState.h does not carry those typedefs yet, so the underlying real
        // types are named directly rather than inventing a second set of typedefs here.
        FlaptIconComponent     mMugShotComponent;
        FlaptAnimatorComponent mMugshotDIARRHiderComponent;
        // derived +0x64E8 (the 0xC bytes between the 0x38-byte flapt animator and the
        // PINNED mShowtimeBounceBoostButton at +0x64F4).
        BrnFlapt::TextFieldRef mMugshotOpponentGamertag;

        // PINNED +0x64F4 -- FlaptHelpItem::{Construct,Prepare,SetItem x4}.
        ShowtimeBounceBoostHelpItem mShowtimeBounceBoostButton;
        bool mbBounceBoostPromptVisible;   // derived +0x653C
        bool mbBounceBoostPromptNeeded;    // derived +0x653D

        // PINNED +0x6540 -- FlaptAnimatorComponent::Run x2 on this+0x6540.
        FlaptAnimatorComponent mIdentAnimator;
        f32 mfBlackBarsCurrentValue;       // derived +0x6578

        // PINNED +0x657C -- RoadRuleShotComponent::{Construct,Prepare,SetupComponent,Snap}.
        RoadRuleShotComponent mRoadRuleShotComponent;

        // PINNED +0x65D0 -- OnlineTimeoutComponent::{Construct,Show,SetTime,Update}; the
        // ctor's off_82072F70 carries OnlineTimeoutComponent::Prepare.
        OnlineTimeoutComponent mOnlineTimeoutTimer;

        // PINNED +0x6680 -- CompassComponent::{Construct,Prepare,SetVisibility,Update}.
        CompassComponent mCompass;

        // PINNED +0x6710 -- FreeburnChallengeStartComponent::{Show,Hide,HandleButtonPress,
        // HandleButtonRelease}; the ctor's off_82072F84 slot 0 is its Construct.
        FreeburnChallengeStartComponent mChallengeComponent;

        // PINNED +0x67A0 -- ChallengeSelector::{Show,Hide,SetAvailableChallenges,
        // SelectAvailableChallenge,SelectAvailableChallengeByID,GetAvailableChallengeCount};
        // the ctor's off_820740B8 slot 0 is ChallengeSelector::Construct.
        ChallengeSelector mChallengeSelectorComponent;

        bool mbChallengeOnShowing;   // between mChallengeSelectorComponent and +0x790C (unpinned)

        // PINNED +0x790C -- the ctor's LAST store, the FlaptIconComponent vtable
        // (off_82071638). The console object runs to +0x7940 (the Prepare slot size), so
        // 0x28 bytes of tail beyond this member's 0x14-byte span are unattributed.
        FlaptIconComponent mChallengeOnComponent;
    };
}
