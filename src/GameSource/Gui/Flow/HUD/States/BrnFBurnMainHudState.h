#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                       // BrnFlapt::MovieClipRef (the RaceMainHUD_mc clip)
#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsList.h"               // BrnGui::FriendsListComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsListChangeIcon.h"     // BrnGui::FriendsListChangeIconComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnInGameMessagesComponent.h"   // BrnGui::InGameMessagesComponent
#include "GameSource/Gui/Flow/hud/Components/BrnDistrictMarker.h"            // BrnGui::DistrictMarkerComponent (the full class; the View ODR-fork slice is retired)
#include "GameSource/Gui/Flow/HUD/Components/BrnJunctionInfoComponent.h"     // BrnGui::JunctionInfoComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnOdometerComponent.h"         // BrnGui::OdometerComponent
#include "GameSource/Gui/Flow/HUD/Components/BrnRoadRuleComponent.h"        // BrnGui::RoadRuleComponent (H2 wave)
#include "GameSource/Gui/SatNav/BrnSatNavComponent.h"                        // [H3b] BrnGui::SatNavComponent (+0x160)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h" // BrnGui::FlaptAnimatorComponent
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"          // CgsGui::GuiComponent (the apt-side animator half)

namespace BrnGui
{
    class GuiCache;

    // BrnGui::FBurnMainHudState - the freeburn (free-drive) main-HUD GUI state, the
    // FBURN_MAIN slot of the BrnHudFlow 14-state pool. Reconstructed from
    // BURNOUT_X360_ARTIST.XEX:
    //   OnEnter @0x8247B0E8, OnLeave @0x82480B88, UpdateLoading @0x8247C640,
    //   UpdateSetupState @0x82480EA0, UpdateWFInit @0x8247C710, UpdateRunning @0x8247B660,
    //   UpdatePermenant @0x824810F0, ProcessAptEvents @0x82475048,
    //   ProcessBoostInfo @0x82474F60, UpdateSatNav @0x82475268,
    //   SetExpectedAptComponentList @0x82475328, GetResourcesToLoad @0x825084D0.
    // The X360 object is a ~0x5350-byte aggregate (ctor @0x82508388 installs ~50
    // sub-object vtables); the X360 byte offsets recorded on each member below are the
    // ASM displacements. Per the x64 gate the host layout is semantic-parity-by-named-
    // members: components with complete recon headers are embedded by value; components
    // whose TUs are not yet reconstructed (the SatNav body, BoostMessageManager) are
    // documented absentees -- their calls are FLAG'd deferrals in the .cpp and their
    // storage is not fabricated here. (H1 wave 2026-08-25: Odometer, JunctionInfo and the
    // full DistrictMarker are REAL embedded members now.)
    struct FBurnMainHudState : public CgsGui::State
    {
        // The internal phase word (X360 this+0x38; OnEnter stores 0, OnLeave stores 4).
        // Phase order proven by the data flow: the cache is captured from event 64 in
        // UpdateSetupState, UpdateLoading needs that cache for EnsureResourcesAreLoaded,
        // and UpdateWFInit gates on the apt components the loading phase mounted.
        enum EInternalState
        {
            E_INTERNALSTATE_SETUP   = 0,   // capture the GuiCache (event 64), config gates
            E_INTERNALSTATE_LOADING = 1,   // resource list + the B5RaceHud mount
            E_INTERNALSTATE_WFINIT  = 2,   // wait apt-init, engine-state transition-in
            E_INTERNALSTATE_RUNNING = 3,   // the per-frame HUD event dispatch
            E_INTERNALSTATE_LEAVING = 4,   // stored by OnLeave
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825084D0 - hands the freeburn main-HUD state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // The phase bodies (addresses above). Each bool phase returns true to advance.
        bool UpdateSetupState();
        bool UpdateLoading();
        bool UpdateWFInit();
        void UpdateRunning();
        void UpdatePermenant();

        void ProcessAptEvents(const s32* lpEvent);
        void ProcessBoostInfo(const void* lpEvent);
        void UpdateSatNav(const void* lpEvent, s32 liEventId);
        void SetExpectedAptComponentList();

        // [H3b] the sat-nav events-filter posts (X360 @0x8247CAE8 / @0x8247CB80 --
        // the id-204 GuiEventEnableSatNavIcons records on the view channels).
        void EnableSatNavEventsFilter();
        void DisableSatNavEventsFilter();

        // The 42-entry static resource list (X360 .rdata @0x82F26230 / count
        // @0x82F2622C -- values read from the XEX image; the .cpp table names each id).
        static const CgsGui::sResourceTuple maResourcesToLoad[];
        static const u32                    muNumResourcesToLoad;

        // The 56 observed event ids (X360 .rdata @0x8205AED8; values from the image).
        static const s32 maiEventToObserve[];
        static const s32 miNumEventsObserved;

        // ---- members (X360 byte offsets in comments; access is by name) ------------
        EInternalState meInternalState;            // +0x038

