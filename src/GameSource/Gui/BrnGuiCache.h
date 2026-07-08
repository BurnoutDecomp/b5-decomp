#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Core/CgsID.h"     // CgsID (u64) -- mPursuedCarID / mShutdownCarID
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"   // BrnGui::GuiFlow (AppendExpectedAptComponent selector)
#include "GameSource/BurnoutConstants.h"          // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT
#include "BrnCommonTypes.h"                        // Vector3 / Vector4 (event-position / camera accessors)

// BrnGui::GuiCache subsystem (DecFIGS DWARF: BrnGuiCache.h). StateLoadingHelper is the
// resource/component watcher embedded in the cache; GuiCache is the cache itself. Only
// the methods reached by the in-scope GUI code are declared on GuiCache (its full data
// layout is an out-of-scope boundary object the leaves only touch through these calls).
namespace CgsGui { class ObjectController; }
namespace BrnResource { class ChallengeList; } // GetFreeburnChallengeList return (pointer only)
namespace BrnGui { struct WorldDataController; }  // GetWorldDataController return (pointer only)
namespace BrnProgression { struct ProfileEvent; } // GetProfileEvent return (pointer only)
namespace BrnProgression { struct Profile; }      // DetermineCarUnlockPending arg (pointer only)
namespace BrnNetwork { namespace BrnNetworkModuleIO { struct InGamePlayerStatusData; } } // GetOnlinePlayerInfo return (pointer only; home BrnNetworkModuleInGamePlayerStatusInterface.h)

namespace BrnGui
{
    // Pointer-only members of the GuiCache layout (forward-declared; the cache never
    // dereferences their full type in this TU -- it stores/returns the pointer).
    struct FreeburnChallengeManager;
    struct BurnoutSkillsManager;   // GetBurnoutSkillsManager return (pointer only)
    struct OptionsDataProfile;     // GetOptionsDataProfile return (pointer only; home BrnCrashNavOptions.h family)
    struct HudMessageController;
    struct HudMessageDirector;
    struct MapIconManager;
    // Defined later in this header (minimal-slice records returned by GetPresetEvent /
    // the inlined event-display helpers).
    struct PresetEvent;
    struct SatNavEventDisplayInfo;

    struct StateLoadingHelper
    {
        enum EResourceState
        {
            E_STATE_UNLOADED         = 0,
            E_STATE_LOAD_REQUESTED   = 1,
            E_STATE_LOADING          = 2,
            E_STATE_LOAD_CANCELLED   = 3,
            E_STATE_LOADED           = 4,
            E_STATE_UNLOAD_REQUESTED = 5,
            E_STATE_UNLOADING        = 6,
            E_STATE_UNLOAD_CANCELLED = 7,
        };

        struct ResourceInfo
        {
            EResourceState               meState;
            CgsGui::ResourceRequestTypes meType;
            const void*                  mpResource;

            void Construct();
        };

        struct ComponentsToWatch
        {
            static const u32 KU_MAX_COMPONENTS_TO_WATCH = 192;

            u32  muNumberOfComponentsToWatch;
            u32  mauComponentsToWatchIds[KU_MAX_COMPONENTS_TO_WATCH];
            bool mabComponentsLoaded[KU_MAX_COMPONENTS_TO_WATCH];
        };

        void Construct();

        bool EnsureResourceIsLoaded(const CgsGui::sResourceTuple& lResource);
        bool EnsureResourcesAreLoaded(const CgsGui::sResourceTuple* lpResources, u32 luCount);
        bool EnsureResourceIsUnloaded(const CgsGui::sResourceTuple& lResource);
        bool EnsureResourcesAreUnloaded(const CgsGui::sResourceTuple* lpResources, u32 luCount);
        const void* GetLoadedResource(u32 luId) const;
        void UnloadResource(const CgsGui::sResourceTuple& lResource);
        void UnloadResources(const CgsGui::sResourceTuple* lpResources, u32 luCount);
        void UnloadAllResources(CgsGui::ResourceRequestTypes leType);

