#pragma once

#include <cstddef>   // offsetof (layout pins in _AssertLayout)
#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Core/CgsID.h"     // CgsID (u64) -- mPursuedCarID / mShutdownCarID
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT (GetCurrentCarSelectType inline)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"   // BrnGui::GuiFlow (AppendExpectedAptComponent selector)
#include "GameSource/Gui/BrnGuiOptionsDataProfileDLC1.h" // BrnGui::OptionsDataProfileDLC1 (live DLC1 options block @+0x12BE8; self-contained header, no clashing slices)
#include "GameSource/BurnoutConstants.h"          // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT
#include "BrnCommonTypes.h"                        // Vector3 / Vector4 (event-position / camera accessors)
#include "GameSource/GameState/BrnCgsPlayerName.h" // CgsNetwork::PlayerName (COMPLETE: value member of ReplayPlayerActive below)
// [gateui r4] CE-4: BrnGui::InGameMessagesQueue is a BY-VALUE member of the cache at
// +0x4080 (see mInGameMessagesQueue), so the COMPLETE type is required here. No cycle:
// that header pulls only types.hpp / BrnCommonTypes.h / BrnGameStateSharedIO.h /
// BrnGuiFlaptComponent.h / BrnFlaptTextFieldRef.h / BrnHudMessageController.h, and none of
// them (transitively) includes BrnGuiCache.h.
#include "GameSource/Gui/Flow/HUD/Components/BrnInGameMessagesComponent.h" // BrnGui::InGameMessagesQueue (by value)

// [friends wave] forward declaration for the +0xB868 branch-mirror friendship
namespace BrnGui { class FriendsListComponent; }

// BrnGui::GuiCache subsystem (DecFIGS DWARF: BrnGuiCache.h). StateLoadingHelper is the
// resource/component watcher embedded in the cache; GuiCache is the cache itself. Only
// the methods reached by the in-scope GUI code are declared on GuiCache (its full data
// layout is an out-of-scope boundary object the leaves only touch through these calls).
namespace CgsGui { class ObjectController; struct GuiEventAptTriggerPayload; class GuiEventTimeInfo; }
namespace CgsGui { namespace ModelIO { struct InputBuffer; } }
namespace BrnResource { class ChallengeList; } // GetFreeburnChallengeList return (pointer only)
namespace BrnGui { struct WorldDataController; }  // GetWorldDataController return (pointer only)
namespace BrnProgression { struct ProfileEvent; } // GetProfileEvent return (pointer only)
namespace BrnProgression { class Profile; }       // DetermineCarUnlockPending arg (pointer only; class per BrnProfile.h:208)
namespace BrnGameState { class LandmarkIndex; }    // GetLandmarkInfoFromIndex arg (by value)
// GetRequiredScoreForMedal arg (by value). Opaque-enum forward declaration with the
// committed underlying type -- the SAME idiom (and the same underlying type) as
// GameSource/GameState/BrnGameStateSharedIO.h:22; the definition lives in
// GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h:144, which cannot be
// included here (it pulls the whole game-state IO tree into this GUI boundary header).
// A fixed-underlying-type enum is a COMPLETE type after an opaque declaration, so it can
// be taken by value and used as an array subscript below.
namespace BrnGameState { enum ECurrentMedalTargetTime : s32; }
namespace BrnNetwork { namespace BrnNetworkModuleIO { struct InGamePlayerStatusData; } } // GetOnlinePlayerInfo return (pointer only; home BrnNetworkModuleInGamePlayerStatusInterface.h)
namespace BrnTraffic { struct ScoringTrafficData; } // GetScoringTrafficData return (pointer only; home BrnTraffic scoring TU) -- element of the maScoringTrafficData table
// CgsNetwork::PlayerName is now a COMPLETE type (included above): it is the lead value member
// of ReplayPlayerActive (the replay-player-active table entry) and the element returned by
// GetSortedReplayPlayerActive. Home: GameSource/GameState/BrnCgsPlayerName.h.

// [gateui] The HUD-message controller is BrnResource::HudMessageController
// (SharedClasses/DataLists/BrnHudMessageController.h). It used to be forward-declared a
// SECOND time as `BrnGui::HudMessageController`, an unrelated empty type that the two
// consumers then reinterpret_cast across; the X360 asm settles it (the director's
// mpController is passed straight to BrnResource::HudMessageController::
// GetIndexFromMessageHash @0x8267D4C8 in FilterAndSendOffMessage @0x82511640), so the fork
// is gone and the real type is named here.
namespace BrnResource { struct HudMessageController; }

namespace BrnGui
{
    // Pointer-only members of the GuiCache layout (forward-declared; the cache never
    // dereferences their full type in this TU -- it stores/returns the pointer).
    struct FreeburnChallengeManager;
    struct BurnoutSkillsManager;   // GetBurnoutSkillsManager return (pointer only)
    struct OptionsDataProfile;     // GetOptionsDataProfile return (pointer only; home BrnCrashNavOptions.h family)
    struct HudMessageAnalyzer;     // friend of GuiCache (reads the analyzer-carved snapshot members by name)
    struct HudMessageDirector;
    class  MapIconManager;         // [H3b] `class` is the canonical tag (BrnMapIconManager.h);
                                   // the old `struct` fwd MANGLED SetMapIconManager differently
                                   // in TUs that never saw the definition (link round 2's one miss)
    class  GuiTracker;             // GetGuiTracker return (pointer only; home GameSource/Gui/SatNav/BrnGuiTracker.h)
    struct OnlineGameRoomPlayerInfo; // friend of GuiCache (reads its wave-H-carved members by name)
    struct OnlineGameOptions;        // friend of GuiCache (reads its wave-I-carved members by name)
    struct CrashNavEnterOnlineBase;  // friend of GuiCache (reads its wave-I-carved members by name)
    struct OnlineCustomMatch;        // friend of GuiCache (reads its ranked/unranked bytes by name)
    class  EventInfoComponent;       // friend of GuiCache (reads the id-428 stunt block by name)
    // Defined later in this header (minimal-slice records returned by GetPresetEvent /
    // the inlined event-display helpers).
    struct PresetEvent;
    struct SatNavEventDisplayInfo;
    struct PreEventInfo;   // opaque boundary record (GetPreEventInfo result; consumed by
                           // OnlinePreEventMessages::Show -- pointer-only)
    struct PresetRace;     // opaque boundary record (GetPresetRace result; the preset-race
                           // CgsArray element, stride 120 -- pointer-only, un-homed element type)

    // One entry of the cache's "stunts to display" list (X360 GuiCache::GetStuntToDisplay
    // @0x8240F770 walks it at stride 8, testing the leading id word against -1 as the
    // list terminator). Modelled as a VALUE member array inside GuiCache, so it must be a
    // complete type here. FLAG: only the leading id word is asm-attested; the +0x04 word is
    // reserved (an unrecovered companion field the stride proves is present).
    struct StuntToDisplayInfo
    {
        s32 miStuntId;     // +0x00 (-1 == list terminator)
        s32 miField_04;    // +0x04 (companion word; semantic unrecovered)
    };

    // One entry of the cache's replay-player-active table (maReplayPlayersActive @0x141E0,
    // 16 entries, stride 32). IncrementReplayPlayerActive @0x824EEEC0 appends/bumps entries;
    // ClearReplayPlayerActive @0x824EEE28 resets them; SortReplayPlayersActive @0x824F8C58
    // qsorts DESCENDING by muActiveCount; GetSortedReplayPlayerActive @0x824EEFA0 returns
    // &entry.mName. Field offsets/types are X360-attested by those store-for-store bodies:
    // the name is cleared/compared/constructed at +0x00, a 64-bit value slot is std'd at
    // +0x10, and the hit count is stw'd/lwz'd at +0x18.
    struct ReplayPlayerActive
    {
        CgsNetwork::PlayerName mName;         // +0x00 (16B; char macName[16])
        s64                    mValue;        // +0x10 (std; seeded from the s32 liValue)
        u32                    muActiveCount; // +0x18 (stw; qsort key)
        u8                     mPad_1C[4];    // +0x1C..+0x1F (pad to the attested stride 32)
    };
    static_assert(sizeof(ReplayPlayerActive) == 0x20, "ReplayPlayerActive stride 0x20 (32)");

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

        // @0x8250DC30 -- flush the previous request queue into ModelIO, advance the
        // double buffer, and materialise every dirty resource-state transition as a
        // GuiEventLoadRequest (39).
        void Update(CgsGui::ModelIO::InputBuffer* lpInputBuffer);

        // @0x824FE3D0 / @0x824FE7E0 -- consume the resource module's completion
        // notifications and advance the matching cache slot.
        void OnLoadNotification(const CgsGui::GuiEventLoadNotification* lpEvent);
        void OnUnloadNotification(const CgsGui::GuiEventUnloadNotification* lpEvent);

        // EnsureResourceIsLoaded @ 0x824FDA28 -- step one watched resource's state
        // machine towards LOADED (UNLOADED -> LOAD_REQUESTED with a type-consistency
        // check; the CANCELLED/REQUESTED unload states step back towards their load
        // counterparts), appending the id to the dirty list on any change. Returns
        // whether the resource is now LOADED.
        bool EnsureResourceIsLoaded(const CgsGui::sResourceTuple& lResource);

        // EnsureResourcesAreLoaded @ 0x824FDD20 -- recount + re-latch the pending
        // unload count (asserting consistency); while any unload is pending nothing
        // loads (returns false), else step every tuple and return whether ALL are
        // loaded.
        bool EnsureResourcesAreLoaded(const CgsGui::sResourceTuple* lpResources, u32 luCount);

        bool EnsureResourceIsUnloaded(const CgsGui::sResourceTuple& lResource);
        bool EnsureResourcesAreUnloaded(const CgsGui::sResourceTuple* lpResources, u32 luCount);
        const void* GetLoadedResource(u32 luId) const;

        // UnloadResource @ 0x824FDF58 -- step one watched resource's state machine
        // towards UNLOADED (the mirror of EnsureResourceIsLoaded; the LOADED case
        // asserts the live resource + type consistency and bumps the pending-unload
        // count), appending the id to the dirty list on any change.
        void UnloadResource(const CgsGui::sResourceTuple& lResource);
        void UnloadResources(const CgsGui::sResourceTuple* lpResources, u32 luCount);

        // UnloadAllResources @ 0x824FE1F8 -- walk every watched slot and UnloadResource
        // each non-UNLOADED entry whose recorded type matches leType.
        void UnloadAllResources(CgsGui::ResourceRequestTypes leType);

        // AreAllAptComponentsInitialised @ 0x824EDC20 -- true once every expected apt
        // component registered on the flow layer has been marked initialised.
        bool AreAllAptComponentsInitialised(GuiFlow leFlow) const;

        // ClearComponentInitialised @ 0x824EE058 -- zero the flow layer's per-component
        // loaded flags and its expected count.
        void ClearComponentInitialised(GuiFlow leFlow);

        // AppendExpectedAptComponent @ 0x824F85D8 -- register one component name hash as
        // "expected" on the flow layer (bounds + duplicate asserted, non-gating), with a
        // cleared loaded flag. ADDITIVE GROW (BrnPauseScreen TU): the helper level of the
        // GuiCache::AppendExpectedAptComponent faces declared below.
        void AppendExpectedAptComponent(GuiFlow leFlow, u32 luComponentNameHash);

        // IsWaitingAptComponent @ 0x824EDB08 -- linear-scan the flow layer's expected
        // component ids for the hash. ADDITIVE GROW (BrnPauseScreen TU).
        bool IsWaitingAptComponent(GuiFlow leFlow, u32 luComponentNameHash) const;

        // MarkAptComponentInitialised @ 0x824EDEC8 -- an Apt ONLOAD trigger marks
        // every matching expected component across the three GUI flows and attaches
        // a waiting controlled component, if present.
        void MarkAptComponentInitialised(const CgsGui::GuiEventAptTriggerPayload* lpEvent);

        void IncrementUnloadPending();
        void DecrementUnloadPending();

    private:
        // X360 ARTIST value: 237 (the 0xED bound + 237-entry walk in EnsureResourceIsLoaded
        // @0x824FDA28 / EnsureResourcesAreLoaded @0x824FDD20 / IncrementUnloadPending
        // @0x824EC008, and the Array<int,237> dirty-list helpers). The PS3-FIGS DWARF says
        // 228 -- version drift; the X360 ledger attestation wins per the DecFIGS rule.
        static const u32 KU_MAX_RESOURCES_TO_WATCH = 237;
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

    // Boundary record returned by the two event-display helpers (GetProfileEventDisplayInfo /
    // GetPresetEventDisplayInfo). [H3b] Now ALSO the ELEMENT of the cache's embedded
    // maEventStarts array (mSetUpAllEventStartsInterface @0x5690): the X360 accessors walk
    // interface + 0x30*i with the count at interface+0x20D0 (GetAt @0x824F7688 -> the
    // stride-48 indexer @0x824F65E0 `return 48*i + base`), so the record stride is 0x30 and
    // the type moved ABOVE the class to serve as the member element type.
    //   +0x10 muLightTriggerId -- the preset matcher (@0x824F8838 compares element+0x10;
    //          its assert names it "light trigger id", masked &0xFFFFFF in the print).
    //   +0x14 muJunctionId -- MEASURED: CrashNavMap::UpdateEvent @0x824CC594 `lwz r9,0x14(r3)`
    //          straight after `bl sub_824F8AF0`, stored into muSelectedJunctionID.
    //   +0x18 muEventInstanceId -- the profile matcher (@0x824F8AF0 compares element+0x18)
    //          and the preset path's WDC lookup id.
    //   +0x1C muCounty / +0x20 mi16AISectionIndex -- ⭐ CARVED 2026-08-27 out of the old
    //          mPad_1C, and NOT consumer-named: the PRODUCER names them. This record is the
    //          GUI's copy of BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface::
    //          EventStart, and GameStateModule::SendSetUpAllEventStartsMessage @0x823759D0
    //          fills those two words with DistrictToCounty(WorldMap2D::GetValue(pos)) and
    //          AISectionsData::FindNearestAISection(pos, map) (its AddEventStart args r7/r8
    //          @0x82375C94/@0x82375C9C). No GUI consumer reads them yet -- the console's own
    //          copy is a whole-interface memcpy, so they ride along either way -- but they are
    //          real data and modelling them is what keeps the member-wise copy in
    //          GuiCache::RecEvent's case-203 arm lossless.
    // FLAG: the record has no DWARF row; the field names above +0x18 are the producer's.
    struct SatNavEventDisplayInfo
    {
        Vector3 mv3Position;        // +0x00 (16-byte VMX lane copied to the cached icon)
        u32     muLightTriggerId;   // +0x10 (preset display-info matcher)
        u32     muJunctionId;       // +0x14
        u32     muEventInstanceId;  // +0x18 (profile matcher + preset WDC lookup id)
        u32     muCounty;           // +0x1C (BrnWorld::ECounty of the junction position)
        s16     mi16AISectionIndex; // +0x20 (nearest AI section; 0x7FFF == invalid)
        u8      mPad_22[0x30 - 0x22]; // +0x22..+0x2F (array stride pad)
    };
    static_assert(sizeof(SatNavEventDisplayInfo) == 0x30,
                  "maEventStarts element stride (X360 indexer @0x824F65E0: 48*i)");

    class GuiCache
    {
    public:
        // @ 0x82505860 -- the cache Construct (PC slice: the embedded watcher reset;
        // the X360 tracker/system-user-profile stores land with their owners).
        void Construct();

        // @0x8250DD80 -- resource-helper update. The unrelated per-car scratch reset
        // in the tail is outside this cache slice.
        void Update(CgsGui::ModelIO::InputBuffer* lpInputBuffer);

        bool EnsureResourceIsLoaded(const CgsGui::sResourceTuple& lResource);
        bool EnsureResourcesAreLoaded(const CgsGui::sResourceTuple* lpResources, u32 luCount);
        bool EnsureResourceIsUnloaded(const CgsGui::sResourceTuple& lResource);
        bool EnsureResourcesAreUnloaded(const CgsGui::sResourceTuple* lpResources, u32 luCount);
        const void* GetLoadedResource(u32 luId) const;
        // DWARF BrnGuiCache.h:638 (`void UnloadResource(const sResourceTuple &)`) -- the
        // single-tuple form of UnloadResources below. PreRaceFlyByState::OnLeave calls it
        // once for the resolved large-event-icon tuple and once for the per-gamemode screen
        // tuple. Body links from the GuiCache TU.
        void UnloadResource(const CgsGui::sResourceTuple& lResource);
        void UnloadResources(const CgsGui::sResourceTuple* lpResources, u32 luCount);

        f32 GetTime() const;
        f32 GetTimeStep() const;
        bool IsLoadingScreenVisible() const { return mbIsLoadingScreenVisible; }

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

        // DWARF BrnGuiCache.h:691 -- the APPEND sibling of SetExpectedAptComponentList
        // above: add the caller's hash array to whatever the flow layer is already waiting
        // on, rather than replacing it. CrashNavMap::AppendExpectedAptComponents passes
        // E_GUIFLOW_SCREEN and the 50 "SatNavIcon<n>" hashes Construct precomputed.
        // The DWARF spells the array `uint32_t *`; declared const here to match the Set
        // sibling (read-only in both, and every caller passes a non-const array).
        // Body links from the GuiCache TU.
        void AppendExpectedAptComponentList(GuiFlow leFlow, const u32* lpauComponentNameHashes,
                                            u32 luCount);

