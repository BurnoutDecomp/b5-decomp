#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"   // BrnGui::GuiFlow (AppendExpectedAptComponent selector)

// BrnGui::GuiCache subsystem (DecFIGS DWARF: BrnGuiCache.h). StateLoadingHelper is the
// resource/component watcher embedded in the cache; GuiCache is the cache itself. Only
// the methods reached by the in-scope GUI code are declared on GuiCache (its full data
// layout is an out-of-scope boundary object the leaves only touch through these calls).
namespace CgsGui { class ObjectController; }
namespace BrnResource { class ChallengeList; } // GetFreeburnChallengeList return (pointer only)

namespace BrnGui
{
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
    };
}