        void IncrementUnloadPending();
        void DecrementUnloadPending();

    private:
        static const u32 KU_MAX_RESOURCES_TO_WATCH = 228;
        static const s32 KI_NUM_LOAD_REQUEST_QUEUES = 2;

        ResourceInfo               maResources[KU_MAX_RESOURCES_TO_WATCH];
        Array<u32, KU_MAX_RESOURCES_TO_WATCH> maRequestDirtyList;
        CgsGui::GuiEventQueueSmall mLoadRequestQueues[KI_NUM_LOAD_REQUEST_QUEUES];
        s32                        miCurrentLoadRequestQueue;
        ComponentsToWatch          maComponentsToWatch[3];
        CgsGui::ObjectController*   mpaControlledComponents[192];
        u32                        muControlledComponentNameHash[192];
        u32                        muControlledComponentCount;
        u32                        muPendingUnloadCount;
    };

    class GuiCache
    {
    public:
        bool EnsureResourceIsLoaded(const CgsGui::sResourceTuple& lResource);
        bool EnsureResourcesAreLoaded(const CgsGui::sResourceTuple* lpResources, u32 luCount);
        bool EnsureResourceIsUnloaded(const CgsGui::sResourceTuple& lResource);
        bool EnsureResourcesAreUnloaded(const CgsGui::sResourceTuple* lpResources, u32 luCount);
        const void* GetLoadedResource(u32 luId) const;

        f32 GetTime() const;
        f32 GetTimeStep() const;

        // DWARF: BrnGuiCache.h:206 -- register a single apt component (by its name hash) as
        // "expected" on the given GUI flow layer, so the cache waits for it to finish
        // initialising before reporting the flow ready. Called by the per-component
        // AppendExpectedAptComponent(s) helpers (TableRow / TableCell / DriveThruMapPanel /
        // CrashNavPanel ...). X360-attested @0x824F87B8. Body links from the GuiCache TU.
        void AppendExpectedAptComponent(GuiFlow leFlow, u32 luComponentNameHash);

        // The name-taking entry @0x824F87C0 (the X360 reaches it as an internal entry of the
        // same function): hash the component name string and register it as above. TableRow /
        // TableCell pass the component's name pointer here; DriveThruMapPanel passes the
        // pre-hashed value to the u32 overload above. Body links from the GuiCache TU.
        void AppendExpectedAptComponent(GuiFlow leFlow, const char* lpacComponentName);

        // ADDITIVE GROW (BrnImageGallery.h TU): replace the flow layer's expected-component
        // list wholesale with the caller's hash array (ImageGalleryState::
        // SetExpectedAptComponentList @0x82484720 passes flow 0, its 7-slot hash list and
        // the live count). Body links from the GuiCache TU.
        void SetExpectedAptComponentList(GuiFlow leFlow, const u32* lpauComponentNameHashes,
                                         u32 luCount);

        // ADDITIVE GROW (BrnImageGallery.h TU): has every expected component on the flow
        // layer finished initialising (the per-frame init-wait poll -- ImageGalleryState::
        // UpdateWFInit @0x824846B8 gates on it). Body links from the GuiCache TU.
        bool AreAllAptComponentsInitialised(GuiFlow leFlow) const;

        // ADDITIVE GROW (BrnOnlinePreEventMessages TU): the cache holds the active game-mode
        // type the GUI reads to pick mode-specific apt key-frames (the X360 reads it as a far
        // member; e.g. the online pre-event messages select the "anim1_StuntRun" key-frame for
        // the online fugitive / free-burn / showtime-end modes). Returned as the raw
        // BrnGameState::GameStateModuleIO::EGameModeType value (s32) so this boundary header
        // does not need to pull in the heavy game-state enum header. Body links from the
        // GuiCache TU.
        s32 GetCurrentGameModeType() const;