        // ADDITIVE GROW (BrnImageGallery.h TU): has every expected component on the flow
        // layer finished initialising (the per-frame init-wait poll -- ImageGalleryState::
        // UpdateWFInit @0x824846B8 gates on it). X360 @0x824EE7A8 (`addi r3,r3,8` +
        // tail-branch into the embedded watcher). Bodied in the GuiCache TU.
        bool AreAllAptComponentsInitialised(GuiFlow leFlow) const;

        // Drop the flow layer's expected-component bookkeeping (the loaded flags + the
        // expected count; the id list itself is left to be overwritten by the next Set/
        // Append). X360 @0x824EE528 (`addi r3,r3,8` + tail-branch into the watcher's
        // ClearComponentInitialised); the online car-select-end state also calls it
        // (@0x824968A8 / @0x82484A3C). Bodied in the GuiCache TU.
        void ClearExpectedAptComponentList(GuiFlow leFlow);

        // DWARF BrnGuiCache.h:720 -- drop the whole CONTROLLED-component registration list
        // (the mpaControlledComponents / muControlledComponentNameHash pair the embedded
        // watcher carries), as opposed to the per-flow EXPECTED list cleared above. Screens
        // call it once, next to ClearExpectedAptComponentList, when they start a fresh apt
        // movie (CrashNavMap::CheckForLoadComplete). Takes no argument -- it is not
        // per-flow. Body links from the GuiCache TU.
        void ClearExpectedControlledAptComponentList();

        // RecEvent @ 0x8250DDF0 -- resource completion (14/16) and Apt ONLOAD (21)
        // branches used by the module bridges.
        void RecEvent(const CgsModule::Event* lpEvent, s32 liEventId);

        // Request the unload of every watched resource of the given type. X360
        // @0x824FEBB0 (`addi r3,r3,8` + tail-branch into the watcher). Bodied in the
        // GuiCache TU.
        void UnloadAllResources(CgsGui::ResourceRequestTypes leType);

        // ADDITIVE GROW (BrnCarSelectOnlineEnd TU): the online-host-game state word the GUI reads
        // to tell whether the local client is the online HOST (== 1). CarSelectOnlineEnd::UpdateWFInit
        // gates the host-choosing clip on it, and HandleLobbyPlayerList gates the host-side
        // car-selection path on it (X360 lwzx r,this,0xA9C8, compared == 1). Exposed by name as the
        // raw state word carved into the layout below. FLAG: consumer-named (no standalone DWARF
        // symbol for this member). Body links from the GuiCache TU.
        s32 GetOnlineHostGameState() const;   // X360 far member @0xA9C8

        // ADDITIVE GROW (BrnOnlinePreEventMessages TU): the cache holds the active game-mode
        // type the GUI reads to pick mode-specific apt key-frames (the X360 reads it as a far
        // member; e.g. the online pre-event messages select the "anim1_StuntRun" key-frame for
        // the online fugitive / free-burn / showtime-end modes). Returned as the raw
        // BrnGameState::GameStateModuleIO::EGameModeType value (s32) so this boundary header
        // does not need to pull in the heavy game-state enum header. Body links from the
        // GuiCache TU.
        s32 GetCurrentGameModeType() const;

        // ADDITIVE GROW (BrnOnlinePreEvent TU): resolve the fly-by line-up entry at the given
        // index to its pre-event info record. The online pre-event state indexes it with its
        // running fly-by counter (miCurrentFlyByIndex) and hands the result to
        // OnlinePreEventMessages::Show (X360 UpdatePermanent case 160 @0x824A13E4). Returned by
        // pointer (opaque boundary record). Declaration-only per the far-member convention; body
        // links from the GuiCache TU. FLAG: consumer-named (index/record shape from the caller).
        const PreEventInfo* GetPreEventInfo(s32 liIndex) const;

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

        // X360-INLINED at BrnGuiCache.h:2310 (GuiModule::Construct's `*(gm + 1021860) =
        // gm + 307836`, preceded by the "lpController" assert): bind the module's own
        // WorldDataController into the cache. The GUI reaches every world/progression/vehicle
        // resource through it.
        void SetWorldDataController(WorldDataController* lpController);

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

        // ADDITIVE GROW (BrnCompassComponent TU): fill lpOutIconInfo with the sat-nav icon
        // record (world position + type) for the given landmark index, and return the out
        // pointer. CompassComponent::ShowLandmarkOnCompass @0x82428C68 hands mpGuiCache, the
        // landmark index and a stack SatNavIconInfo, then VMX-copies the record's leading
        // mv3Position lane onto the compass. X360-attested @0x8240F0xx (inlined at the call
        // site; the cache forwards to the WorldDataController). Body links from the GuiCache TU.
        GuiEventUpdateSatNav::SatNavIconInfo*
            GetLandmarkInfoFromIndex(BrnGameState::LandmarkIndex lLandmarkIndex,
                                     GuiEventUpdateSatNav::SatNavIconInfo* lpOutIconInfo) const;

        // ADDITIVE GROW (wave J: CrashNavMap::CalculateEventZoomFactor and
        // PreRaceFlyByState::CalculateZoomFactor, which both walk an event's checkpoints and
        // resolve each checkpoint's landmark CgsID to its on-map icon record). The id sibling
        // of GetLandmarkInfoFromIndex above: it asserts mpWorldDataController, forwards to
        // WorldDataController::GetLandmarkInfoFromID, asserts the returned landmark, then
        // fills the caller's SatNavIconInfo (position lane, the two float fields at +0x18/
        // +0x1C, the id at +0x10, the district/county bytes, type 4 and the -1 slot at +0x26).
        // X360 out-of-line @0x825067E0; the id rides in ONE GPR (r4) with the out pointer in
        // r5 -- the Xenon ABI passes a 64-bit CgsID whole -- and r3 at return is the
        // DistrictToCounty leftover, NOT a result. Return type is `void` per DWARF
        // (dwarfdump BrnGuiCache.h:798) and neither wave-J caller uses a return value.
        // DECLARATION-ONLY per the far-member convention; body links from the GuiCache TU.
        void GetLandmarkInfoFromID(CgsID lLandmarkID,
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

        // ⭐ [stuntrace] X360-INLINED at BrnGuiCache.h:2374 -- THE ONLY WRITER of
        // mpChallengeManager in the whole image. BrnGui::GuiModule::Construct @0x82518028
        // constructs the module's own FreeburnChallengeManager and stores it here:
        //     BrnGui::FreeburnChallengeManager::Construct(gm + 309584, gm + 1005376);
        //     if (gm == -309584) { ... FireAssert("lpChallengeManager",
        //                                "..\\..\\..\\GameSource\\Gui/BrnGuiCache.h", 2374); }
        //     *(gm + 1021868) = gm + 309584;    // 1021868 - 1005376 == 16492 == +0x406C
        // -- the same shape as the SetSkillsManager pair four lines above it
        // (BrnGuiCache.h:2341) and the SetWorldDataController/SetHudMessageController/
        // SetHudMessageDirector trio. Until it landed, GetFreeburnChallengeManager's
        // "mpChallengeManager" assert (BrnGuiCache.h:2390) fired on every in-event HUD frame
        // that reached the freeburn-challenge arms of RaceMainHudState.
        void SetChallengeManager(FreeburnChallengeManager* lpChallengeManager);

        // ADDITIVE GROW (OnlineGameRoomPlayerInfo keystone, wave H): the sat-nav GUI
        // tracker pointer @X360 +0x4054. The screen's HandleGuiCacheEvent asserts
        // "mpGuiCache->GetGuiTracker()" then ClearTracker()s it (@0x824A3F60 region);
        // BrnInGame.cpp's tracker boundary records the same +0x4054 identification.
        GuiTracker* GetGuiTracker() const                        { return mpGuiTracker; }

        // The sat-nav zoom level (miSatNavZoomLevel @0x803C; every X360 reader inlines
        // the raw word load -- e.g. SatNavComponent::Update `lwzx cache+0x803C`).
        // ADDITIVE GROW (H3a), same pattern as GetGuiTracker above.
        s32 GetSatNavZoomLevel() const                           { return miSatNavZoomLevel; }

        // ADDITIVE GROW (H3b), DWARF BrnGuiCache.h:1521/:1524 -- the sat-nav event
        // filter pair (X360-inlined at the FBurn UpdateSetupState read site).
        s32  GetSatNavEventFilter() const                        { return meSatNavEventFilter; }
        bool GetSatNavEventFilterEnabled() const                 { return mbSatNavEventFilterEnabled; }

        // ADDITIVE GROW (PlayerPositionSingle::RenderValue @0x82421F78, which inlines all
        // three): the game-mode word, the active road rule, and the skills-manager pointer
        // (DWARF accessors h:981 GetGameMode / h:1290 GetActiveRoadRule / h:1362
        // GetBurnoutSkillsManager; no standalone X360 symbols).
        s32 GetGameMode() const                                  { return meGameModeType; }

        // ADDITIVE GROW (RoadRuleComponent::ShouldUseInEventColouring @0x82410568,
        // which inlines the byte load mpGuiCache+0x4B4A): the "use in-event sign
        // colouring" gate byte.
        bool GetInEventColouringGate() const                     { return mbInEventColouringGate; }

        // ADDITIVE GROW (BrnOnlinePlay TU). The online-play main-menu state's multiplayer /
        // invite / online-start surface. IsMultiplayerAllowed is a real out-of-line X360 method
        // (BrnGuiCache.cpp; CheckPrivileges @0x82485980 calls it) -- declaration-only here, body
        // links from the GuiCache TU. The invite/start flag accessors read/write the +0x4B4C..
        // +0x4B53 cluster carved into the layout below. FLAG: consumer-named.
        bool IsMultiplayerAllowed() const;                                       // X360 far member (decl-only)
        // The gameplay-HUD ready trio (+0x4B54/+0x4B56/+0x4B58 -- see the members' carve note).
        // The console inlines both sides (no standalone symbols); header inlines per the
        // cluster's precedent. GetGameplayHudReady is the exact three-byte AND the
        // BoostBarRenderer::Update / FriendsListComponent gates read.
        bool GetGameplayHudReady() const
        {
            return mbGameplayUpdateActive && mbGameplayHudReadyB && mbGameplayHudReadyC;
        }
        // The +0x407C gameplay-HUD gate byte (console-inlined read; the ready-trio publisher
        // in GuiModule::Update mirrors it).
        bool IsGameplayHudActive() const { return mbGameplayHudActive; }
        // ⭐ [boost-bar gate 2026-08-25] The +0x407C PRODUCERS (all console-inlined stb):
        // the gameplay-HUD flow states' bring-up stores 1 -- FBurnMainHudState::UpdateWFInit
        // @0x8247C710 (stb r28 @0x8247CA58, r28 == 1 on BOTH engine arms),
        // RaceMainHudState::UpdateWFInit @0x82480200, CrashedHudState::UpdateSetupState
        // @0x8247CF60, CrashedStuntHudState::UpdateSetupState @0x8247D9E0 -- and
        // GuiCache::RecEvent case 132 stores 0 (@0x8250F59C). Until this landed NOTHING
        // wrote the byte, so GetGameplayHudReady() stayed false forever and
        // BoostBarRenderer::Update never ran (visibility machine frozen at FULL -> the
        // "Visibility is full but we are not allowed to boost" assert, and the bar never
        // re-keyed toward the live boost amount).
        void SetGameplayHudActive(bool lbActive) { mbGameplayHudActive = lbActive; }
        void SetGameplayHudReady(bool lbReady)
        {
            mbGameplayUpdateActive = lbReady;
            mbGameplayHudReadyB    = lbReady;
            mbGameplayHudReadyC    = lbReady;
        }

        // ==== [stuntrace wS2 wave 2026-08-27] the RACE_MAIN HUD gate accessors ==========
        // The four cache reads BrnRaceMainHudState_wS2.cpp needs that were unnamed padding
        // until this wave. Each member below carries the consuming instruction.

        // THE in-event reveal gate: RaceMainHudState::UpdateWFInit @0x82480200 loads it as
        // `lis r10,0 ; ori r26,r10,0xA014 ; lbzx r10,r11,r26` (@0x824803BC/C0) and branches
        // the whole reveal ladder on it -- CLEAR -> RevealHud(true) immediately; SET -> wait
        // for the pre-race countdown (or mbOnlineStartInProgress). The same byte is re-read
        // `== 1` @0x82480554 for the mode-4 post. The MEMBER was carved by the A9 mode-type
        // arm in this same wave (see it for the two producers); this is only its accessor
        // face, added because the wS2 partfile must not read it as raw padding.
        bool IsEventPreparedForModeStart() const { return mbEventPreparedForModeStart; }  // +0xA014

        // The player's live race position, straight to PositionIndicator::SetPosition.
        // RaceMainHudState::UpdateRunning @0x8247FEE4 `lbz r4, 0x4B24(r11)` -> the value is
        // passed in r4 and range-checked `0 < r4 <= 8` @0x8247FF10/18; 0 hides the indicator.
        u8   GetPlayerRacePosition() const    { return mu8PlayerRacePosition; }     // +0x4B24
        // The override that skips that range/enable check (@0x8247FEF8 `lbz r11, 0x4B25(r11)`
        // -> branch straight to SetPosition). FLAG: consumer-named, producer unrecovered.
        bool IsPlayerRacePositionOverridden() const { return mbPlayerRacePositionOverride; } // +0x4B25

        // The friends-list pair. RaceMainHudState::OnLeave @0x824797FC
        // (`ori r10,r10,0xB86C ; lbzx` -> FriendsListComponent::Close) and ::UpdateWFInit
        // @0x824805B8 (`ori r10,r10,0xB86D ; lbzx ; cmplwi 1` -> FriendsListChangeIcon::ShowNow).
        // FLAG: consumer-named -- neither producer is recovered.
        bool IsFriendsListOpen() const        { return mbFriendsListOpen; }          // +0xB86C
        bool IsFriendsListChangePending() const { return mbFriendsListChangePending; } // +0xB86D

        // ADDITIVE GROW (pause wave, 2026-08-28) -- the two carved far members above.
        // Both are inlined at every X360 consumer (no standalone symbol), so exposing them
        // as one-load accessors keeps those consumers off the raw offsets.
        bool IsHighDefinition() const         { return mbIsHighDef; }                // +0x4B49
        u16  GetLicencePointsToNextRank() const { return mu16LicencePointsToNextRank; } // +0xB874

        // The online-event timeout-timer gate. RaceMainHudState::UpdateWFInit @0x824802A8
        // (`lis r10,1 ; ori r10,r10,0x3B5C` == +0x13B5C, `lbzx`) ANDs it with the state's own
        // mbOnlineTimeoutTimer flag before OnlineTimeoutComponent::Show.
        // ⚠️ The s2 scout dossier spells this offset "+0x13B1C"; the asm encodes 0x13B5C
        // (Hex-Rays' decimal 80732 == 0x13B5C, not 0x13B1C). Member carved by the A9
        // mode-type arm in this same wave; this is only its accessor face.
        bool IsOnlineTimeoutPending() const   { return mbOnlineTimeoutPending; }    // +0x13B5C

        // The payback "award available" trio RaceMainHudState::UpdateWFInit @0x824805FC-620
        // reads as a group -- `lbz r10,0x4B64` gates, then `lwz r4,0x4B5C ; lwz r5,0x4B60`
        // become the two PaybackComponent::ShowAvailableInstantly arguments (skipped when the
        // type word is 3). FLAG: consumer-named -- the producer side is unrecovered.
        bool IsPaybackAvailable() const       { return mbPaybackAvailable; }         // +0x4B64
        s32  GetPaybackAvailableType() const  { return mePaybackAvailableType; }     // +0x4B5C
        s32  GetPaybackVictimRaceCarIndex() const { return mePaybackVictimRaceCarIndex; } // +0x4B60
        // ================================================================================

        bool IsOnlineStartInProgress() const  { return mbOnlineStartInProgress; }  // +0x4B4C
        bool IsInviteInProgress() const       { return mbInviteInProgress; }       // +0x4B4D
        bool IsPerformingInvite() const       { return mbPerformingInvite; }       // +0x4B4F
        void SetOnlineMatchRanked(bool lbRanked)     { mbOnlineMatchRanked = lbRanked; }     // +0x4B51
        void SetOnlineMatchUnranked(bool lbUnranked) { mbOnlineMatchUnranked = lbUnranked; } // +0x4B52
        void SetOnlineStartPending(bool lbPending)   { mbOnlineStartPending = lbPending; }   // +0x4B53
        bool IsOnlineStartPending() const     { return mbOnlineStartPending; }      // +0x4B53 (19283) CrashNavEnterOnlineBase Handle{Disconnected,OverlayComplete}Event lbz
        s32 GetPlayerActiveRaceCarIndex() const                  { return mePlayerActiveRaceCarIndex; }  // DWARF h:924

        // [hud reveal gate 2026-08-25] The console spells this accessor out by name in its own
        // assert text -- "( GuiPlayerEngineEvent::E_ENGINE_OFF == mpCache->GetPlayerEngineState( ))
        // || ( GuiPlayerEngineEvent::E_ENGINE_ON == mpCache->GetPlayerEngineState( ))"
        // (BrnFBurnMainHudState.cpp:1536) -- so the name is the console's, not invented. No
        // standalone X360 symbol: both HUD-state call sites inline the load
        // (`lwz r11,0x4B20(r11)`), exactly like the two index accessors above, so this is a
        // header inline too.
        s32 GetPlayerEngineState() const                         { return mePlayerEngineState; }

        // [gateui r3] ADDITIVE GROW -- the twin of the accessor above over the GLOBAL index
        // carved at +0x4B04 (see the member for the Construct/RecEvent pairing that pins it).
        // Consumer: HudMessageAnalyzer::HandleRaceCheckpointReached @0x8251B350. No standalone
        // X360 symbol (the console inlines the load), so this is a header inline like its twin.
        s32 GetPlayerGlobalRaceCarIndex() const                  { return mePlayerGlobalRaceCarIndex; }

        // [gateui r3] ADDITIVE GROW -- the active-landmark count carved at +0x5286. Returns u8:
        // the console STORES it with `stb` behind a `<= 512` assert, so the truncation is the
        // shipped width (see the member). No standalone X360 symbol (inlined at every read).
        u8 GetNumActiveLandmarks() const                         { return muNumActiveLandmarks; }

        // [gateui r3] @0x82443C50 -- maRaceCarDisconnected[index] @0xA0F4, front-guarded by the
        // same two range asserts its siblings carry (X360 builds "Invalid EActiveRaceCarIndex : "
        // + the index; BrnGuiCache.h:3904 / :3905).
        // FLAG (deliberate host boundary): the console emits this as an OUT-OF-LINE body and the
        // tree's siblings (IsActiveRaceCarConnecting @0x82443B28, IsRaceCarCrashing @0x824438A8)
        // are bodied in BrnGuiCache_wB_05/_wB_06.cpp. It is inlined HERE instead so its one
        // consumer -- HudMessageAnalyzer::HandleEventFinisher, landing this round in
        // BrnGuiHudMessageAnalyzer_gUI_03.cpp -- does not acquire an unresolved external that
        // only an as-yet-unmounted GuiCache partfile could satisfy. Behaviour is identical; move
        // it to _wB_06.cpp next to its twin whenever that partfile is mounted.
        bool IsActiveRaceCarDisconnected(EActiveRaceCarIndex leActiveRaceCarIndex) const
        {
            CGS_ASSERT(0 <= leActiveRaceCarIndex, "Invalid EActiveRaceCarIndex");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "Invalid EActiveRaceCarIndex");
            return maRaceCarDisconnected[leActiveRaceCarIndex];
        }