        GuiCache* mpGuiCache;                      // +0x140 (captured from event 64)
        BrnFlapt::MovieClipRef mRaceMainHudClip;   // +0x144/+0x148 (the RaceMainHUD_mc ref)

        // The ten component-enable bytes (+0x14C..+0x155). UpdateSetupState sets them
        // all; each gates its component's calls everywhere else. Named for their gated
        // component (the X360 byte order preserved).
        bool mbSatNavEnabled;                      // +0x14C
        bool mbInGameMessagesEnabled;              // +0x14D
        bool mbEnable14E;                          // +0x14E (consumer not yet attested)
        bool mbBoostMessagesEnabled;               // +0x14F
        bool mbRoadRulesEnabled;                   // +0x150
        bool mbFriendsListEnabled;                 // +0x151
        bool mbJunctionInfoEnabled;                // +0x152
        bool mbOdometerEnabled;                    // +0x153
        bool mbPpToggleEnabled;                    // +0x154
        bool mbDistrictMarkerEnabled;              // +0x155

        // ⭐ [H3b] the SatNavComponent aggregate (X360 +0x160..+0x3D0; the old "absent
        // member" FLAG is retired -- the full class landed in H3a and the H3b slice
        // mounts it). The X360 per-frame pre-pass reaches this component's
        // mpPlayerInfo/+0x130 and its manager's count through +0x254 (friend grants on
        // both classes). The two config members that FOLLOW it carry their DWARF names
        // (BrnFBurnMainHudState.h:176/:177) -- the old miSatNavShowState /
        // miSatNavEventsFilter stand-in names had the pair's roles swapped.
        SatNavComponent mSatNavComponent;          // +0x160
        s32  meSatNavEventFilter;                  // +0x3E0 (cache+0x8034 mirror; EModeType spelled s32)
        bool mbEventFilterEnabled;                 // +0x3E4 (cache+0x8038 mirror)

        InGameMessagesComponent mInGameMessages;   // +0x3E8 (setters real; ctor deferred)
        DistrictMarkerComponent mDistrictMarker;   // +0x834 (the FULL marker class since the
                                                   //         H1 wave -- Construct/Prepare/
                                                   //         SetCounty/SetDistrict/
                                                   //         SetHideCountyIcon all real)
        bool mbDistrictRefreshArmed;               // +0x8AC (OnEnter stores 1; the +0x8AC
                                                   //         word also guards the county
                                                   //         refresh in UpdateRunning)

        // FLAG absent member: BoostMessageManager (+0x8B0..~+0xA3F) -- TU not
        // reconstructed (symbols demangle-mishomed to CgsStrStream.h in the ledger);
        // calls deferred behind mbBoostMessagesEnabled.

        // The "EventHud_Animator" pair: the APT half (a plain named GuiComponent -- the
        // X360 UpdateWFInit calls CgsGui::GuiComponent::AddOutputAptViewState on
        // field_A40 directly, so this is the base type, NOT a Flapt component) and the
        // FLAPT goto-and-play half.
        CgsGui::GuiComponent   mEventHudAnimatorIcon; // +0xA40 ("EventHud_Animator" pair, 1st)
        FlaptAnimatorComponent mEventHudAnimator;     // +0xACC (the Run(...) target)

        // The road-rules HUD panel (H2 wave 2026-08-25: the full lifecycle/handler TU
        // landed; the old absent-member FLAG is retired). The X360 event-338 "mirrors"
        // at +0x102C/+0x1034 are the component's OWN mfTargetCrashScore /
        // miCrashMultiplier (this+0xB10 + 0x51C/0x524) -- UpdateRunning pokes them
        // directly through the friend grant in the component header.
        RoadRuleComponent mRoadRuleComponent;      // +0xB10

        FriendsListComponent mFriendsList;         // +0x1040
        bool mbFriendsListOffline;                 // +0x18D9 (mode == -1 mirror)

        FriendsListChangeIconComponent mFriendsListChangeIcon; // +0x5160

        // The junction-info panel and the odometer readout (H1 wave 2026-08-25: both TUs
        // complete, the old absent-member FLAGs retired).
        JunctionInfoComponent mJunctionInfoComponent;  // +0x5178
        OdometerComponent     mOdometer;               // +0x52A8

        FlaptAnimatorComponent mIdentAnimator;     // +0x5390 ("Ident_Animator")

        bool mbCountyIconHidden;                   // +0x5279 (SetHideCountyIcon arg mirror)
        bool mbTrophyRoadMarker;                   // +0x527B (profile==data marker)
        bool mbPpToggleActive;                     // +0x53C8
        f32  mfPpToggleNextTime;                   // +0x53CC
        s32  miPpToggleRunCount;                   // +0x53D0
        f32  mfBoostAmountPrev;                    // +0x53D4
        bool mbTrophyUnlockScanned;                // +0x53D8 (OnEnter zeroes)
    };
}