        // ADDITIVE GROW (BrnGuiFreeburnChallengeManager TU): the cache owns the freeburn
        // challenge list the GUI tracker resolves challenge ids against. The X360
        // FreeburnChallengeManager StartChallenge/TriggerChallenge (@0x82509D60 / @0x8250A160)
        // load mpGuiCache, call this, then drive ChallengeList::GetChallengeIndex /
        // GetChallengeData on the returned list. Returned by pointer (those two callees are
        // const accessors), so a forward declaration of BrnResource::ChallengeList suffices
        // here. Body links from the GuiCache TU.
        const BrnResource::ChallengeList* GetFreeburnChallengeList() const;

        // ADDITIVE GROW (BrnFriendsList TU): FriendsListComponent::SetGuiCachePointer
        // (X360 @0x82473580) latches the cache pointer, then caches a single u32 the
        // cache exposes at a far member (X360 lwzx r,this,0xAC74). The component reads it
        // once at attach time and stores it locally; its exact semantic is not recovered,
        // so it is exposed by name as an opaque u32. Body links from the GuiCache TU.
        //
        // REFINED (BrnGuiBurnoutSkillsManager TU): the burnout-skills manager
        // (SetSkillsData, X360 @0x825118F0) reads this SAME far member @0xAC74 and tests it
        // as a player count (`> 1`) to gate the "you beat someone's record" HUD flash -- i.e.
        // it is the number of active players in the current race. Exposed below by an apt
        // name for that consumer; both accessors hit the one @0xAC74 member.
        u32 GetFriendsListCachedField() const;   // X360 far member @0xAC74

        // ADDITIVE GROW (BrnGuiBurnoutSkillsManager TU): the active-player count the
        // burnout-skills manager gates its HUD-message emission on. Same far member as
        // GetFriendsListCachedField (X360 lwzx r,this,0xAC74). Body links from the GuiCache TU.
        s32 GetNumActivePlayers() const;         // X360 far member @0xAC74

        // ADDITIVE GROW (BrnGui::MapIconManager TU): the map-icon manager walks the cache's
        // drive-through / junkyard sat-nav icon list when building the on-map selection set
        // (X360 GetDriveThroughOrJunkyardAtIndex @0x824FAC10 reads the count member, then
        // indexes each SatNavIconInfo entry). DWARF (BrnGuiCache.h:1482/1485) gives the
        // accessor shapes: an indexed const SatNavIconInfo* and the entry count. The X360
        // inlines both at the call site; exposing them by name keeps the manager TU off raw
        // offsets. Bodies link from the GuiCache TU.
        const GuiEventUpdateSatNav::SatNavIconInfo* GetDriveThrough(s32 liIndex) const;
        s32 GetNumberOfDriveThroughs() const;

        // ADDITIVE GROW (BrnGui::MapIconManager TU): UpdateSatNavIcons' network-rivals pass
        // (X360 AddTeamToNetworkRivals @0x824F4FF8) looks up the player's online team for the
        // active-race-car index (X360 indexes maeCurrentPlayerTeam[index], the far member at
        // GuiCache+0xB808). DWARF (BrnGuiCache.h:954) gives the accessor. Returned as the raw
        // GsmIO::EPlayerTeam value (s32) so this GUI boundary header does not pull the heavy
        // GameState IO enum header (same convention as GetCurrentGameModeType). Body links
        // from the GuiCache TU.
        s32 GetCurrentOnlinePlayerTeam(EActiveRaceCarIndex leActiveRaceCarIndex) const;

        // ADDITIVE GROW (BrnSatNavRenderer TU). The sat-nav icon renderer reaches the world /
        // event data through the cache when it builds and refreshes its on-map icon set. DWARF
        // (BrnGuiCache.h) gives every signature; the X360 inlines several at the call site, so
        // exposing them by name keeps the renderer off raw offsets. Bodies link from the GuiCache TU.

        // DWARF h: -- the GUI world-data front-end (event records + landmark counts).
        WorldDataController* GetWorldDataController() const;