        s32 GetActiveRoadRule() const                            { return meActiveRoadRule; }
        // [tut-ticker] the in-game ticker's controller-present read (see the member carve).
        s32 GetActiveControllerIndex() const                     { return miActiveControllerIndex; }

        // ADDITIVE GROW (BrnCarSelectMain wave G). The car-select flow surface. All three were
        // header-inlines on the X360 (no out-of-line bodies exist in the image); their bodies
        // are attested by the CarSelectMain call-site inlines cited on the members below.

        // DWARF h:1301 GetCurrentCarSelectType. The assert (string + BrnGuiCache.h:4378 line
        // baked into the X360 image) fires when no car-select flow is active; raw s32 return
        // per this header's enum convention (values: GsmIO::ECarSelectType,
        // BrnGameStateSharedIO.h).
        s32 GetCurrentCarSelectType() const
        {
            CGS_ASSERT(meCarSelectType > 0, "meCarSelectType > GsmIO::E_CAR_SELECT_TYPE_NONE");
            return meCarSelectType;
        }
        bool IsJunkyardCarUnlockPending() const          { return mbJunkyardCarUnlockPending; }  // +0x4B74 (19316)
        // [profile-save] the post-title intro-video gate (+0x4B78 -- see the member's carve
        // note). Console read site: PostTitleScreenLoad::Update; console write sites:
        // GuiCache::Construct (1) and ProfileManager::ReportTaskCompleted (= mbIsNewProfile).
        bool ShouldPlayIntroVideo() const                { return mbPlayIntroVideo; }            // +0x4B78 (19320)
        void SetPlayIntroVideo(bool lbPlay)              { mbPlayIntroVideo = lbPlay; }          // +0x4B78 (19320)

        // DWARF h:1425 SetDoDisconnectPopup(const CgsModule::Event*). Latch the error word the
        // disconnect popup shows: the event's leading word, or 0 when no event rode along. The
        // whole X360 body is the CarSelectMain event-44 inline @0x824D76C0 (positional read of
        // an external event blob -- its layout is fixed by the producer, not a C++ class here).
        void SetDoDisconnectPopup(const CgsModule::Event* lpDisconnectEvent)
        {
            meLastDisconnectedError =
                lpDisconnectEvent ? *reinterpret_cast<const s32*>(lpDisconnectEvent) : 0;
        }

        // DWARF h:1666 (member); the CarSelectMain event-93 ENTER_GAME gate reads it.
        bool IsStartingGameDueToPlayerJoin() const { return mbIsStartingGameDueToPlayerJoin; }

        // ADDITIVE GROW (BrnOdometerComponent TU). The odometer HUD caches the player profile
        // and reads the running offline distance off the cache. Both are inlined far-member
        // reads at the X360 call site (OdometerComponent::Construct @0x82415088 loads the
        // profile pointer from mpGuiCache+0x405C; Update @0x82424160 loads the distance float
        // from mpGuiCache+0x13B94), so exposing them by name keeps that leaf off raw offsets.
        // DECLARATION-ONLY per the far-member convention (bodies link from the GuiCache TU).
        const BrnProgression::Profile* GetProfile() const;   // X360 far member @0x405C
        f32 GetDistanceDriven() const;                        // X360 far member @0x13B94

        // ADDITIVE GROW (BrnPhotoBoothComponent / BrnLicenseComponent / BrnIntro TUs).
        // The latched BrnGui::GuiEventCamStatus word (X360 far member @0x13B58) -- see the
        // member's own note for the producer and the fifteen readers. Every X360 reader
        // inlines the raw far-member load and tests it against zero; exposing it by name is
        // what keeps those TUs off raw offsets.
        // DECLARATION-ONLY per the far-member convention (body links from the GuiCache TU).
        s32 GetCamStatus() const;                            // X360 far member @0x13B58

        // ADDITIVE GROW (BrnGui::FriendsListComponent TU). BuildShortcutOptions
        // @0x824145B0 gates the offline shortcut list's option-1 entry on this far byte
        // (`lbzx r10, cache, 0x13B9A`). DECLARATION-ONLY per the far-member convention.
        bool GetOfflineShortcutProgressGate() const;          // X360 far member @0x13B9A

        // ADDITIVE GROW (BrnCarSelectVehicle TU). The car-select "transition already shown"
        // gate at X360 far member @0x13B5E -- see the member's own note. Header-inline (a
        // single byte read; the X360 reader inlines the raw far-member load too).
        bool GetCarSelectTransitionAlreadyShown() const { return mbCarSelectTransitionAlreadyShown; }

        // ADDITIVE GROW (BrnCarSelectLivery TU). Its OnLeave @0x824D6C30 clears the same far
        // member on the way out (`lis r10,1 / ori r10,r10,0x3B5E / stbx r9(0), cache, r10`),
        // so the screen that consumed the one-shot transition gate is the one that re-arms it.
        // The SET side is BrnGui::Intro::OnLeave @0x824D1640 (`stbx 1`) -- see the member's
        // own note for the full writer/reader roster.
        void SetCarSelectTransitionAlreadyShown(bool lbShown)
        {
            mbCarSelectTransitionAlreadyShown = lbShown;
        }

        // ADDITIVE GROW (BrnCarSelectLivery TU). The params-mirror game mode
        // BrnGui::CarSelectLivery::CanCarBePainted @0x824B52E0 gates on (it reads
        // `*(mpGuiCache + 43008 + 440)`, i.e. the meGameMode slot of the +0xA800 mirror, and
        // refuses to paint in modes 12 and 17). Header-inline, matching the raw far-member
        // load the X360 emits.
        s32 GetOnlineGameMode() const { return meOnlineGameMode; }

        // ADDITIVE GROW (the GUI per-frame time pump). The cache LEADS with the embedded
        // GuiEventTimeInfo pair (mfTimeStep @+0x00, mfTimeNow @+0x04) and every GUI-side timer
        // in the game reads it -- Intro::HandleStateTransitions @0x824DAA48 does
        // `lfs f13, 0(mpGuiCache)` for its two 2 s dwells, LicenseComponent::Update @0x8243C0B8
        // does `**(this+168)` for its rank tick-up, and so on. The pair is refreshed once a
        // frame from GUI event 26 (CgsGui::GuiEventTimeInfo, payload { delta, now }, 8 bytes,
        // produced by BridgeGameStateToGui @0x823EF300).
        // ⚠ The X360's latch is NOT in GuiCache::RecEvent -- that function was walked
        // instruction by instruction this wave and stores nothing to +0x00/+0x04 in any of its
        // 240 cases -- so it happens inside GuiModule::Update @0x82527A58, the only other
        // per-frame owner of the cache. This is the named PC face of that latch; BrnGuiModule
        // calls it from its own input drain, at the same point in the frame.
        void RecTimeInfo(const CgsGui::GuiEventTimeInfo* lpTimeInfo);

        // @0x824EE7B0 / @0x824EE7C0 -- the player-name LANGUAGE-DATABASE STRING IDs, not the
        // name text. GuiModule::UpdatePlayerName @0x824F0D30 fetches the signed-in gamertag
        // (XUserGetName, or the "DEFAULTPLAYERNAME" database entry when that fails) and
        // registers it in the language manager UNDER these ids
        // (LanguageManager::AddString(mgr, id, name)); every consumer then resolves the id.
        // The X360 bodies are a single load of a .data const char* (off_82F278AC /
        // off_82F278B0) and never touch `this`. Consumers: LicenseComponent::SetPlayerInfo
        // via Intro / CrashNavDriverDetails / CompletedGame / ReplayCredits, RoadPanel,
        // CrashNavPanel (GetPlayerName); RoadRuleComponent, InGameMessageRenderer
        // (GetPlayerNameInQuotes). Bodies in BrnGuiCache.cpp.
        const char* GetPlayerName() const;
        const char* GetPlayerNameInQuotes() const;

        // The player-options profile block (X360 far member @0xB878/47224 -- past the
        // modelled tail; both CrashNavOptions::SetSettingsFromProfile @0x824B8028 and
        // OnlineGameRoomPlayerInfo::ShowSettingsOptions @0x82485140 inline the fetch).
        // DECLARATION-ONLY per the far-member convention (body links from the GuiCache TU).
        OptionsDataProfile* GetOptionsDataProfile();   // X360 far member @0xB878

        // The live DLC1 options block that sits directly after it (X360 far member
        // @0x12BE8/76776; ReadProfileData @0x824FF298 inlines the fetch the same way).
        OptionsDataProfileDLC1* GetOptionsDataProfileDLC1() { return &mOptionsDataProfileDLC1; }
        const BurnoutSkillsManager* GetBurnoutSkillsManager() const { return mpSkillsManager; }

        // DWARF h:1203 -- the checkpoint count for the current event (muCheckpointsInEvent).
        // ADDITIVE GROW: real X360 symbol (called by RenderValue @0x82422030);
        // declaration-only (bodied with the GuiCache accessor TUs).
        u8 GetCheckpointsInEvent() const;
        const BrnResource::HudMessageController* GetHudMessageController() const;  // X360 @0x82472D00
        const HudMessageDirector*       GetHudMessageDirector() const;        // X360 @0x82472D58

        // [gateui r2] DWARF h:2221/:2224 -- the two setters that pair with the getters
        // above. The X360 INLINES both into BrnGui::GuiModule::Construct @0x82518028
        // (line 369 `*(gm + 1021872) = a2` with the assert "lpController"
        // BrnGuiCache.h:2405, and line 376 `*(gm + 1021876) = gm + 639264` with the
        // assert "lpDirector" BrnGuiCache.h:2433), which is why no standalone body
        // exists in the export set -- the ASSERT TEXT + the baked header line numbers
        // are the attestation. Bodied inline here, as the console has them.
        void SetHudMessageController(const BrnResource::HudMessageController* lpController)
        {
            CGS_ASSERT(lpController != 0, "lpController");   // BrnGuiCache.h:2405
            mpHudMessageController = lpController;
        }
        void SetHudMessageDirector(const HudMessageDirector* lpDirector)
        {
            CGS_ASSERT(lpDirector != 0, "lpDirector");       // BrnGuiCache.h:2433
            mpHudMessageDirector = lpDirector;
        }

        // [gateui r2] FLAG: ADDITIVE HOST ACCESSOR -- no X360 symbol. The console never
        // needs to ask: its HUD-message controller is always loaded by the time
        // GuiModule::Update reaches the SetController leg (@0x82527A58 line 912 asserts it
        // outright). This build has no producer for one yet, so the module's leg must be
        // able to TEST the pointer without tripping GetHudMessageController's non-null
        // assert every frame. Same treatment, and same justification, as GetProfile().
        // DELETE-WHEN a HudMessageController producer lands.
        bool HasHudMessageController() const { return mpHudMessageController != 0; }

        // [gateui r4] CE-4. The console has NO accessor for this -- BrnFBurnMainHudState::
        // OnEnter @0x8247B0E8 forms `cache + 16512` by hand and hands it to
        // InGameMessagesComponent::SetInGameMessagesQueue. This tree does not use console
        // byte offsets, so the member needs a name to travel through. ADDITIVE, and the only
        // way the by-value member is reachable from outside.
        InGameMessagesQueue* GetInGameMessagesQueue() { return &mInGameMessagesQueue; }

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

        // ====================================================================
        //  Remaining GuiCache accessors recovered in this wave (dossier: 60
        //  funcs). All read/index the named far members carved into the layout
        //  below at their asm-proven offsets; bodies link from the GuiCache TU.
        //  Trailing comments give the X360 address + the member each one hits.
        // ====================================================================

        // --- per-event scalar accessors (mode/score/time/distance) ---
        f32 GetDistanceInEvent() const;             // X360 @0x8240F398 (mfDistanceInEvent @0x9F48, >= 0)
        BrnGameState::LandmarkIndex
            GetEventDestinationLandmarkIndex() const; // X360 @0x8240FA88 (mEventDestinationLandmarkIndex @0x9F4C)
        BrnGameState::LandmarkIndex
            GetEventFinishLandmark() const;         // X360 @0x824EC610 (maCheckpointLandmarks[count-1]; see the array's note)
        BrnGameState::LandmarkIndex
            GetOnlineLandmarkIndex(u32 luCheckpointIndex) const; // X360 @0x8240FB50 (miOnlineRoundIndex @0xA7FC + maOnlineGameModeOptions @0xA800)

        // --- race-car info block (ARCI-indexed, mRaceCarInfo SoA @0xA020..) ---
        const Vector4& GetRaceCarPosition(EActiveRaceCarIndex leActiveRaceCarIndex) const; // X360 @0x82443750 (maRaceCarPositions @0xA020)
        s32  GetEventPositionOfRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex) const;    // X360 @0x82443D78 (maEventPositionOfRaceCar @0xA130, gate @0xA140)
        bool HasRaceCarFinished(EActiveRaceCarIndex leActiveRaceCarIndex) const;           // X360 @0x824EC4C8 (maRaceCarFinished @0xA138, gate @0xA140)
        bool IsActiveRaceCarIndexUsed(EActiveRaceCarIndex leActiveRaceCarIndex) const;     // X360 @0x82443A00 (maRaceCarUsed @0xA0E4)
        bool IsActiveRaceCarConnecting(EActiveRaceCarIndex leActiveRaceCarIndex) const;    // X360 @0x82443B28 (maRaceCarConnecting @0xA0EC)
        bool IsRaceCarCrashing(EActiveRaceCarIndex leActiveRaceCarIndex) const;            // X360 @0x824438A8 (maRaceCarCrashing @0xA104, used-gate @0xA0E4)

        // --- online player state tables (ARCI-indexed, @0xB84C..) ---
        bool GetOnlinePlayerDisconnected(EActiveRaceCarIndex leActiveRaceCarIndex) const;  // X360 @0x8240F988 (maOnlinePlayerDisconnected @0xB84C)
        bool GetOnlinePlayerInCarSelect(EActiveRaceCarIndex leActiveRaceCarIndex) const;   // X360 @0x824436D0 (maOnlinePlayerInCarSelect @0xB854)
        bool IsOnlinePlayerEliminated(EActiveRaceCarIndex leActiveRaceCarIndex) const;     // X360 @0x8240FA08 (maOnlinePlayerEliminated @0xB85C)

        // --- road-rule / scoring-traffic / stunt / preset tables ---
        bool IsRoadRuleActive(s32 liRoadRuleType) const;   // X360 @0x82472E78 (maRoadRuleActiveByType @0xAC44, idx 0..1)
        s32  GetScoringTrafficCount() const;               // X360 @0x824497C0 (miScoringTrafficCount @0xA3D0)
        const BrnTraffic::ScoringTrafficData*
            GetScoringTrafficData(u32 luIndex) const;      // X360 @0x82450718 (maScoringTrafficData @0xA150)
        const StuntToDisplayInfo* GetStuntToDisplay(s32 liIndex) const; // X360 @0x8240F770 (maStuntToDisplay @0xAC5C)
        const PresetRace* GetPresetRace(s32 liPresetRaceIndex) const;   // X360 @0x824B2FE8 (maPresetRaces @0x4FB0, count miNumPresetRaces @0x5280)
        u32  GetNumOnlineFinishPoints() const;             // X360 @0x8241E7D8 (sum of per-word 64-bit popcounts over maOnlineFinishPointsMask @+0x7770, 4 doublewords)

