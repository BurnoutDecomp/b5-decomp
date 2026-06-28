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

        // ADDITIVE GROW (BrnFriendsList TU): FriendsListComponent::SetGuiCachePointer
        // (X360 @0x82473580) latches the cache pointer, then caches a single u32 the
        // cache exposes at a far member (X360 lwzx r,this,0xAC74). The component reads it
        // once at attach time and stores it locally; its exact semantic is not recovered,
        // so it is exposed by name as an opaque u32. Body links from the GuiCache TU.
        u32 GetFriendsListCachedField() const;   // X360 far member @0xAC74
    };
}