        // ADDITIVE GROW (BrnSatNavRenderer TU). The world-space camera position the sat-nav
        // renderer measures off-screen icons against. RenderIconsForSatNav loads it ONCE before
        // the per-icon loop (X360 @0x8245FA48: lvx128 v124, mpGuiCache, 0x4AE0 -- the far member
        // GuiCache+0x4AE0, a 16-byte VMX lane) and then computes the per-icon squared distance
        // |iconPos - cameraPos|^2 (vsubfp128 + vmsum3fp128 over the first three lanes) to pick the
        // closest off-screen icon. Exposed by name as the leading 16-byte lane (xyz = camera
        // world position); body links from the GuiCache TU. X360-attested @0x4AE0.
        const Vector4& GetWorldCameraPosition() const;   // X360 far member @0x4AE0

        // DWARF h:1386/1389 -- the player's profile-event list (offline events). GetProfileEvent
        // indexes it; GetNumProfileEvents is the live count.
        u32                              GetNumProfileEvents() const;
        const BrnProgression::ProfileEvent* GetProfileEvent(u32 luIndex) const;

        // DWARF h:1392/1398 -- the preset (online) event list. GetPresetEvent (X360 @0x8241E520)
        // indexes it (asserts liIndex >= 0 / array-constructed / in-bounds); GetNumPresetEvents is
        // the live count. The element is the game-state preset Event; only the two ids the renderer
        // reads off it are exposed (by name) via PresetEvent below.
        const PresetEvent*   GetPresetEvent(s32 liIndex) const;
        s32                  GetNumPresetEvents() const;

        // DWARF BrnGuiCache.h:801 -- the live count of registered event-start records. X360 @0x824F8830
        // is a pure tail-forwarder: hands &mSetUpAllEventStartsInterface (embedded at GuiCache+0x5690)
        // to its GetNumEventStarts() @0x824F7688 and tail-returns its result.
        u32                  GetNumEventStarts() const;   // X360 far member @0x5690

        // DWARF h:1456-ish -- fill lpOutIconInfo with the online-landmark icon record at the given
        // position-in-list slot (used for meIconDisplayType == ONLINE_CHECKPOINTS). Returns the
        // out pointer. The element is the GuiEventUpdateSatNav::SatNavIconInfo (committed type).
        GuiEventUpdateSatNav::SatNavIconInfo*
            GetOnlineLandmarkInfoAtPositionInList(s32 liIndex,
                                                  GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo) const;

        // X360 inlined cache helpers (sub_824F8838 / sub_824F8AF0): resolve an event id to its
        // on-map display record. The record's leading 16-byte lane is the world-space icon
        // position (the renderer VMX-copies it into the cached icon's mv3Position); its +0x18 word
        // is the event-instance id the renderer feeds to GetEventInfoFromEventId in the preset
        // path. Two flavours for the two event lists (profile vs preset).
        const SatNavEventDisplayInfo* GetProfileEventDisplayInfo(u32 luEventId) const; // X360 @0x824F8AF0
        const SatNavEventDisplayInfo* GetPresetEventDisplayInfo(u32 luEventId) const;  // X360 @0x824F8838

        // ====================================================================
        //  Scalar / pointer snapshot accessors recovered in the GuiCache TU.
        //  Each reads ONE named member at an asm-proven offset, guarded by the
        //  game's debug assert (a no-op CGS_ASSERT in this build). Bodies in
        //  BrnGuiCache.cpp. (Array-indexed accessors -- race-car / online-player
        //  tables, preset/profile-event indexers, replay tables, scoring traffic --
        //  remain declaration-only: their element strides/types and per-element
        //  callees are out-of-scope boundary records not yet recovered.)
        // ====================================================================
        GuiCache();                                 // X360 @0x827E05B8 (field inits only)