        // --- replay slot / player tables ---
        bool IsReplayARCRendered(EActiveRaceCarIndex leActiveRaceCarIndex) const;  // X360 @0x824EEDA8 (maReplayARCRendered @0x143C0)
        s32  ReplayConvertGuiSlotIndexToReelIndex(s32 liSlotIndex) const;          // X360 @0x824EEBE0 (maReplayReelForSlot @0x13BA0, miReplaySlotsUsed @0x13BB8)
        const CgsNetwork::PlayerName*
            GetSortedReplayPlayerActive(u32 luIndex) const;                        // X360 @0x824EEFA0 (maReplayPlayersActive @0x143A0, gated on mbReplayHasBeenSorted @0x143F8)
        void ClearReplayPlayerActive();                                            // X360 @0x824EEE28 (clears maReplayPlayersActive/maReplayARCRendered, mbReplayHasBeenSorted=0)
        void IncrementReplayPlayerActive(const char* lpcPlayerName, s32 liValue);  // X360 @0x824EEEC0 (maReplayPlayersActive lookup/append)
        void SortReplayPlayersActive();                                            // X360 @0x824F8C58 (qsort maReplayPlayersActive; sets mbReplayHasBeenSorted)
        static s32 _SortReplayPlayersActiveByCount(const void* lpA, const void* lpB); // X360 @0x824EF028 (qsort comparator on entry +0x18 count)

        // --- misc setters / sat-nav / car-unlock ---
        void SetMapIconManager(MapIconManager* lpMapIconManager); // X360 @0x824EC3C8 (mpMapIconManager @0x4060)
        void ZoomSatNavOut();                                     // X360 @0x82472FD0 (miSatNavZoomLevel @0x803C)
        void DetermineCarUnlockPending(BrnProgression::Profile* lpProfile); // X360 @0x824EC678 (sets mbCarUnlockPending/mbCarUnlockDetermined @0x4B75/0x4B76; reads Profile far members -- BrnProfile.h boundary)

        // [H3c] ADDITIVE GROW (MapIconManager::UpdateWorldIcons @0x82511C88, which inlines
        // both byte loads): the nearest-junkyard pass gate (+0x4B75, `lbz` -- an un-shown
        // unlocked car volunteers the junkyard icon) and the nearest-body-shop pass gate
        // (+0x4B26; see the member's carve note). Console-inlined reads; named faces per
        // the cluster precedent above.
        bool IsCarUnlockPending() const       { return mbCarUnlockPending; }      // +0x4B75
        bool GetShowNearestBodyShop() const   { return mbShowNearestBodyShop; }   // +0x4B26

        // ====================================================================
        //  ADDITIVE GROW (wave J: PreRaceFlyByState + CrashNavMap). The pre-race
        //  fly-by / crash-nav map surface. Split by how the X360 built each face:
        //  the two `bl` targets are DECLARATION-ONLY (real out-of-line X360 symbols,
        //  bodies link from the GuiCache TU); the rest have NO X360 symbol at all --
        //  every call site inlines a raw far-member load/store -- so they are
        //  header-inlines over the named members carved below, exactly like the
        //  GetGuiTracker / GetGameMode / GetOnlineGameMode precedents above.
        // ====================================================================

        // DWARF BrnGuiCache.h:993. Re-publish the map state after the fly-by tears its
        // screen down. X360 out-of-line, called by PreRaceFlyByState::OnLeave
        // (`bl` @0x824C6AE8). Body: BrnGuiCache_wMap.cpp (map arm 2026-08-27).
        void RefreshMapState();

        // [map arm 2026-08-27] the tracker-refresh worker RefreshMapState's offline arm
        // tail-calls (@0x82510F7C) and GuiCache::RecEvent case 112's Burning-Home-Run arm
        // calls (@0x825105F8): resolve `liNumLandmarks` u16 landmark indices to their icon
        // records and publish the whole set to the GuiTracker as one id-232 SetTracker
        // event. X360 @0x82506F28. Body: BrnGuiCache_wMap.cpp.
        void UpdateTrackerInfo(const u16* lpLandmarkIndices, s32 liNumLandmarks);

        // [map arm 2026-08-27] the active-landmark latch: copy the event's u16 landmark
        // list into maActiveLandmarks (+0x5288) and its count into muNumActiveLandmarks
        // (+0x5286, BYTE store -- the console truncates a > 255 list; keep it). Asserts
        // count <= KI_MAX_LANDMARKS_IN_GAME (512). X360 @0x824EE7D0, called by the
        // HACK_...SetActiveLandmarksByEventID body and (on console) the id-231 RecEvent
        // arm. Body: BrnGuiCache_wMap.cpp.
        struct SetActiveLandmarksEvent
        {
            u32 muNumLandmarks;                    // +0x00
            u16 maLandmarkIndices[512];            // +0x04 (KI_MAX_LANDMARKS_IN_GAME)
        };
        void HandleSetActiveLandmarksEvent(const SetActiveLandmarksEvent* lpEvent);

        // DWARF BrnGuiCache.h:1476 -- EXACT signature (`int32_t (uint32_t, float32_t, bool)`).
        // Re-latch the cache's active-landmark set for one event id at animation parameter
        // lfT, returning the resulting active-landmark count. X360 out-of-line, called by
        // PreRaceFlyByState::UpdateIconManager (`bl` @0x824C7C00) and CrashNavMap::UpdateEvent.
        // The PPC float-arg ABI is why the trailing bool is invisible in Hex-Rays: at the
        // call site r4 = GetEventID(), f1 = the clamped animation t (the float SKIPS its GPR
        // slot, so r5 is dead) and r6 = 0. The result is compared against the screen's
        // miPreviousIconCount, so it is the s32 count. DECLARATION-ONLY.
        s32 HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID(u32 luEventID, f32 lfT,
                                                                   bool lbFlag);

        // DWARF BrnGuiCache.h:780 -- the map-icon manager SetMapIconManager latched.
        // X360-INLINED at every call site (`lwz r11, 0x4060(cache)`, e.g.
        // PreRaceFlyByState::OnEnter @0x824C679C, CrashNavMap::ResetIconManager).
        MapIconManager* GetMapIconManager() const                { return mpMapIconManager; }

        // DWARF BrnGuiCache.h:987 / :990 -- the current event's id and its junction id.
        // X360-INLINED as far-member word loads (`lwzx r4, cache, 0x9E5C` /
        // `lwzx r11, cache, 0x9E60`, PreRaceFlyByState::UpdateIconManager @0x824C7BDC /
        // @0x824C7C8C; SetupComponents and the five Set*Description workers read the
        // event id the same way).
        u32 GetEventID() const                                   { return muEventID; }
        u32 GetJunctionID() const                                { return muJunctionID; }

        // DWARF BrnGuiCache.h:1215 (member mbIsPreRaceFlyByActive, DWARF h:569). The
        // fly-by-active gate. X360-INLINED as a far-member byte store: OnEnter
        // @0x824C6718 stores 1 and OnLeave @0x824C6BC4 stores 0 (`stbx r, cache, 0xA015`).
        void SetPreRaceFlyByActive(bool lbActive)                { mbIsPreRaceFlyByActive = lbActive; }

        // DWARF BrnGuiCache.h:1122 -- EXACT signature (`float32_t (ECurrentMedalTargetTime)`,
        // declared NON-const there). The per-medal score target for the current event.
        // X360-INLINED as `ori r9, 0x9F34` + `lfsx` at PreRaceFlyByState::
        // SetRoadRageDescription @0x824C70C0 and ::SetFreestyleDescription @0x824C727C
        // (both with the constant GOLD index 0). The value is a FLOAT: those two callers
        // convert it with `fctiwz`/`stfiwx` before handing it to SPrintf's "%d", which is
        // why the printed target looks like an integer at the call site.
        f32 GetRequiredScoreForMedal(BrnGameState::ECurrentMedalTargetTime leMedal)
        {
            return mafTargetScores[leMedal];
        }

        // ADDITIVE GROW (BrnCrashNavMap wave J): the local player's car id. X360-INLINED as
        // an 8-byte far-member load (`ld r11, 0x4AF0(cache)` @0x824CBCB0) in CrashNavMap::
        // UpdateIconManager, which copies it into the screen's mHoveringRivalId when the map
        // cursor snaps onto the local-player icon. FLAG: consumer-named -- only the WIDTH
        // (8 bytes), the OFFSET (+0x4AF0) and that one use are measured; no DWARF member row
        // can be pinned to this X360 offset.
        CgsID GetLocalPlayerCarId() const                        { return mLocalPlayerCarId; }

        // The ORIGINAL car id twin (see the +0x4AF8 carve note; FBurnMainHudState case 311's
        // `ld r5, 0x4AF8(cache)` is the attested whole-CgsID read this accessor names).
        CgsID GetLocalPlayerOriginalCarId() const                { return mLocalPlayerOriginalCarId; }

        // The district-marker source words (the +0x4FA0 carve): the X360 state reads the
        // three words raw off the cache (@0x8247B660 post-loop); these are the named PC
        // faces of those reads. The write-back goes through RecEvent(169).
        s32  GetChangeDistrictCounty() const                     { return meChangeDistrictCounty; }
        s32  GetChangeDistrictDistrict() const                   { return meChangeDistrictDistrict; }
        bool IsChangeDistrictConsumed() const                    { return mu8ChangeDistrictConsumed != 0; }

        // ADDITIVE GROW (BrnCrashNavMap wave J): the gate byte for the friend-selected
        // road-rule score prompts. X360-INLINED as a BYTE load (`lbz r11, 0x4B50(cache)` at
        // BOTH read sites, @0x824B6A50 and @0x824B6B3C, in CrashNavMap::UpdateButtonPrompts);
        // ANDed with CrashNavPanel::IsRoadRuleFriendSelected() it picks prompt state 11 over
        // state 9. FLAG: consumer-named -- the producer side is not reconstructed. (The byte
        // width is what makes this carve safe: an `lwz` here would have spanned
        // mbOnlineMatchRanked/mbOnlineMatchUnranked/mbOnlineStartPending at +0x4B51..53.)
        bool AreRoadRuleFriendScoresAvailable() const            { return mbRoadRuleFriendScoresAvailable; }

    private:
        // The HUD-message analyzer reads a handful of consumer-carved snapshot members
        // directly by name (mfTimeStep via GetTimeStep(), mbGameplayHudActive, the +0x4930 pending
        // cluster, miGameFlowState, miLastStuntScore): the X360 inlines the raw loads at
        // its Update / HandleWreckedEvent sites and the PS3 DWARF has no accessor rows
        // for these X360-only offsets, so friendship -- not a fabricated accessor
        // surface -- is the honest exposure. (HudMessageAnalyzer wave-C keystone.)
        friend struct HudMessageAnalyzer;

        // Same exposure rule for the online game-room screen (wave-H keystone): the
        // X360 inlines its raw loads/stores of the freeburn gate bytes (+0x4B57..5A),
        // the disconnect/invite bytes (+0x4B40/+0x4B4E/+0x4B4F/+0x4B53), the params
        // mirror (+0xA800..+0xA9DF memcpy target + field reads), the lobby mirror rows
        // (+0xB640), the host byte (+0xB864) and the world-camera vector (+0x4AE0) --
        // no DWARF accessor rows exist for these.
        friend struct OnlineGameRoomPlayerInfo;

        // Same exposure rule again for the create-match / game-options screen and the
        // sign-in screen (wave I): the X360 inlines their raw loads and stores of the
        // ranked/unranked and online-start bytes (+0x4B51..53), the game-mode options
        // storage, the options-changed byte (+0xA9E0) and the junkyard car-unlock byte
        // (+0x4B74). No DWARF accessor rows exist for these X360-only offsets.
        friend struct OnlineGameOptions;
        friend struct CrashNavEnterOnlineBase;

        // Same exposure rule once more for the online custom-match search screen (wave J):
        // ShowInitialScreen, HandleControllerInputSelectParams, HandleGuiCacheEvent and the
        // three back-out arms all read mbOnlineMatchRanked (+0x4B51, `lbz r11, 0x4B51(r30)`
        // @0x82497D38) and mbOnlineMatchUnranked (+0x4B52, @0x82497D50) as raw inlined byte
        // loads. The cluster has setters but NO getters in the DWARF, so friendship -- not a
        // fabricated accessor pair -- is the honest exposure. (Its IsOnlineStartPending /
        // SetOnlineStartPending / IsOnlineStartInProgress uses go through the public
        // accessors above.)
        friend struct OnlineCustomMatch;
        friend class BrnGui::FriendsListComponent;   // [friends wave]

        // [gateui] Same exposure rule for the HUD-message DIRECTOR: its
        // CheckMessageIsAvailable @0x824F2B28 inlines exactly two raw loads off the cache --
        // the game-mode word (`lwz r30, 0x9E58(r9)`, which picks the offline vs online
        // availability bit) and the game-flow state (`lwz r12, 0x4B30(r9)`, the "is the player
        // crashed" test). meGameModeType does have a public GetGameMode() accessor, but
        // miGameFlowState is one of the X360-only consumer-carved words with no DWARF
        // accessor row -- the same situation, and the same answer, as the analyzer above.
        friend struct HudMessageDirector;

        // [stunt-readout wave A8 2026-08-27] Same exposure rule once more for the in-race
        // event-info HUD component: UpdateStuntAttack @0x82429C08 inlines FIVE raw loads
        // off the cache that have no DWARF accessor row -- the id-428 stunt block
        // muCurrentStunts (`lwzx r11, r23, 0xAC64` @0x8242A628, tested & 0x100),
        // muAllStunts (`lwzx` @0x8242A6A8, tested & 0x4000), mfComboWarningTimeActive
        // (`lfsx f0, r23, 0xAC6C` @0x8242A04C -- a FLOAT truncated by fctiwz),
        // mbComboWarningActive (`lbzx r11, r23, 0xAC70` @0x8242A028) and the
        // maStuntToDisplay terminator walk (@0x8242A548, the GetNumberOfStuntsToDisplay()
        // the sibling GetStuntToDisplay @0x8240F770 also inlines and names in its own
        // assert text). The scored/timed members it reads DO have accessors and go through
        // them (GetTargetScoreInEvent / GetCurrentScoreInEvent / GetCurrentTimeInEvent /
        // GetCurrentComboInEvent / GetMultiplierInEvent / GetStuntToDisplay / GetTime).
        friend class BrnGui::EventInfoComponent;

        // ===================================================================
        //  DATA LAYOUT -- named anchors at asm-proven `this+offset`, gaps
        //  reserved with explicit padding (AGENTS.md "LAYOUT RECOVERY WITH
        //  PADDING"). Member NAMES/TYPES from DecFIGS DWARF (BrnGuiCache.h);
        //  every offset in a trailing comment is X360-attested. Only the members
        //  the recovered accessors touch are named; the object is a large plain
        //  aggregate (no vptr) so the first member sits at offset 0.
        // ===================================================================
        // The cache leads with the embedded per-frame GuiEventTimeInfo pair
        // (CgsGuiEventTypeDefs.h: mfTimeDelta @+0, mfTimeNow @+4). GetTimeStep reads
        // the delta word, GetTime the now stamp. (HudMessageAnalyzer::Update
        // @0x82525FC0 accumulates the same leading delta word into its message timers.)
        f32 mfTimeStep;                                  // +0x0000 (0)     GetTimeStep (the frame delta)
        f32 mfTimeNow;                                   // +0x0004 (4)     GetTime, != -FLT_MAX
        // +0x0008..+0x405B -- the embedded resource/component watcher fills this span
        // (X360: the GuiCache::EnsureResource* forwarders @0x824FEB50/58 are
        // `addi r3,r3,8` + tail-branch into the helper; the ctor's -1 store at +0x0ED8
        // is the helper's own CgsArray sentinel). Widens on x64; access by name.
        StateLoadingHelper mStateLoadingHelper;          // +0x0008 (spans ..+0x4053 on X360)
        // ADDITIVE CARVE (OnlineGameRoomPlayerInfo keystone, wave H): the sat-nav GUI
        // tracker pointer. X360 +0x4054 (16468) -- the "mpGuiCache->GetGuiTracker()"
        // assert + GuiTracker::ClearTracker(*(cache+0x4054)) in the screen's
        // HandleGuiCacheEvent @0x824A3F60 region. One X360 word (+0x4058) stays
        // unclaimed between this and mpProfile.
        GuiTracker*               mpGuiTracker;          // +0x4054 (16468)
        u8  mPad_4058[4];                                // +0x4058..+0x405B (unclaimed word)
        BrnProgression::Profile*  mpProfile;             // +0x405C (16476) OdometerComponent::Construct @0x82415088 (mpGuiCache+0x405C); DetermineCarUnlockPending source
        MapIconManager*           mpMapIconManager;      // +0x4060 (16480) SetMapIconManager @0x824EC3C8 (v3[4120]=a2)
        WorldDataController*      mpWorldDataController;  // +0x4064 (16484)
        const BurnoutSkillsManager* mpSkillsManager;      // +0x4068 (16488) DWARF h:1632 (PlayerPositionSingle::RenderValue @0x824223FC)
        FreeburnChallengeManager* mpChallengeManager;    // +0x406C (16492)
        // [gateui r2] both pointers are `const` in the DWARF (h:1674/:1677) and both
        // getters already returned const; the setters above are the only writers.
        const BrnResource::HudMessageController* mpHudMessageController; // +0x4070 (16496)
        const HudMessageDirector* mpHudMessageDirector;   // +0x4074 (16500)
        u8  mPad_4078[4];                                // +0x4078..+0x407B
        // ADDITIVE CARVE (HudMessageAnalyzer keystone): the gameplay-HUD gate byte the
        // analyzer's Update checks before firing crash/challenge/trophy/player-left
        // messages (lbz mpGuiCache+0x407C, X360 @0x82527668/0x825276B0/...). FLAG:
        // consumer-named.
        bool mbGameplayHudActive;                        // +0x407C (16508)
        u8  mPad_407D[0x4080 - 0x407D];                  // +0x407D..+0x407F (alignment tail)