        f32 GetCurrentTimeInEvent() const;          // X360 @0x8240F2C8 (mfEventTime,  >= 0)
        f32 GetTargetTimeInEvent() const;           // X360 @0x8240F330 (mfTargetTime, >= 0)
        s32 GetOpponentsInEvent() const;            // X360 @0x8240F450 (miOpponentsInEvent, s8)
        s32 GetEventDestinationDistrict() const;    // X360 @0x82472DB0 (mEventDestinationDistrict)
        s32 GetCheckpointReached() const;           // X360 @0x824EC468 (miCheckpointReached, >= 0)
        s32 GetCurrentTakedownsInEvent() const;     // X360 @0x8240F4B0 (miTakedownsCurrent, ROAD_RAGE)
        s32 GetTargetTakedownsInEvent() const;      // X360 @0x8240F550 (miTakedownTarget,   ROAD_RAGE)
        s32 GetCurrentScoreInEvent() const;         // X360 @0x8240F5F0 (miScoreCurrent)
        s32 GetTargetScoreInEvent() const;          // X360 @0x8240F650 (miScoreTarget)
        s32 GetCurrentComboInEvent() const;         // X360 @0x8240F6B0 (miScoreCombo)
        s32 GetMultiplierInEvent() const;           // X360 @0x8240F710 (miComboMultiplier)
        CgsID GetPursuitCarID() const;              // X360 @0x8240F7F0 (mPursuedCarID,  PURSUIT)
        CgsID GetShutdownCarID() const;             // X360 @0x824B3060 (mShutdownCarID)
        s32 GetTrophyCarUnlockType() const;         // X360 @0x824B30C0 (meTrophyCarUnlockType, != NONE)
        s32 GetActiveRoadRuleScoringMode() const;   // X360 @0x8240FC28 (meRoadRuleScoreMode, != COUNT)

        const FreeburnChallengeManager* GetFreeburnChallengeManager() const; // X360 @0x8240F168

        // ADDITIVE GROW (PlayerPositionSingle::RenderValue @0x82421F78, which inlines all
        // three): the game-mode word, the active road rule, and the skills-manager pointer
        // (DWARF accessors h:981 GetGameMode / h:1290 GetActiveRoadRule / h:1362
        // GetBurnoutSkillsManager; no standalone X360 symbols).
        s32 GetGameMode() const                                  { return meGameModeType; }

        // ADDITIVE GROW (RoadRuleComponent::ShouldUseInEventColouring @0x82410568,
        // which inlines the byte load mpGuiCache+0x4B4A): the "use in-event sign
        // colouring" gate byte.
        bool GetInEventColouringGate() const                     { return mbInEventColouringGate; }
        s32 GetPlayerActiveRaceCarIndex() const                  { return mePlayerActiveRaceCarIndex; }  // DWARF h:924
        s32 GetActiveRoadRule() const                            { return meActiveRoadRule; }

        // The player-options profile block (X360 far member @0xB878/47224 -- past the
        // modelled tail; both CrashNavOptions::SetSettingsFromProfile @0x824B8028 and
        // OnlineGameRoomPlayerInfo::ShowSettingsOptions @0x82485140 inline the fetch).
        // DECLARATION-ONLY per the far-member convention (body links from the GuiCache TU).
        OptionsDataProfile* GetOptionsDataProfile();   // X360 far member @0xB878
        const BurnoutSkillsManager* GetBurnoutSkillsManager() const { return mpSkillsManager; }

        // DWARF h:1203 -- the checkpoint count for the current event (muCheckpointsInEvent).
        // ADDITIVE GROW: real X360 symbol (called by RenderValue @0x82422030);
        // declaration-only (bodied with the GuiCache accessor TUs).
        u8 GetCheckpointsInEvent() const;
        const HudMessageController*     GetHudMessageController() const;      // X360 @0x82472D00
        const HudMessageDirector*       GetHudMessageDirector() const;        // X360 @0x82472D58

        // ---- the road-rule-shot block + the online player records (past the
        // modelled tail; RoadRuleShotComponent::Snap @0x82415620 inlines all
        // three reads). DECLARATION-ONLY per the far-member convention (bodies
        // link from the GuiCache TU). ----

        // DWARF h:1277 (member meRoadRuleShotOpponentARCI, h:1817; X360
        // +0xAC48/44104). s32-typed per this header's ARCI house style.
        s32 GetRoadRuleShotOpponentARCI() const;

        // The shot "captured-for line" gate byte (X360 +0xAC5A/44122). FLAG: no
        // PS3-DWARF member lands on this X360 offset (the DWARF's shot bools sit
        // at +0xAC44..46 / +0xAC58) -- an X360-side addition, named from its one
        // consumer (Snap fills the "CAPTURED_FOR <ruler>" line only when set).
        bool GetRoadRuleShotCapturedLineGate() const;

        // One online player record (DWARF h:1836 maPlayerInfo[8]; X360
        // +0xAC80/44160, the committed 312-byte InGamePlayerStatusData stride).
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
             GetOnlinePlayerInfo(s32 liIndex) const;

        // ADDITIVE GROW (BrnGuiHudMessageAnalyzer online-stunt-run TU): resolve a network
        // player id to its in-game online-player record (X360 @0x82482738). The analyzer's
        // online-stunt-run handlers pass mpGuiCache + the event's mPlayerId, then read the
        // returned record's online name (@+256) and active-race-car index (@+276). May return
        // NULL (the message handler asserts non-NULL before use). Returned by pointer (the same
        // committed 312-byte InGamePlayerStatusData record). Body links from the GuiCache TU.
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData*
             GetOnlinePlayerInfoFromPlayerId(s32 liPlayerId) const;