        // ⭐ [gateui r4] CE-4: THE IN-GAME MESSAGE QUEUE'S REAL HOME, replacing the round-2
        // "pending-status cluster" carve (miPendingEventStatusA/B @+0x4930/+0x4934 and
        // mu64PendingEventPayload @+0x4938) that ALIASED three of its members.
        //
        // PROOF, from the X360 asm of HudMessageAnalyzer::Update @0x82525FC0, the wire-id
        // 291/320 arm (0x82526B08..0x82526B44) -- the console forms the queue base ITSELF:
        //     0x82526B08  lwz   r11, 4(r31)          ; mpGuiCache
        //     0x82526B0C  addi  r11, r11, 0x4080     ; == 16512 == &mInGameMessagesQueue
        //     0x82526B10  lwz   r10, 0x8B0(r11)      ; maeMessageState[0]   (16512+2224 = 18736)
        //     0x82526B14  std   r25, 0x8B8(r11)      ; muCurrentEventEndTime = 0  (r25 is
        //                                            ;   `li r25, 0` @0x82525FE0)
        //     0x82526B28  stw   r25, 0x8B0(r11)      ; maeMessageState[0] = 0, if it was 1 or 2
        //     0x82526B2C  lwz   r10, 0x8B4(r11)      ; maeMessageState[1]   (16512+2228 = 18740)
        //     0x82526B40  stw   r25, 0x8B4(r11)      ; maeMessageState[1] = 0, if it was 1 or 2
        // 0x8B0 / 0x8B4 / 0x8B8 are exactly maeMessageState[0], maeMessageState[1] and
        // muCurrentEventEndTime (BrnInGameMessagesComponent.h). So the "unrecovered
        // producer-side semantics" the carve's FLAG admitted to are simply the message
        // component's own slot machine, and the arm means "cancel any WAITING/TRANSIN slot
        // and drop the live expiry".
        //
        // The console binds it into the component from BrnFBurnMainHudState::OnEnter
        // @0x8247B0E8 with the same base -- `SetInGameMessagesQueue(cache + 16512)` -- which
        // is what makes the two views one object.
        //
        // Host width: BrnResource::HudMessageEvent is POINTER-FREE (CgsID + s32/f32 + char
        // arrays + HudMessageParameter), so sizeof is the console's 0x458 on x64 too and the
        // whole queue is exactly 0x8C8 bytes here as well -- the one place in this header
        // where a console span survives the widening intact. Access stays BY NAME regardless.
        InGameMessagesQueue mInGameMessagesQueue;        // +0x4080 (16512), spans ..+0x4947
        u8  mPad_4948[0x4AE0 - 0x4948];                  // +0x4948..+0x4ADF
        Vector4 mv4WorldCameraPosition;                  // +0x4AE0 (19168) GetWorldCameraPosition (SatNavRenderer @0x8245FA48 lvx128 mpGuiCache,0x4AE0)
        // ADDITIVE CARVE (BrnCrashNavMap wave J): the leading 8 bytes of the former
        // mPad_4AF0[16] -- the local player's car id. X360-attested as an `ld` (8-byte load)
        // at +0x4AF0 in CrashNavMap::UpdateIconManager @0x824CBCB0, `std`'d straight into
        // the screen's mHoveringRivalId slot. No member is shifted (8 + 8 == 16).
        // FLAG: consumer-named -- no DWARF member row can be pinned to this X360 offset.
        CgsID mLocalPlayerCarId;                         // +0x4AF0 (19184)
        // ADDITIVE CARVE (HUD H1 wave, 2026-08-25): the trailing 8 bytes of the same former
        // 16-byte pad -- the local player's ORIGINAL car id (the pre-conversion id
        // GetOriginalCarId maps the live id back to). X360-attested as a PAIR at both ends:
        // GuiCache::RecEvent @0x8250DDF0 case 415 writes both back to back
        // (`*(+19184) = event CgsID; *(+19192) = GetOriginalCarId(it)`), and
        // FBurnMainHudState::UpdateRunning case 311 @0x8247C270 reads it WHOLE
        // (`ld r5, 0x4AF8(cache)`) as JunctionInfoComponent::HandleJunctionChange's
        // lCurrentCarId (the burning-route "is this the player's route car" compare).
        // FLAG: consumer-named; no PC producer yet (case 415 needs GetOriginalCarId,
        // unreconstructed) -- reads kCGSID_NULL(0) on this build, so the mode-5 compare
        // simply never matches. No member is shifted (8 + 8 == 16).
        CgsID mLocalPlayerOriginalCarId;                 // +0x4AF8 (19192)
        s32 mePlayerActiveRaceCarIndex;                  // +0x4B00 (19200) EActiveRaceCarIndex (DWARF h; HudMessageAnalyzer::HandleLiveRevengeUpdate @0x8251E2xx)
        // [gateui r3] ADDITIVE CARVE from the head of the former mPad_4B04[0x2C] -- the local
        // player's GLOBAL race-car index, the twin of the ACTIVE index at +0x4B00. Pinned as a
        // PAIR at both ends: GuiCache::Construct @0x82505860 seeds `*(a1+19200) = -1;
        // *(a1+19204) = -1;` back to back, and GuiCache::RecEvent @0x8250DDF0 writes them from
        // one event record (`*(+19200) = *rec; *(+19204) = *(rec+4)`). Consumer:
        // HudMessageAnalyzer::HandleRaceCheckpointReached @0x8251B350 (`lwz 4(event)` compared
        // against `lwz 0x4B04(cache)`) -- which is why the event carries BOTH indices.
        // No member is shifted (4 + 0x28 == 0x2C). FLAG: consumer-named.
        s32  mePlayerGlobalRaceCarIndex;                  // +0x4B04 (19204) EGlobalRaceCarIndex
        // [hud H3b tracking slice 2026-08-25] the GuiPlayerInfo tail carved from the
        // head of the former mPad_4B08[0x28]. Producers: GuiCache::RecEvent case 147
        // (`{+19208,+19212,+19216} = the {speed,rpm,gear} words`) and case 199's
        // player-icon arm (`+19220 = icon mfRotation; +19224/+19228 = county/district`).
        // Consumers: SatNavComponent's GuiPlayerInfo view over +0x4AE0 (miSpeedMph
        // @+0x28 == +0x4B08, mfOrientation @+0x34 == +0x4B14). No member is shifted
        // (0x18 + 0x10 == 0x28).
        s32  miPlayerSpeedMph;                            // +0x4B08 (19208) case 147 word 0
        s32  miPlayerRPM;                                 // +0x4B0C (19212) case 147 word 1
        s32  miPlayerGear;                                // +0x4B10 (19216) case 147 word 2
        f32  mfPlayerOrientation;                         // +0x4B14 (19220) case 199 (icon mfRotation)
        s32  mePlayerCounty;                              // +0x4B18 (19224) case 199 GetCounty
        s32  mePlayerDistrict;                            // +0x4B1C (19228) case 199 GetDistrict
        // [hud reveal gate 2026-08-25] THE PLAYER ENGINE STATE. Carved out of the head of
        // the former mPad_4B20[6]. TRIPLE-WITNESSED IN THE IMAGE at +0x4B20 (19232):
        //   producer  GuiCache::RecEvent @0x8250DDF0 -- the ONLY store to this word in the
        //             whole ~180-case switch: `stw r11, 0x4B20(r31)` @0x8251017C
        //             (pseudocode `else if (a3 == 379) *(a1 + 19232) = *a2;`)
        //   consumer  FBurnMainHudState::UpdateWFInit  @0x8247C7EC/@0x8247C820
        //             `lwz r11,0x140(r31) ; lwz r11,0x4B20(r11)`  (r31+0x140 == mpGuiCache)
        //   consumer  FBurnMainHudState::UpdateRunning @0x8247BD14 (the case-215 boost arm)
        // ⚠️ OFFSET CORRECTION, DO NOT REVERT: every source comment in this tree that called
        // this word "X360 cache word +19220" was reading Hex-Rays' `mpGuiCache->gapC[19220]`
        // as an object-relative index. It is gapC-RELATIVE -- IDA's gapC starts at +12, and
        // 12 + 19220 == 19232 == the 0x4B20 the asm actually encodes. +0x4B14 (19220) is
        // mfPlayerOrientation, declared six lines above and already named by the H3b wave.
        // Values are BrnGui::GuiPlayerEngineEvent's own pair: 0 == E_ENGINE_OFF,
        // 1 == E_ENGINE_ON (UpdateWFInit asserts `< 2` on it, BrnFBurnMainHudState.cpp:1536).
        // Construct @0x82505860 seeds it 0 -- so the HUD composes on its INVISIBLE frame and
        // stays there until the world posts the ignition. That is the console's reveal gate.
        s32  mePlayerEngineState;                         // +0x4B20 (19232) case 379
        // ADDITIVE CARVE ([stuntrace wS2] wave, 2026-08-27) out of the whole former
        // mPad_4B24[2] -- the two bytes RaceMainHudState::UpdateRunning @0x8247E898 drives
        // the race-position indicator from. Both are BYTE loads, back to back:
        //     0x8247FEE4  lbz r4,  0x4B24(r11)   ; the position VALUE (goes straight into r4,
        //                                        ;   SetPosition's argument; 0 -> SetVisible(0))
        //     0x8247FF18  cmplwi cr6, r4, 8      ; and it is range-checked 1..8
        //     0x8247FEF8  lbz r11, 0x4B25(r11)   ; the override that skips that range/enable check
        // No member is shifted (1 + 1 == 2). FLAG: consumer-named -- neither producer is
        // recovered on this build (both read 0, i.e. no position indicator).
        u8   mu8PlayerRacePosition;                       // +0x4B24 (19236) 1..8; 0 == none
        bool mbPlayerRacePositionOverride;                // +0x4B25 (19237)
        // ADDITIVE CARVE (H3c MapIconManager::UpdateWorldIcons @0x82511C88): the byte gating
        // the "show the nearest body shop on the sat-nav" pass (`lbz mpGuiCache+0x4B26`; the
        // pass runs when this byte is set AND the mode is freeburn (-1/15), or unconditionally
        // in road-rage / marked-man). FLAG: consumer-named -- the producer side is unrecovered
        // (reads 0 on this build: no nearest-body-shop icon is volunteered outside road rage).
        bool mbShowNearestBodyShop;                       // +0x4B26 (19238)
        u8   mPad_4B27[9];                                // +0x4B27..+0x4B2F
        // ADDITIVE CARVE (HudMessageAnalyzer keystone): the game-flow state word the
        // analyzer's Update gates its trigger passes on (lwz mpGuiCache+0x4B30; fire
        // only when it holds 1 or 3, treat -1 as invalid). FLAG: consumer-named -- the
        // enum home is unrecovered (values observed: -1 / 1 / 3).
        s32  miGameFlowState;                             // +0x4B30 (19248)
        // ADDITIVE CARVE (RecEvent case 132, @0x8250F5B0): the byte the game-flow-state
        // change clears when the NEW state is 1 or 3 (stb r10==0 +0x4B34, gated on the
        // cmpwi 1/3 pair). FLAG: producer-named -- the consumer side is unrecovered.
        u8   mu8GameFlowByte_4B34;                        // +0x4B34 (19252)
        u8   mPad_4B35[0x3];                              // +0x4B35..+0x4B37
        // ADDITIVE CARVE ([tut-ticker] wave, 2026-08-24): the active-controller index the
        // in-game ticker reads. X360-attested at BOTH InGameMessageRenderer read sites
        // (Update @0x82446F30 and RecvEvent case 505: `mbGamePausedForDisconnect =
        // (*(cache + 19256) == -1)`) -- -1 == no active controller == "reconnect controller"
        // ticker mode. FLAG: consumer-named (producer side unrecovered; reads 0 on this
        // build, i.e. controller present).
        s32  miActiveControllerIndex;                     // +0x4B38 (19256)
        u8   mPad_4B3C[0x4];                              // +0x4B3C..+0x4B3F
        // ADDITIVE CARVE (BrnCarSelectMain wave G): the last server-interface error word the
        // disconnect popup shows. DWARF h:1657 `CgsNetwork::EServerInterfaceError
        // meLastDisconnectedError` (order fit: right after miConsecutiveLosses in the DWARF
        // member run that lands this cluster); X360-attested by CarSelectMain::
        // ProcesssIncomingEvents' event-44 inline @0x824D769C (stw mpGuiCache+0x4B40 --
        // the DWARF SetDoDisconnectPopup(const CgsModule::Event*) body) and read by
        // InGame::Update's cache latch (BrnInGame.cpp CacheTakeLastDisconnectedError
        // boundary, which this member replaces when that TU is next touched). Raw s32 per
        // this boundary header's enum convention (see GetCurrentGameModeType).
        s32  meLastDisconnectedError;                     // +0x4B40 (19264) 0 == none
        u8   mPad_4B44[5];                                // +0x4B44..+0x4B48
        // ADDITIVE CARVE (pause wave, 2026-08-28) from the TAIL of the former
        // mPad_4B44[6] -- the cache's HIGH-DEFINITION byte. Five consumers read it, all
        // with the far-member `lbz +0x4B49` idiom, and all of them pick between an HD and
        // an SD layout constant: MainMapComponent::Construct @0x8245E5D4,
        // BootLegal::Update @0x824778D8, CrashNavDriverDetails::UpdateWFInit @0x824BFEB8
        // (HD -> KV2_LICENSE_POSITION, SD -> KV2_LICENSE_POSITION_SD),
        // CrashNavMapMain::HandleCrashNavInputPressed @0x824CCC74 and
        // RoadSignIconManager::Update @0x82517014. BrnMainMap.cpp:168 asked for exactly
        // this carve by name; it is made here so that its FLAG boundary can retire.
        // No member is shifted (5 + 1 == 6). FLAG: consumer-named -- there is NO writer
        // anywhere in the export set, so it reads 0 (== SD) on this build.
        bool mbIsHighDef;                                // +0x4B49 (19273)
        bool mbInEventColouringGate;                     // +0x4B4A (19274) RoadRuleComponent::ShouldUseInEventColouring gate byte
        u8   mPad_4B4B[1];                                // +0x4B4B
        // ADDITIVE GROW (BrnOnlinePlay TU): the online-play main-menu invite / online-start flag
        // cluster the online-play state reads/writes (X360 far bytes GuiCache+0x4B4C..+0x4B53).
        // Carved from the former mPad_4B4B WITHOUT shifting any following member. Names are inferred
        // from the OnlinePlay consumer; FLAG: consumer-named (no standalone DWARF for these bytes).
        bool mbOnlineStartInProgress;                    // +0x4B4C (19276) HandleControllerInputMainMenu confirm gate
        bool mbInviteInProgress;                         // +0x4B4D (19277) HandleGuiCacheEvent source
        // ADDITIVE CARVE (BrnCarSelectMain wave G): "this game start was driven by a
        // drop-in player join" gate. DWARF h:1666 mbIsStartingGameDueToPlayerJoin (the
        // bool run mbIsPreparingForInvite(h:1665)/THIS/mbIsPerformInviteReceived(h:1667)
        // brackets this byte; BrnInGame.cpp's CacheIsStartingGameDueToPlayerJoin boundary
        // comment records the same X360 +0x4B4E identification). X360-attested by
        // CarSelectMain::ProcesssIncomingEvents' event-93 gate @0x824D7784 (lbz
        // mpGuiCache+0x4B4E: blocks the ENTER_GAME state event while set).
        bool mbIsStartingGameDueToPlayerJoin;            // +0x4B4E (19278)
        bool mbPerformingInvite;                         // +0x4B4F (19279) HandleGuiCacheEvent source
        // MERGE RECONCILE PENDING: the loading-screen-visible byte (DecFIGS h:
        // IsLoadingScreenVisible; the accessor + BootProfile::OnLeave read it) was carved
        // at the SAME X360 byte (+0x4B4F) the OnlinePlay wave named mbPerformingInvite.
        // The x64 layout is name-based, so both live as distinct members until the DWARF
        // claim is re-verified; the IsLoadingScreenVisible accessor reads this one.
        bool mbIsLoadingScreenVisible;                   // +0x4B4F claim (19279) -- see note
        // ADDITIVE CARVE (BrnCrashNavMap wave J): the friend-selected road-rule score gate.
        // X360-attested as an `lbz` at BOTH read sites in CrashNavMap::UpdateButtonPrompts
        // (@0x824B6A50 / @0x824B6B3C) -- a BYTE, so this carve stops short of
        // mbOnlineMatchRanked @+0x4B51. FLAG: consumer-named (producer side unrecovered).
        bool mbRoadRuleFriendScoresAvailable;             // +0x4B50 (19280)
        bool mbOnlineMatchRanked;                        // +0x4B51 (19281) SelectOnlineMenuOption
        bool mbOnlineMatchUnranked;                      // +0x4B52 (19282) SelectOnlineMenuOption
        bool mbOnlineStartPending;                       // +0x4B53 (19283) SelectOnlineMenuOption (cleared)
        // ADDITIVE CARVE (boost-bar wave): the three per-frame "gameplay HUD data ready" bytes
        // the BoostBarRenderer::Update gate reads as a trio (X360 @0x82451CA4: lbz 0x4B54 &&
        // 0x4B56 && 0x4B58; FriendsListComponent::HandleControllerInput reads the same three).
        // PRODUCERS: +0x4B54 is published each frame by the console GuiModule::Update as
        // `(lUpdateSet & 8) != 0` (PS3 0x5A7C6C stores the equivalent cache+0x4AE6); the other
        // two bytes' producers are not yet recovered (FLAG: consumer-named). GuiCache::Construct
        // @0x82505AB8-D8 zeroes all three.
        bool mbGameplayUpdateActive;                     // +0x4B54 (19284) update-set bit 8
        u8   mPad_4B55[1];                               // +0x4B55 (set by PostTitleScreenLoad::Update)
        bool mbGameplayHudReadyB;                        // +0x4B56 (19286) FLAG consumer-named
        // ADDITIVE CARVE (OnlineGameRoomPlayerInfo keystone, wave H): the free-burn
        // input gate bytes the game-room screen reads (lbz, X360-attested widths).
        // FLAG: consumer-named -- producer-side semantics unrecovered.
        bool mbFreeBurnMenuLocked;                       // +0x4B57 (19287) blocks pause/map entry from freeburn (HandleControllerInputFreeBurnSubState)
        bool mbGameplayHudReadyC;                        // +0x4B58 (19288) FLAG consumer-named (see +0x4B54)
        bool mbFreeBurnInputDisabled;                    // +0x4B59 (19289) gates ALL freeburn controller handling
        bool mbOnlineEventCompleted;                     // +0x4B5A (19290) set when the online event ends; HandleGuiCacheEvent consumes it (ClearTracker + "TO_ST_POST" when meGameModeType==16), then clears it
        u8   mPad_4B5B[1];                               // +0x4B5B
        // ADDITIVE CARVE ([stuntrace wS2] wave, 2026-08-27) from the HEAD of the former
        // mPad_4B5B[0x15] -- the payback "award available" trio RaceMainHudState::UpdateWFInit
        // @0x82480200 reads as one group (asm @0x824805FC..0x82480620):
        //     lbz r10, 0x4B64(r11)   ; the gate -- nothing shown while clear
        //     lwz r4,  0x4B5C(r11)   ; -> ShowAvailableInstantly arg 1 (payback type; 3 is skipped)
        //     lwz r5,  0x4B60(r11)   ; -> ShowAvailableInstantly arg 2 (victim ARCI)
        // Widths are the asm's (two `lwz`, one `lbz`). The two words are spelled s32 per this
        // boundary header's enum convention (the real homes are BrnNetwork::EPaybackType and
        // ::EActiveRaceCarIndex, both in headers this one deliberately does not pull in).
        // No member is shifted (1 + 4 + 4 + 1 + 11 == 0x15). FLAG: consumer-named -- the
        // producer side is unrecovered (all three read 0 on this build, i.e. no payback).
        s32  mePaybackAvailableType;                     // +0x4B5C (19292) BrnNetwork::EPaybackType
        s32  mePaybackVictimRaceCarIndex;                // +0x4B60 (19296) ::EActiveRaceCarIndex
        bool mbPaybackAvailable;                         // +0x4B64 (19300)
        u8   mPad_4B65[0xB];                             // +0x4B65..+0x4B6F
        // ADDITIVE CARVE (BrnCarSelectMain wave G): which car-select flow is running.
        // DWARF h:1687 `BrnGameState::GameStateModuleIO::ECarSelectType meCarSelectType`;
        // the member NAME is baked into the X360 assert string "meCarSelectType >
        // GsmIO::E_CAR_SELECT_TYPE_NONE" (BrnGuiCache.h:4378) fired by the inlined
        // GetCurrentCarSelectType() in CarSelectMain::OnEnter @0x824C8B34 /
        // ExitCarSelection @0x824C8CF4 (both: lwz mpGuiCache+0x4B70). Raw s32 per this
        // boundary header's enum convention; the value home is
        // BrnGameState::GameStateModuleIO::ECarSelectType (BrnGameStateSharedIO.h).
        s32  meCarSelectType;                            // +0x4B70 (19312)
        // ADDITIVE CARVE (CrashNavEnterOnlineBase::HandleEnteringJunkyard @0x824CB070,
        // lbz mpGuiCache+0x4B74 == 1 -> "TO_CUNLOCK" else "TO_CSELECT"). FLAG:
        // consumer-named -- the producer-side semantics are unrecovered (it gates the
        // junkyard entry into the car-UNLOCK flavour of car select).
        bool mbJunkyardCarUnlockPending;                 // +0x4B74 (19316)
        // ADDITIVE GROW (BrnGuiCache DetermineCarUnlockPending @0x824EC678): the two car-unlock
        // pending flag bytes (Hex-Rays field_4B75 / field_4B76). FLAG: consumer-named.
        bool mbCarUnlockPending;                         // +0x4B75 (19317) set 1 when an un-shown unlocked car remains
        bool mbCarUnlockDetermined;                      // +0x4B76 (19318) set 1 on entry (determination has run)
        u8   mPad_4B77[0x4B78 - 0x4B77];                 // +0x4B77
        // ⭐⭐ ADDITIVE CARVE ([profile-save] wave, 2026-08-27) out of the head of the former
        // mPad_4B77 run -- THE POST-TITLE INTRO-VIDEO GATE. Three X360 sites pin it, and
        // together they are the console's own "a returning player does not watch the intro
        // again" rule:
        //   GuiCache::Construct              @0x82505860  `stb r11(1), 0x4B78(r31)` @0x82505AF0
        //                                                 -- default 1: a cold cache plays it.
        //   ProfileManager::ReportTaskCompleted @0x82513EC0 `stb r11, 0x4B78(r9)` @0x82514024,
        //                                                 the PROFILE_LOADED arm's last store:
        //                                                 `*(cache + 19320) = *(profile + 118033)`
        //                                                 == BrnProgression::Profile::mbIsNewProfile.
        //   PostTitleScreenLoad::Update      @0x8247E2B8  `lbz r11, 0x4B78(r11)` @0x8247E408
        //                                                 -- set == play "intro", clear == post
        //                                                 phase-complete (70) immediately.
        // No shift: one byte carved from a five-byte pad, four bytes of pad left behind.
        bool mbPlayIntroVideo;                           // +0x4B78 (19320)
        u8   mPad_4B79[0x4B7C - 0x4B79];                 // +0x4B79..+0x4B7B
        // ADDITIVE CARVE (A9 mode-type arm, 2026-08-27) from the head of the former
        // mPad_4B77 -- an 8-word run, zeroed at both of its recovered writers:
        //   GuiCache::Construct @0x82505DD8..0x82505DF4 walks `r11 = this + 0x4B7C`, storing
        //   zero to `0(r11)` AND to `0x5594(r11)` (== the twin run at +0xA110) eight times,
        //   `addi r11, r11, 4` per step -- so both runs are 8 x s32.
        //   GuiCache::RecEvent case 93 @0x8250E9B8..0x8250E9C8 re-zeroes this one alone
        //   (`addi r11, r31, 0x4B7C`, `mtctr 8`, `stw ; addi r11,r11,4 ; bdnz`).
        // FLAG: NAME INFERRED. There is no recovered READER anywhere in the exports, no DWARF
        // row, and no accessor; what is attested is the base, the stride, the count and the
        // reset. Named as a per-active-race-car lane run because it is 8 words wide and is
        // reset on mode start, the shape every other 8-lane table on this class has.
        // No member is shifted (5 + 32 + 1024 == 0x4F9C - 0x4B77).
        s32  maPerRaceCarWord_4B7C[8];                   // +0x4B7C (19324) FLAG: name inferred, no reader recovered
        u8   mPad_4B9C[0x4F9C - 0x4B9C];                 // +0x4B9C..+0x4F9B
        // ADDITIVE CARVE (E1 event-status wave 2026-08-26) from the TAIL of the former
        // mPad_4B77 -- the count of remaining checkpoints the id-492 GuiEventCurrentStatus
        // record carries. X360 GuiCache::RecEvent case 112 @0x82510540/0x82510558:
        // `lwz r11, 0x34(r30) ; ... ; stw r11, 0x4F9C(r31)`, then the SAME word is reloaded
        // as the bound of the landmark-tracker fill loop (@0x82510590) and passed as the
        // third argument to GuiCache::UpdateTrackerInfo (@0x825105F8). The store is
        // UNCONDITIONAL; the loop and the UpdateTrackerInfo call that read it back are gated
        // on meGameModeType == E_MODE_ONLINE_BURNING_HOME_RUN (13) and are FLAG-deferred in
        // BrnGuiCache.cpp -- see the case-492 banner there.
        // No member is shifted (the pad simply loses its last 4 bytes).
        s32  miNumRemainingCheckpoints;                  // +0x4F9C (20380)
        // ADDITIVE CARVE (HUD H1 wave, 2026-08-25): the district-marker source words -- the
        // latest GUI-event-169 (GuiEventChangeDistrict) record, stored verbatim. Pinned as a
        // TRIO at three ends: GuiCache::Construct @0x82505860 seeds
        // `district := 18 (E_DISTRICT_INVALID); county := WorldRegion::DistrictToCounty(18);
        // consumed byte := 0`; GuiCache::RecEvent @0x8250DDF0 case 169 copies the record's
        // three words here; and FBurnMainHudState::UpdateRunning @0x8247B660's post-loop
        // reads all three, drives DistrictMarkerComponent::SetCounty/SetDistrict when the
        // consumed byte is clear (or the state's own refresh is armed and the district is
        // valid), then hands the record BACK through RecEvent(169) with the consumed byte
        // set. The console flag word's tested byte is its FIRST byte (BE `lbz` at +20392);
        // modelled as a leading u8 so the LE host tests the same authored byte.
        s32  meChangeDistrictCounty;                     // +0x4FA0 (20384) BrnWorld::ECounty
        s32  meChangeDistrictDistrict;                   // +0x4FA4 (20388) BrnWorld::EDistrict
        u8   mu8ChangeDistrictConsumed;                  // +0x4FA8 (20392) 0 == fresh, 1 == consumed
        u8   maPad_4FA9[7];                              // +0x4FA9..+0x4FAF
        u8   maPresetRacesStorage[6 * 120];              // +0x4FB0 (20400) PresetRace maPresetRaces[6] (stride 120; GetPresetRace @0x824B2FE8 -> 120*(idx+170)+this; element un-homed)
        s32 miNumPresetRaces;                            // +0x5280 (21120) count of maPresetRaces (GetPresetRace bound)
        u8  mPad_5284[2];                                // +0x5284..+0x5285
        // [gateui r3] ADDITIVE CARVE from the head of the former mPad_5284[9436] -- the count of
        // ACTIVE landmarks. Producer GuiCache::HandleSetActiveLandmarksEvent @0x824EE7D0 copies
        // the incoming list into the u16 array at +0x5288 (`addi r10, r30, 0x5288`, `sth`) and
        // stores the count with a BYTE store (`stb r11, 0x5286(r30)`) after asserting the source
        // list is <= 0x200 (512); GuiCache::Construct zeroes it. KEEP THIS `u8` -- the console
        // truncates a >255 list and that is the shipped behaviour, not a bug to widen away.
        // Consumer: HudMessageAnalyzer::HandleRaceCheckpointReached @0x8251B350.
        // NOT the same counter as muCheckpointsInEvent (+0x9FB8, GetCheckpointsInEvent
        // @0x8240F1C0) -- two different "checkpoint" counts on this class.
        // No member is shifted (2 + 1 + 9433 == 9436); the u16 array at +0x5288 stays padded.
        u8  muNumActiveLandmarks;                        // +0x5286 (21126)
        u8  mPad_5287[1];                                // +0x5287
        // [map arm 2026-08-27] ADDITIVE CARVE from the former mPad_5287[0x409]: the ACTIVE
        // landmark-index array HandleSetActiveLandmarksEvent @0x824EE7D0 fills (`addi r10,
        // r30, 0x5288` + the `sth` copy loop, bounded by the KI_MAX_LANDMARKS_IN_GAME == 512
        // assert). 512 * 2 == 0x400; the pad keeps its first byte and its 8-byte tail, so no
        // member shifts (1 + 0x400 + 8 == 0x409).
        u16 maActiveLandmarks[512];                      // +0x5288..+0x5687 (KI_MAX_LANDMARKS_IN_GAME)
        u8  mPad_5688[8];                                // +0x5688..+0x568F
        // [H3b ADDITIVE CARVE] the embedded mSetUpAllEventStartsInterface's event-start
        // array (X360 @0x5690, 175 x 0x30 == 0x20D0 bytes -- ends EXACTLY at the count
        // word miEventStartsCount @0x7760 the accessors read at interface+0x20D0).
        SatNavEventDisplayInfo maEventStarts[175];       // +0x5690..+0x775F
        s32 miEventStartsCount;                          // +0x7760 (30560) mSetUpAllEventStartsInterface CgsArray count (ctor -1; GetNumEventStarts sentinel/count @+0x20D0 of the interface)
        u8  mPad_7764[12];                               // +0x7764..+0x776F
        // 256-bit online finish-point bitmask (4 doublewords). GetNumOnlineFinishPoints
        // @0x8241E7D8 loads each 64-bit word (ld @0x7770/0x7778/0x7780/0x7788), 64-bit
        // SWAR-popcounts it, and sums the four counts. Carved from the former mPad_7764.
        u64 maOnlineFinishPointsMask[4];                 // +0x7770 (30576)
        // [H3b ADDITIVE CARVE] the drive-through / junkyard sat-nav icon list + its count
        // (X360 GetDriveThroughOrJunkyardAtIndex @0x824FAC10: entries at cache+0x7790,
        // stride 0x30, count word at cache+0x8030 -- 46*0x30 + 4 == the old 2212-byte pad
        // EXACTLY; DWARF rows mDriveThroughInfo[46] / the count).
        GuiEventUpdateSatNav::SatNavIconInfo maDriveThroughInfo[46]; // +0x7790..+0x802F
        s32 miNumDriveThroughs;                          // +0x8030 (32816)
        // ADDITIVE CARVE (H3b): the sat-nav event filter pair the freeburn HUD's
        // UpdateSetupState reads (X360 @0x82480EA0: lwz cache+0x8034 / lbz cache+0x8038
        // -> FBurn +992/+996 and the Enable/DisableSatNavEventsFilter pick). DWARF
        // BrnGuiCache.h:455/:458 names them; the run ends exactly at miSatNavZoomLevel.
        s32 meSatNavEventFilter;                         // +0x8034 (32820) BrnProgression::RaceEventData::EModeType (spelled s32 -- the BrnGameEvents.h precedent)
        bool mbSatNavEventFilterEnabled;                 // +0x8038 (32824)
        u8  mPad_8039[3];                                // +0x8039..+0x803B
        s32 miSatNavZoomLevel;                           // +0x803C (32828) ZoomSatNavOut @0x82472FD0 (result[8207]); clamp [0,1], assert <= E_SAT_NAV_ZOOM_COUNT
        u8  maEventsStorage[7700];                       // +0x8040 (32832) mEvents preset-event CgsArray storage (GetPresetEvent @0x8241E520 forwards 32832; count is mEventsCtorSentinel @+7700)
        s32 mEventsCtorSentinel;                         // +0x9E54 (40532) mEvents array ctor marker / GetNumPresetEvents count (-1 = not constructed)
        s32 meGameModeType;                              // +0x9E58 (40536) GsmIO::EGameModeType
        // ADDITIVE CARVE (wave J): the two ids the DWARF member run places IMMEDIATELY after
        // meGameModeType (DWARF BrnGuiCache.h:467 muEventID, :470 muJunctionID -- the order
        // is what pins them here), X360-attested as the far words read by
        // PreRaceFlyByState::UpdateIconManager (`lwzx r4, cache, 0x9E5C` @0x824C7BDC /
        // `lwzx r11, cache, 0x9E60` @0x824C7C8C). No member is shifted (4 + 4 + 116 == 124).
        u32 muEventID;                                   // +0x9E5C (40540) GetEventID
        u32 muJunctionID;                                // +0x9E60 (40544) GetJunctionID
        u8  mPad_9E64[116];                              // +0x9E64..+0x9ED7
        s32 miCtorSentinel_9ED8;                         // +0x9ED8 (40664) ctor writes -1 (CgsArray sentinel; sub-array un-homed)
        u8  mPad_9EDC[36];                               // +0x9EDC..+0x9EFF
        s32 miCtorSentinel_9F00;                         // +0x9F00 (40704) ctor writes -1 (CgsArray sentinel; sub-array un-homed)
        u8  mPad_9F04[36];                               // +0x9F04..+0x9F27
        // ADDITIVE CARVE (E1 event-status wave 2026-08-26) from the TAIL of the former
        // mPad_9F04[40] -- the medal-target word that leads the event medal/time block.
        // X360-attested by GuiCache::RecEvent case 44 (GUI event 424, GuiEventScoreUpdate):
        // `lwz r11, 0(r30) ; ori r10, 0x9F28 ; stwx r11, r31, r10` @0x825107F8..0x82510800,
        // where the payload's +0x00 word is ScoringOutputInterface::meCurrentMedalTarget
        // (the producer's `lwz r9, 0xA9C(r27)` @0x823EEC5C). NAME is producer-derived; the
        // value home is BrnGameState::ECurrentMedalTargetTime, spelled s32 here per this
        // boundary header's enum convention (same as meSatNavEventFilter/meCarSelectType).
        // No member is shifted (36 + 4 == 40); the pair it leads (mfEventTime/mfTargetTime)
        // is written by the SAME RecEvent arm, gated on the payload's mbTimerActive byte.
        s32 meCurrentMedalTarget;                        // +0x9F28 (40744)
        f32 mfEventTime;                                 // +0x9F2C (40748)
        f32 mfTargetTime;                                // +0x9F30 (40752)
        // ADDITIVE CARVE (wave J): the per-medal score targets the pad comment already
        // labelled. DWARF BrnGuiCache.h:485 `float32_t[4] mafTargetScores`, sitting right
        // after mfTargetTime in the DWARF member run -- which is exactly this slot.
        // X360-attested FLOAT: PreRaceFlyByState::SetRoadRageDescription @0x824C70C0 and
        // ::SetFreestyleDescription @0x824C727C do `ori r9, 0x9F34` + `lfsx` + `fctiwz`.
        f32 mafTargetScores[4];                          // +0x9F34 (40756) GetRequiredScoreForMedal
        s8  miOpponentsInEvent;                          // +0x9F44 (40772)
        u8  mPad_9F45[3];                                // +0x9F45..+0x9F47
        f32 mfDistanceInEvent;                           // +0x9F48 (40776) GetDistanceInEvent @0x8240F398 (result[10194], >= 0)
        u16 mEventDestinationLandmarkIndex;              // +0x9F4C (40780) LandmarkIndex (s16)
        u8  mPad_9F4E[2];                                // +0x9F4E..+0x9F4F
        // ⛔ THE UNION IS RETIRED (A9 mode-type arm, 2026-08-27). It used to read
        //       union { s32 mEventDestinationDistrict;                      // +0x9F50
        //               struct { u8 mPad_9F50_pre[2]; u16 maCheckpointLandmarks[49]; } };  // +0x9F52
        // on the premise that GetEventFinishLandmark @0x824EC610's `2*(count + 0x4FA9)` load
        // pinned the landmark array's BASE at +0x9F52 -- which forced it to overlap the
        // district word and made its 49-entry bound region-derived guesswork.
        // REFUTED, and by the only writer of the region: GuiCache::RecEvent's case-93 arm
        // (@0x8250EA18..0x8250EA60) copies GuiEventPrepareForModeStart's two checkpoint
        // tables into TWO SEPARATE arrays with clean bases and clean bounds --
        //     `addi r9, r31, -0x60AC` == cache+0x9F54, `sth` per entry   (u16 landmark)
        //     `addi r7, r31, -0x608C` == cache+0x9F74, `stw` per entry   (s32 district)
        // and the tail-fill loop right after it (@0x8250EA68..) fills the REMAINDER of both up
        // to 16 entries (`4*(i + 10205) + this` == 0x9F74 + 4i, seeded 18 == E_DISTRICT_INVALID;
        // `2*(i + 20394) + this` == 0x9F54 + 2i, seeded 0) with the console's own
        // KI_MAX_LANDMARKS_IN_MODE bound of 16 (its assert literal, BrnGuiCache.cpp:2094,
        // `cmplwi 0x10`). 4 + 32 + 64 == 100 == the exact span to miCheckpointReached, with no
        // overlap and no slack -- the run is PINNED, not derived.
        // GetEventFinishLandmark: `+0x9F52 + 2*count` IS `maCheckpointLandmarks[count - 1]`,
        // i.e. the LAST checkpoint's landmark, which is what "finish landmark" means. Its body
        // in BrnGuiCache_wB_res.cpp indexes [count - 1] accordingly (fixed 2026-08-27 -- the
        // pre-rebase body indexed [count], one u16 past the console's slot).
        s32 mEventDestinationDistrict;                   // +0x9F50 (40784) BrnWorld::EDistrict
        u16 maCheckpointLandmarks[16];                   // +0x9F54 (40788) per-checkpoint LandmarkIndex (KI_MAX_LANDMARKS_IN_MODE)
        s32 maCheckpointDistricts[16];                   // +0x9F74 (40820) per-checkpoint BrnWorld::EDistrict
        s32 miCheckpointReached;                         // +0x9FB4 (40884)
        // ⚠️ WIDTH CORRECTED (A9, 2026-08-27): this is a **u8**, not a u32. Two independent
        // pins, both single-byte: GuiCache::GetCheckpointsInEvent @0x8240F1C0 reads it with
        // `lbzx r3, r31, r30` (r30 == 0x9FB8) and its caller GetEventFinishLandmark masks the
        // result with `clrlwi r30, r3, 24`; and RecEvent's case-93 arm WRITES it with
        // `lbz r11, 0x8C(r30) ; stbx r11, r31, r21` (r21 == 0x9FB8). The console's own assert
        // literal spells the type: "lu8NumCheckpointsInEvent > 0" (BrnGuiCache.h:4108).
        // As a u32 on the little-endian host a byte store would land in the LOW byte while the
        // console's lands in the HIGH one -- the offsets/strides class of bug this campaign
        // keeps re-catching. Name kept (readers spell it muCheckpointsInEvent); 3 bytes of
        // explicit pad keep miTakedownsCurrent at +0x9FBC. No member is shifted.
        u8  muCheckpointsInEvent;                        // +0x9FB8 (40888)
        u8  mPad_9FB9[3];                                // +0x9FB9..+0x9FBB
        s32 miTakedownsCurrent;                          // +0x9FBC (40892)
        s32 miTakedownTarget;                            // +0x9FC0 (40896)
        s32 miScoreCurrent;                              // +0x9FC4 (40900)
        s32 miScoreTarget;                               // +0x9FC8 (40904)
        s32 miScoreCombo;                                // +0x9FCC (40908)
        s32 miComboMultiplier;                           // +0x9FD0 (40912)
        // ADDITIVE CARVE (HudMessageAnalyzer wave-C keystone): the last stunt's score.
        // NAME is DWARF-attested by adjacency (BrnGuiCache.h:1762 miLastStuntScore is the
        // member immediately after h:1760 miComboMultiplier); the OFFSET is X360-attested
        // by HudMessageAnalyzer::HandleWreckedEvent @0x8251CB1C (lwzx cache+0x9FD4 > 0,
        // together with meGameModeType == E_MODE_STUNT_ATTACK, picks the WRECKED_STUNT
        // lane). The PS3-DWARF tail after it (miLastStuntMultiplier / miRivalDamage*)
        // does NOT fit the 12-byte gap to mPursuedCarID @0x9FE0, so only this first
        // member is carved; the rest stays padding.
        s32 miLastStuntScore;                            // +0x9FD4 (40916)
        // ADDITIVE CARVE (RecEvent case 132, @0x8250F5C8/CC): the -1 reset pair -- the
        // flow-state change stores -1 into BOTH +0x9FD4 (miLastStuntScore above) and this
        // word (stwx r29==-1 via the 0x9FD4/0x9FD8 ori pair). FLAG: producer-named -- the
        // consumer side is unrecovered (plausibly the DWARF miLastStuntMultiplier, but the
        // 12-byte-gap note above keeps that name uncommitted).
        s32 miGameFlowResetWord_9FD8;                    // +0x9FD8 (40920)
        u8  mPad_9FDC[4];                                // +0x9FDC..+0x9FDF
        CgsID mPursuedCarID;                             // +0x9FE0 (40928)
        // ADDITIVE CARVE (A9 mode-type arm, 2026-08-27) from mPad_9FE8[8]. Both words are
        // written by GuiCache::Construct @0x82505CF8/0x82505CFC (`stwx r29`, r29 == -1) and
        // again by RecEvent's case-93 arm: `stwx r29, r31, r24` with r24 == 0x9FE8 (-1) and
        // `lwz r11, 0x88(r30) ; stwx r11, r31, r23` with r23 == 0x9FEC (the wire's
        // miPursuitRivalTotalDamage, GameModeParams+0x50). The +0x9FEC name is the producer's;
        // FLAG: +0x9FE8 has no recovered reader at all -- producer-named after its neighbour
        // (the pursuit pair the DWARF run puts here), reset-to--1 semantics only.
        // No member is shifted (4 + 4 == 8).
        s32 miPursuitRivalDamageLeft_9FE8;               // +0x9FE8 (40936) FLAG: name inferred; only writers recovered
        s32 miPursuitRivalTotalDamage;                   // +0x9FEC (40940)
        CgsID mShutdownCarID;                            // +0x9FF0 (40944)
        u8  mPad_9FF8[8];                                // +0x9FF8..+0x9FFF
        s32 meTrophyCarUnlockType;                       // +0xA000 (40960) TrophyUnlockData::UnlockType
        // ---- mRaceCarInfo SoA (ARCI-indexed, 8 lanes) carved from the former mPad_A004[0x9C4] ----
        u8  mPad_A004[16];                               // +0xA004..+0xA013
        // ADDITIVE CARVE (A9 mode-type arm, 2026-08-27) from the tail of the former
        // mPad_A004[17]. A single byte, and every recovered access is a byte access:
        //   WRITTEN 0 by GuiCache::Construct @0x82505D6C (`stbx r30, r31, r9`, r9 == 0xA014)
        //   WRITTEN 1 by GuiCache::RecEvent case 93 @0x8250E9B4 (`stbx r20, r31, r11`, r20 == 1)
        //   READ     by RaceMainHudState::UpdateWFInit @0x824803C0 (gates the event-info /
        //            countdown branch), ::UpdateSetupState @0x8247A260 (gates the
        //            GetEventDestinationDistrict lookup), ::UpdateRunning @0x8247F124,
        //            InGame::PauseAllowed @0x824B8E44 (set == pause NOT allowed) and
        //            InGame::PauseGame @0x824DF05C (set == suppress the "ON_PAUSE" state event).
        // FLAG: NAME INFERRED. The DWARF has no row for this byte and the console has no
        // accessor for it; "an event has been prepared for / is starting" is what the one
        // writer and the five readers agree on, and it is what the case-93 arm means.
        // No member is shifted (16 + 1 == 17).
        bool mbEventPreparedForModeStart;                // +0xA014 (40980) FLAG: name inferred
        // ADDITIVE CARVE (BrnPreRaceFlyBy wave J): the fly-by-active gate byte. NAME from
        // DWARF (BrnGuiCache.h:569 mbIsPreRaceFlyByActive, with the DWARF accessor pair
        // IsPreRaceFlyByActive/SetPreRaceFlyByActive); OFFSET X360-attested by the two
        // inlined `stbx r, cache, 0xA015` stores -- PreRaceFlyByState::OnEnter @0x824C6718
        // (r24 == 1) and ::OnLeave @0x824C6BC4 (r29 == 0). Those two are the ONLY recovered
        // writers and no recovered reader exists yet. No member is shifted (17 + 1 + 10 == 28).
        bool mbIsPreRaceFlyByActive;                     // +0xA015 (40981)
        u8  mPad_A016[10];                               // +0xA016..+0xA01F
        Vector4 maRaceCarPositions[8];                   // +0xA020 (40992) GetRaceCarPosition @0x82443750 (16*(idx+2562)+this)
        // [hud H3b tracking slice 2026-08-25] the middle of the SoA block carved from
        // the former mPad_A0A0[68]: the identity qwords + entry count the case-207 copy
        // lands (`memcpy(cache+0xA020, GuiRaceCarInfoEvent, 240)` on the console --
        // record maIdentity @0x80 -> +0xA0A0, miNumEntries @0xC0 -> +0xA0E0). No member
        // is shifted (64 + 4 == 68).
        u64  maRaceCarIdentities[8];                     // +0xA0A0 (41120) case-207 identity qwords
        s32  miNumRaceCarsInInfo;                        // +0xA0E0 (41184) case-207 entry count
        bool maRaceCarUsed[8];                           // +0xA0E4 (41188) IsActiveRaceCarIndexUsed @0x82443A00 / GetRaceCarPosition used-gate
        bool maRaceCarConnecting[8];                     // +0xA0EC (41196) IsActiveRaceCarConnecting @0x82443B28
        // [gateui r3] ADDITIVE CARVE from the head of the former mPad_A0F4[16] -- the
        // per-active-race-car "disconnected from the network" flags, the third table in the
        // maRaceCarUsed / maRaceCarConnecting run above. X360-attested by
        // GuiCache::IsActiveRaceCarDisconnected @0x82443C50, whose whole body is the two range
        // asserts plus `lbzx r3, index, 0xA0F4`. HudMessageAnalyzer::HandleEventFinisher
        // @0x824F2FB0 also walks all eight bytes (+0xA0F4..+0xA0FB) for its debug dump.
        // No member is shifted (8 + 8 == 16).
        bool maRaceCarDisconnected[8];                   // +0xA0F4 (41204) IsActiveRaceCarDisconnected @0x82443C50
        // [hud H3b] case-207 flag-D lane (GuiRaceCarInfoEvent maFlagD @0xDC == in-range).
        bool maRaceCarInRange[8];                        // +0xA0FC (41212) case-207 in-range bytes
        bool maRaceCarCrashing[8];                       // +0xA104 (41220) IsRaceCarCrashing @0x824438A8
        u8  mPad_A10C[36];                               // +0xA10C..+0xA12F
        s8  maEventPositionOfRaceCar[8];                 // +0xA130 (41264) GetEventPositionOfRaceCar @0x82443D78 (place; gated on @0xA140)
        bool maRaceCarFinished[8];                       // +0xA138 (41272) HasRaceCarFinished @0x824EC4C8 (gated on @0xA140)
        bool maEventPositionValid[8];                    // +0xA140 (41280) validity gate for maEventPositionOfRaceCar / maRaceCarFinished
        u8  mPad_A148[8];                                // +0xA148..+0xA14F
        // ---- scoring-traffic CgsArray (mTrafficCarInfo.mScoreTargets) ----
        u8  maScoringTrafficDataStorage[640];            // +0xA150 (41296) BrnTraffic::ScoringTrafficData[] (GetScoringTrafficData @0x82450718 forwards 41296; element sizeof un-attested)
        s32 miScoringTrafficCount;                       // +0xA3D0 (41936) GetScoringTrafficCount @0x824497C0 (CgsArray count; ctor -1)
        u8  mPad_A3D4[1036];                             // +0xA3D4..+0xA7DF
        s32 miCtorSentinel_A7E0;                         // +0xA7E0 (42976) ctor writes -1 (CgsArray sentinel; sub-array un-homed)
        u8  mPad_A7E4[24];                               // +0xA7E4..+0xA7FB
        // ---- online game-mode-options (round-indexed) ----
        s32 miOnlineRoundIndex;                          // +0xA7FC (43004) GetOnlineRoundIndex (GetOnlineLandmarkIndex @0x8240FB50), < KU_MAX_ONLINE_ROUNDS_IN_MODE
        // The +0xA800 span is the cached GuiEventNetworkGameParams PAYLOAD MIRROR:
        // OnlineGameRoomPlayerInfo::HandleGameParamsChangedEvent @0x824A52F8 memcpy's
        // the whole 480-byte params payload to cache+0xA800, so +0xA800..+0xA9DF has
        // exactly the GuiEventNetworkGameParams field layout (maEvents[10] stride 44,
        // then the scalar tail -- see GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h).
        u8  maOnlineGameModeOptionsStorage[10 * 44];     // +0xA800 (43008) GetOnlineGameModeOptions[] (stride 44, 10 rounds; GetOnlineLandmarkIndex indexes 44*round+43008)
        // ADDITIVE CARVE (OnlineGameRoomPlayerInfo keystone, wave H): the params-mirror
        // scalar tail, named per the GuiEventNetworkGameParams DWARF field row.
        s32 meOnlineGameMode;                            // +0xA9B8 (43448) GsmIO::EGameModeType (params.meGameMode; 15/16 == IsOnlineFreeBurnLobby)
        s32 meOnlinePreviousGameMode;                    // +0xA9BC (43452) params.mePreviousGameMode
        s32 meOnlineSecurity;                            // +0xA9C0 (43456) BrnNetwork::EBrnGameSecurity (params.meSecurity; 0 public / 1 private / 2 closed)
        s32 meOnlineBoostType;                           // +0xA9C4 (43460) params.meBoostType
        // ADDITIVE GROW (BrnCarSelectOnlineEnd TU): the online-host-game state word (== 1 when the
        // local client is the online host). FLAG: consumer-named -- structurally this word is the
        // params mirror's meVehicleChoice slot (see the carve above); MERGE RECONCILE PENDING with
        // that TU when it is next touched (same discipline as the +0x4B4F dual claim).
        s32 miOnlineHostGameState;                       // +0xA9C8 (43464) (params.meVehicleChoice slot)
        s32 miOnlineTimeLimit;                           // +0xA9CC (43468) params.miTimeLimit
        s32 miOnlineNumRounds;                           // +0xA9D0 (43472) params.miNumRounds (the game-room route/round scroll bound)
        s32 miOnlineVehicleClass;                        // +0xA9D4 (43476) params.miVehicleClass
        s32 miOnlineNumRunnerCrashes;                    // +0xA9D8 (43480) params.miNumRunnerCrashes
        bool mbOnlineInfiniteBoost;                      // +0xA9DC (43484) params.mbInfiniteBoost
        bool mbOnlineTrafficOn;                          // +0xA9DD (43485) params.mbTrafficOn
        bool mbOnlineTrafficCheckingOn;                  // +0xA9DE (43486) params.mbTrafficCheckingOn
        bool mbOnlineRanked;                             // +0xA9DF (43487) params.mbRanked (lbzx @0x8248C184; gates the security pause option + world-rank display)
        // ADDITIVE CARVE (OnlineGameOptions wave I): the byte right after the params
        // mirror. OnlineGameOptions::HandleGuiCacheEvent (@0x824A85E8, lbzx cache+0xA9E0)
        // tests it (OR meOnlineGameMode == 15) to publish the id-409 refresh record on
        // first entry, then clears it. FLAG: consumer-named -- the producer side is not
        // yet reconstructed.
        bool mbOnlineGameOptionsChanged;                 // +0xA9E0 (43488)
        u8  mPad_A9E1[79];                               // +0xA9E1..+0xAA2F
        // ctor field-inits a stride-56 SoA of 8 lanes (one per ARCI): int@+0, float@+4 each
        // (ctor @0x827E05B8 writes +43568..+43964). FLAG: only the +0/+4 words are attested
        // (the 48-byte tail is reserved); semantic of the pair is unrecovered.
        struct PerRacerPair_AA30 { s32 miField_00; f32 mfField_04; u8 mPad_08[48]; }; // 56 bytes
        PerRacerPair_AA30 maPerRacerData_AA30[8];        // +0xAA30 (43568) ctor-initialised per-racer numeric pairs
        u8  mPad_AC10[72];                               // +0xAC10..+0xAC37
        bool mabRoadRulesActive[2];                      // +0xAC38 (DWARF h; precedes meActiveRoadRule)
        u8  mPad_AC3A[2];                                // +0xAC3A..+0xAC3B
        s32 meActiveRoadRule;                            // +0xAC3C (44092) BrnGameState::EActiveRoadRule (PlayerPositionSingle::RenderValue gate @0x824220B4)
        s32 meRoadRuleScoreMode;                         // +0xAC40 (44096) GuiEventSetRoadRuleScoreMode::ERoadPanelModes
        bool maRoadRuleActiveByType[2];                  // +0xAC44 (44100) IsRoadRuleActive @0x82472E78 (idx 0..1, score type)
        u8  mPad_AC46[2];                                // +0xAC46..+0xAC47
        // ---- road-rule-shot block (RoadRuleShotComponent::Snap @0x82415620) ----
        s32 meRoadRuleShotOpponentARCI;                  // +0xAC48 (44104) GetRoadRuleShotOpponentARCI (DWARF h:1817)
        u8  mPad_AC4C[14];                               // +0xAC4C..+0xAC59
        bool mbRoadRuleShotCapturedLineGate;             // +0xAC5A (44122) GetRoadRuleShotCapturedLineGate ("CAPTURED_FOR" line gate)
        u8  mPad_AC5B[1];                                // +0xAC5B
        // ⚠️ CORRECTED BOUND (E1 event-status wave 2026-08-26). This member used to read
        // `maStuntToDisplay[3]`, an unattested space-filler chosen to reach +0xAC74. The
        // console DISAGREES about everything past the first entry: GuiCache::RecEvent
        // case 48 (GUI event 428, GuiAttackScoreUpdate) @0x825109C0..0x825109F8 writes
        // +0xAC5C/+0xAC60 as the stunt pair, but +0xAC64/+0xAC68 as two more score WORDS,
        // +0xAC6C with an `lfs`/`stfsx` FLOAT and +0xAC70/+0xAC71 with two `lbz`/`stbx`
        // BYTES -- which no stride-8 {s32,s32} array can be. GetStuntToDisplay
        // @0x8240F770's own terminator walk is bounded at ONE iteration (`cmpwi r11, 1 ;
        // blt`), so a single entry is what the accessor can ever hand out. The five members
        // below are the rest of the id-428 record, named after the producer's own
        // ScoringOutputInterface fields (:563-:568) whose values they receive. Total is
        // unchanged (8 + 4 + 4 + 4 + 1 + 1 + 2 == 24, +0xAC5C..+0xAC74).
        StuntToDisplayInfo maStuntToDisplay[1];          // +0xAC5C (44124) GetStuntToDisplay @0x8240F770 (-1-terminated; miStuntId receives maStunts[0].meStuntType, miField_04 the score)
        u32 muCurrentStunts;                             // +0xAC64 (44132) RecEvent 428 (stwx, payload +0x10)
        u32 muAllStunts;                                 // +0xAC68 (44136) RecEvent 428 (stwx, payload +0x14)
        f32 mfComboWarningTimeActive;                    // +0xAC6C (44140) RecEvent 428 (stfsx, payload +0x18)
        bool mbComboWarningActive;                       // +0xAC70 (44144) RecEvent 428 (stbx, payload +0x24)
        bool mbComboInProgress;                          // +0xAC71 (44145) RecEvent 428 (stbx, payload +0x25)
        u8  mPad_AC72[2];                                // +0xAC72..+0xAC73
        u32 muNumActivePlayers;                          // +0xAC74 (44148) GetFriendsListCachedField / GetNumActivePlayers
        // ADDITIVE CARVE (friends wave): slot id BuildChallengeList matches each
        // freeburn challenge player-nibble against (read @0x8242B8AC, cache+0xAC78).
        // Consumer-named; no DWARF accessor row.
        u32 muChallengeSlotMirror;                       // +0xAC78 (44152)
        u8  mPad_AC7C[4];                                // +0xAC7C..+0xAC7F
        // One online-player record per player. Byte storage for the fwd-declared 312-byte
        // BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData (GetOnlinePlayerInfo @0x8240F890
        // -> 312*idx+44160). ctor field-inits each lane's +0x60(int)/+0x64(float).
        u8  maPlayerInfo[8][312];                        // +0xAC80 (44160) InGamePlayerStatusData maPlayerInfo[8] (stride 312)
        // ADDITIVE CARVE (OnlineGameRoomPlayerInfo keystone, wave H): the cached lobby
        // player-status mirror (the event-244 payload copied in). Byte storage for the
        // 56-byte BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData stride (home:
        // GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h;
        // kept opaque here for the same include-clash reason as maPlayerInfo above).
        // Row fields the game-room screen reads: mPlayerID @row+16, meReadyStatus
        // @row+20, mePlayerTeam @row+24, meGameConnectionType @row+28,
        // meVoipConnectionType @row+32, mbLocalPlayer @row+48.
        u8  maLobbyPlayerInfo[8][56];                    // +0xB640 (46656) LobbyPlayerStatusData maLobbyPlayerInfo[8] (stride 56)
        u8  mPad_B800[8];                                // +0xB800..+0xB807 (unclaimed; the live count the screen loops with is muNumActivePlayers @+0xAC74)
        s32 maCurrentPlayerTeam[8];                      // +0xB808 (47112) GetCurrentOnlinePlayerTeam @0x8240F910 (4*(idx+11778)+this; GsmIO::EPlayerTeam)
        u8  mPad_B828[36];                               // +0xB828..+0xB84B
        bool maOnlinePlayerDisconnected[8];              // +0xB84C (47180) GetOnlinePlayerDisconnected @0x8240F988
        bool maOnlinePlayerInCarSelect[8];               // +0xB854 (47188) GetOnlinePlayerInCarSelect @0x824436D0
        bool maOnlinePlayerEliminated[8];                // +0xB85C (47196) IsOnlinePlayerEliminated @0x8240FA08
        // Far tail. The player-options profile block (GetOptionsDataProfile @0xB878/47224) and
        // the sat-nav icon / drive-through / landmark tables live in this span but are reached
        // only through declaration-only accessors; not individually modelled. See notes.
        // +0xB878 (47224) -- the embedded player-options profile block (GetOptionsDataProfile
        // hands it out; ScreenLoading writes the loaded save into it). BrnGuiOptionsDataProfile.h
        // carries compile-only slices of network/game-state types that clash with the REAL
        // headers inside this cache header's wide include set, so the block is reserved OPAQUE
        // here and typed only inside BrnGuiCache.cpp (which static_asserts the real
        // OptionsDataProfile fits this reservation). Widens on x64; access by name. The 20-byte
        // lead-in (+0xB864..+0xB877) holds un-modelled sat-nav/landmark words.
        // ADDITIVE CARVE (OnlineGameRoomPlayerInfo keystone, wave H): the "local player
        // is the online host" byte (lbzx cache+0xB864 across the screen: host leave
        // question / host pause options / kick option / change-options entry). FLAG:
        // consumer-named (no standalone DWARF for this byte).
        bool mbIsOnlineHost;                             // +0xB864 (47204)
        // ADDITIVE CARVE (friends wave 2026-08-26): the EasyDrive friends panel's
        // branch-state mirror. The X360 inlines dword reads/writes here from
        // ShowFriendsListBranch (@0x82422E34 switch), HandleDPadRightFriends
        // (@0x824386F0 test / @0x82438728 store 4), HandleBranchDPadRightFriends
        // (stores 1/2/3), EndWait (@0x82443018 clear) -- no DWARF accessor row;
        // consumer-named. Exposed by friendship below.
        u32 muFriendsPanelBranchMirror;                  // +0xB868 (47208)
        // ADDITIVE CARVE ([stuntrace wS2] wave, 2026-08-27) from the HEAD of the former
        // mPad_B86C[12] -- the two friends-list gate bytes the RACE_MAIN HUD state reads.
        // Both are `lbzx` through a materialised offset (the far-member idiom):
        //   +0xB86C  RaceMainHudState::OnLeave     @0x824797FC `ori r10,r10,0xB86C ; lbzx`
        //            -> the ONLY gate on FriendsListComponent::Close (@0x82479810), i.e.
        //            "the friends panel is currently open, tear it down on the way out".
        //   +0xB86D  RaceMainHudState::UpdateWFInit @0x824805B8 `ori r10,r10,0xB86D ; lbzx ;
        //            cmplwi cr6, r11, 1` -> FriendsListChangeIconComponent::ShowNow
        //            (@0x824805CC), i.e. "a friends-list change arrived while the HUD was
        //            down -- pop the change icon straight away, no animate-in".
        // Note the mirrored neighbour muFriendsPanelBranchMirror at +0xB868 above: this whole
        // cluster is the cache's friends-panel mirror, which is what makes the carve safe.
        // No member is shifted (1 + 1 + 10 == 12). FLAG: consumer-named -- neither producer
        // is recovered, so both read 0 on this build (no panel, no pending change).
        bool mbFriendsListOpen;                          // +0xB86C (47212)
        bool mbFriendsListChangePending;                 // +0xB86D (47213)
        u8 mPad_B86E[6];                                 // +0xB86E..+0xB873
        // ADDITIVE CARVE (pause wave, 2026-08-28) from the TAIL of the former
        // mPad_B86E[10] -- the licence card's "points to the next rank" counter, read as a
        // HALFWORD (`ori r10,r10,0xB874 ; lhzx r27, r11, r10`, the far-member idiom) by
        // CrashNavDriverDetails::UpdateSetupLicense @0x824C1D20 and handed straight to
        // LicenseComponent::SetPlayerInfo's liPointsToNextRank slot. u16 because the
        // console loads it with lhzx, not lwz. No member is shifted (6 + 2 + 2 == 10).
        // FLAG: consumer-named -- the producer is not recovered, so it reads 0 here.
        u16 mu16LicencePointsToNextRank;                 // +0xB874 (47220)
        u8 mPad_B876[2];                                 // +0xB876..+0xB877
        u8 mOptionsDataProfileStorage[0x8000];           // +0xB878 (X360 object: 0x7370 bytes)
        // +0x12BE8 (76776) -- the live DLC-pack-1 options block, which the X360 lays
        // immediately after the options profile (47224 + 0x7370 == 76776). Unlike the block
        // above this type has no clashing includes, so it is modelled by name here.
        // GuiCache::Construct @0x82505860 seeds it and ProfileManager::ReadProfileData
        // @0x824FF298 reads its four words (`v5 = cache + 76776; field_2EEE8 = v5[0]; ..
        // = v5[3]`) into the stored image's mOptionsDataProfileDLC1 segment.
        OptionsDataProfileDLC1 mOptionsDataProfileDLC1;  // +0x12BE8 (76776)
        // ---- mPreRaceData: fly-by pre-event messages (GetPreEventInfo @0x824827D8) ----
        u8  maPreEventInfoStorage[3][580];               // +0x12F0C (77580) PreEventInfo maPreEventInfo[3] (stride 580)
        s32 miNumMessages;                               // +0x135D8 (79320) mPreRaceData.miNumMessages (GetPreEventInfo bound)
        // ---- mProfileEventState: offline profile-event CgsArray ----
        u8  mProfileEventStateStorage[1400];             // +0x135DC (79324) GetProfileEvent @0x82449880 forwards 79324; count is miProfileEventsCount @+1400
        s32 miProfileEventsCount;                         // +0x13B54 (80724) GetNumProfileEvents count / CgsArray sentinel (ctor -1)
        // ---- the latched Live Vision camera status ----
        // +0x13B58 (80728). Written in exactly two places: GuiCache::Construct @0x82506204
        // seeds it 0 (stwx of the zero register through r9 = 0x13B58), and GuiCache::RecEvent
        // @0x82510EDC latches the payload word of GUI event **570**
        // (BrnGui::GuiEventCamStatus, size 4, posted by
        // BrnNetworkManager::OutputPlayerStatusInfo through
        // BrnNetworkModule::AddOutputGuiEvent<GuiEventCamStatus> @0x82595788, which stamps
        // id 0x23A). Both the store and all 15 reads are 32-bit (stwx / lwzx), so this is a
        // word, not a bool.
        //
        // Every reader treats non-zero as "a Live Vision camera is attached": Intro
        // ::HandleTransitionFromWelcomeText @0x824D1B00 picks the "Intro_Take_Photo" voice
        // over when it is set and "Intro_No_Cam" when it is not, and the other fourteen are
        // all photo-booth / mugshot paths (PhotoBoothComponent OnLoad / ShowComponent /
        // SetButtonPromptVisible / SendPlayerPictureEvent, InstantResultsState
        // ::UpdatePhoto / ::UpdateTakePhotoPage, CrashNavDriverDetails, CompletedGame).
        //
        // PC has no Live Vision camera and no producer for event 570, so it stays 0 -- which
        // is exactly an Xbox 360 with no camera plugged in, and selects the retail
        // no-camera path in every one of those readers.
        s32 miCamStatus;                                 // +0x13B58 (80728)
        // ADDITIVE CARVE (A9 mode-type arm, 2026-08-27) from the head of mPad_13B5C[2]. One
        // byte, all three recovered accesses byte-wide:
        //   WRITTEN 0 by GuiCache::Construct @0x825061C4 (`stbx r30, r31, r8`, r8 == 0x13B5C)
        //   WRITTEN 0 by GuiCache::RecEvent case 93 @0x8250E9AC (`stbx r24, r31, r10`, r24 == 0)
        //   READ     by RaceMainHudState::UpdateWFInit @0x824802AC -- and it is the ONLY gate on
        //            `OnlineTimeoutComponent::Show(this + 0x65D0)` (@0x824802BC), behind the
        //            state's own mbOnlineTimeout byte at +0x163.
        // FLAG: NAME INFERRED from that single consumer (no DWARF row, no console accessor).
        // No member is shifted (1 + 1 == 2).
        bool mbOnlineTimeoutPending;                     // +0x13B5C (80732) FLAG: name inferred
        u8  mPad_13B5D[1];                               // +0x13B5D
        // +0x13B5E (80734). Carved out of the old mPad_13B5C span (2 + 1 + 53 == 56, so the
        // layout is unchanged). The one-shot "skip the car-select presentation" gate; every
        // access image-verified 2026-08-24:
        //   SET   by BrnGui::Intro::OnLeave @0x824D1640 (`stbx 1, mpGuiCache, 0x13B5E`) --
        //         leaving the first-boot intro arms the skip;
        //   READ  by CarSelectVehicle::SetupComponents @0x824C9978 (SET re-shows the ticker
        //         instead of running mMainAnimComponent's "transin"),
        //         CarSelectLivery::SetupComponents @0x824C8240 (SET disables the car-modify
        //         surface) and CarSelectLivery::Update @0x824DFCD0 (SET auto-accepts the
        //         selection on the first interactive frame, so the intro's junkyard visit
        //         never shows the colour-select screen);
        //   CLEAR by CarSelectLivery::OnLeave @0x824D6C30 and GuiCache::Construct.
        bool mbCarSelectTransitionAlreadyShown;          // +0x13B5E (80734)
        u8  mPad_13B5F[53];                              // +0x13B5F..+0x13B93
        f32 mfDistanceDriven;                            // +0x13B94 (80788) GetDistanceDriven (OdometerComponent::Update @0x82424160)
        u8  mPad_13B98[8];                               // +0x13B98..+0x13B9F
        // ---- replay slots / status interface / player tables ----
        s32 maReplayReelForSlot[6];                      // +0x13BA0 (80800) ReplayConvert... @0x824EEBE0 (4*(slot+20200)+this)
        s32 miReplaySlotsUsed;                           // +0x13BB8 (80824) ReplayConvert... bound
        u8  mReplayStatusInterfaceStorage[1572];         // +0x13BBC (80828) BrnReplays::ReplayIO::StatusInterface (ReplayConvert forwards 80828; GetReel)
        // 16 replay-player-active entries, stride 32 (lead CgsNetwork::PlayerName + value@+0x10 + count@+0x18).
        ReplayPlayerActive maReplayPlayersActive[16];    // +0x141E0 (82400) GetSortedReplayPlayerActive @0x824EEFA0 (32*(idx+2575)+this); qsort'd by muActiveCount
        bool maReplayARCRendered[8];                     // +0x143E0 (82912) IsReplayARCRendered @0x824EEDA8
        bool mbReplayHasBeenSorted;                      // +0x143E8 (82920) SortReplayPlayersActive / GetSortedReplayPlayerActive gate

        static void _AssertLayout();
    };

    // The former inline GuiCache::_AssertLayout absolute/relative X360 byte-offset pin
    // block is RETIRED at the l2 merge: the merged layout embeds the widening
    // StateLoadingHelper (+0x8) and the 0x8000-byte options-profile reservation, so the
    // "single pointer cluster + X360-sized pads" premise those pins depended on no
    // longer holds. Per the x64 gate rule the layout contract is semantic parity by
    // NAMED members; the documented X360 offsets in the member comments above remain
    // the offset authority.
    inline void GuiCache::_AssertLayout() {}


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

    private:
        // [H3b] named storage over the documented offsets so the accessor bodies
        // (BrnGuiCache_wH3b.cpp) read by name. Stride 0x2C: the mEvents CgsArray's
        // 7700-byte storage / its 175-entry landmark cap == 44, consistent with the
        // +0x28 id being the record's last word.
        u8  mauHead[0x20];        // +0x00..+0x1F (not in this slice)
        u32 muPositionLookupId;   // +0x20
        u8  mauPad24[4];          // +0x24..+0x27
        u32 muEventId;            // +0x28
    };
    static_assert(sizeof(PresetEvent) == 0x2C, "preset-event record stride (7700/175)");
}