    private:
        // ===================================================================
        //  DATA LAYOUT -- named anchors at asm-proven `this+offset`, gaps
        //  reserved with explicit padding (AGENTS.md "LAYOUT RECOVERY WITH
        //  PADDING"). Member NAMES/TYPES from DecFIGS DWARF (BrnGuiCache.h);
        //  every offset in a trailing comment is X360-attested. Only the members
        //  the recovered accessors touch are named; the object is a large plain
        //  aggregate (no vptr) so the first member sits at offset 0.
        // ===================================================================
        u8  mPad_0000[4];                                // +0x0000 GuiEventTimeInfo header word
        f32 mfTimeNow;                                   // +0x0004 (4)     GetTime, != -FLT_MAX
        u8  mPad_0008[16476];                            // +0x0008..+0x4063
        WorldDataController*      mpWorldDataController;  // +0x4064 (16484)
        const BurnoutSkillsManager* mpSkillsManager;      // +0x4068 (16488) DWARF h:1632 (PlayerPositionSingle::RenderValue @0x824223FC)
        FreeburnChallengeManager* mpChallengeManager;    // +0x406C (16492)
        HudMessageController*     mpHudMessageController; // +0x4070 (16496)
        HudMessageDirector*       mpHudMessageDirector;   // +0x4074 (16500)
        u8  mPad_4078[2696];                             // +0x4078..+0x4AFF
        s32 mePlayerActiveRaceCarIndex;                  // +0x4B00 (19200) EActiveRaceCarIndex (DWARF h; HudMessageAnalyzer::HandleLiveRevengeUpdate @0x8251E2xx)
        u8   mPad_4B04[0x46];                             // +0x4B04..+0x4B49
        bool mbInEventColouringGate;                     // +0x4B4A (19274) RoadRuleComponent::ShouldUseInEventColouring gate byte
        u8   mPad_4B4B[0x527F - 0x4B4A];                 // +0x4B4B..+0x527F
        s32 miNumPresetRaces;                            // +0x5280 (21120)
        u8  mPad_5284[19408];                            // +0x5284..+0x9E53
        s32 mEventsCtorSentinel;                         // +0x9E54 (40532) mEvents array ctor marker
        s32 meGameModeType;                              // +0x9E58 (40536) GsmIO::EGameModeType
        u8  mPad_9E5C[208];                              // +0x9E5C..+0x9F2B
        f32 mfEventTime;                                 // +0x9F2C (40748)
        f32 mfTargetTime;                                // +0x9F30 (40752)
        u8  mPad_9F34[16];                               // +0x9F34..+0x9F43 mafTargetScores[4]
        s8  miOpponentsInEvent;                          // +0x9F44 (40772)
        u8  mPad_9F45[7];                                // +0x9F45..+0x9F4B
        u16 mEventDestinationLandmarkIndex;              // +0x9F4C (40780) LandmarkIndex (s16)
        u8  mPad_9F4E[2];                                // +0x9F4E..+0x9F4F
        s32 mEventDestinationDistrict;                   // +0x9F50 (40784) BrnWorld::EDistrict
        u8  mPad_9F54[96];                               // +0x9F54..+0x9FB3
        s32 miCheckpointReached;                         // +0x9FB4 (40884)
        u32 muCheckpointsInEvent;                        // +0x9FB8 (40888)
        s32 miTakedownsCurrent;                          // +0x9FBC (40892)
        s32 miTakedownTarget;                            // +0x9FC0 (40896)
        s32 miScoreCurrent;                              // +0x9FC4 (40900)
        s32 miScoreTarget;                               // +0x9FC8 (40904)
        s32 miScoreCombo;                                // +0x9FCC (40908)
        s32 miComboMultiplier;                           // +0x9FD0 (40912)
        u8  mPad_9FD4[12];                               // +0x9FD4..+0x9FDF
        CgsID mPursuedCarID;                             // +0x9FE0 (40928)
        u8  mPad_9FE8[8];                                // +0x9FE8..+0x9FEF
        CgsID mShutdownCarID;                            // +0x9FF0 (40944)
        u8  mPad_9FF8[8];                                // +0x9FF8..+0x9FFF
        s32 meTrophyCarUnlockType;                       // +0xA000 (40960) TrophyUnlockData::UnlockType
        u8  mPad_A004[3124];                             // +0xA004..+0xAC37
        bool mabRoadRulesActive[2];                      // +0xAC38 (DWARF h; precedes meActiveRoadRule)
        u8  mPad_AC3A[2];                                // +0xAC3A..+0xAC3B
        s32 meActiveRoadRule;                            // +0xAC3C (44092) BrnGameState::EActiveRoadRule (PlayerPositionSingle::RenderValue gate @0x824220B4)
        s32 meRoadRuleScoreMode;                         // +0xAC40 (44096) GuiEventSetRoadRuleScoreMode::ERoadPanelModes
        u8  mPad_AC44[1];                                // tail guard (further far members --
        // pre-race messages, profile/replay/online tables, sat-nav zoom -- are reached only
        // by declaration-only array accessors and are not modelled here.)

        static void _AssertLayout();
    };

    // Boundary record returned by the two inlined event-display helpers above. Only the two
    // fields the sat-nav renderer reads are named (X360-proven offsets); the rest of the record
    // is opaque. FLAG: minimal-slice display record.
    struct SatNavEventDisplayInfo
    {
        Vector3 mv3Position;     // +0x00 (16-byte VMX lane copied to the cached icon)
        u8      mPad_10[0x08];   // +0x10..+0x17
        u32     muEventInstanceId; // +0x18 (preset path WDC lookup id)
    };

    // Minimal slice of the preset/online event record the cache hands back from GetPresetEvent.
    // The sat-nav renderer only reads two ids off it (X360 GetIconInformation preset branch):
    //   * GetPositionLookupId -- X360 word +0x20; passed to GetPresetEventWorldPosition.
    //   * GetEventId          -- X360 word +0x28; stored as the cached icon's miEventId and used
    //                            for the WorldDataController event-info lookup.
    // The real element is BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event
    // (uncommitted); modelled here as a named-accessor boundary type. Bodies link from the
    // GuiCache / game-state TU. FLAG: minimal-slice preset-event record.
    struct PresetEvent
    {
        u32 GetPositionLookupId() const;  // +0x20
        u32 GetEventId() const;           // +0x28
    };
}
