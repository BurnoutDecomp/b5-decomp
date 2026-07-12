#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"     // CgsID
#include "GameSource/Gui/Flow/BrnBaseFlow.h"       // BrnGui::BrnBaseFlow (base)

namespace CgsGui     { struct GuiAccessPointers; struct State; }
namespace CgsMemory  { class  LinearMalloc; }
namespace rw         { struct IResourceAllocator; }

// BrnGui::BrnHudFlow - the HUD GUI flow (E_GUIFLOW_HUD). A BrnBaseFlow that owns a *pool* of 14
// concrete CgsGui::State objects -- the boot states (BF_PRELOAD..BF_LOADING) and the in-game HUD
// states (RACE_MAIN..PRE_FLY_BY) -- and installs them into its embedded CgsGui::StateMachine. The
// GuiFsmController sequences the boot by loading each single-state FSM LuaCode bundle into this
// flow's state machine (PrepareLua) and letting the matching C++ state (selected by the script's
// SetState) run; when a state signals done the controller loads the next bundle.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct @0x824F1E78  Prepare @0x8251A620  Update @0x82508620  PrintStateSizes @0x824F1E80
// The 14 members are the X360 fields flow+0x1024C..flow+0x10280 (one CgsGui::State* each, in the
// Prepare build order); on x64 the offsets differ (8-byte pointers) but the member order is faithful.
namespace BrnGui
{
    class ProfileManager;   // GameSource/Gui/... (threaded to BF_PROFILE; held by ptr only)

    // boot states
    struct BootPreload;
    struct BootVideos;
    struct BootLegal;
    struct BootAttract;
    struct PostTitleScreenLoad;   // the BF_COMPLOAD slot (DWARF: mpStatePostTitleScreenLoad)
    struct BootProfile;
    struct BootLoading;
    // in-game HUD states
    struct RaceMainHudState;
    struct FBurnMainHudState;
    struct PausedHudState;
    struct CrashedHudState;
    struct CrashedStuntHudState;
    struct IdleHudState;
    struct PreRaceFlyByState;

    struct BrnHudFlow : public BrnBaseFlow
    {
        static const s32 KI_NUM_HUD_STATES = 14;   // BrnHudFlow::Prepare SetStates(...,14)

        // @ 0x824F1E78 -- chain to BrnBaseFlow::Construct (stash the GUI cache + reset bookkeeping).
        virtual void Construct(GuiCache* lpGuiCache);

        // @ 0x82508620 -- BrnBaseFlow::Update wrapped in the HUD-flow CPU perf monitor.
        virtual void Update();

        // @ 0x8251A620 -- base-prepare, then build + install the 14-state HUD pool. The wider
        // overload (adds the linear allocator the states are carved from + the profile manager BF_
        // PROFILE needs -- forwarded into BootProfile's 3-arg Construct, X360 vtable slot 9).
        // Distinct vtable slot from BrnBaseFlow::Prepare(access, allocator).
        bool Prepare(CgsGui::GuiAccessPointers* lpAccessPointers,
                     rw::IResourceAllocator* lpAllocator,
                     CgsMemory::LinearMalloc* lpLinearMalloc,
                     ProfileManager* lpProfileManager);

    private:
        // The 14-state pool, in BrnHudFlow::Prepare build order (X360 flow+0x1024C..+0x10280).
        BootPreload*          mpPreload;               // BF_PRELOAD
        BootVideos*           mpVideos;                // BF_VIDEOS
        BootLegal*            mpLegal;                 // BF_LEGAL
        BootAttract*          mpAttract;               // BF_ATTR
        PostTitleScreenLoad*  mpPostTitleScreenLoad;   // BF_COMPLOAD (DWARF slot name; plays the post-title intro)
        BootProfile*          mpProfile;               // BF_PROFILE
        BootLoading*          mpLoading;        // BF_LOADING
        RaceMainHudState*     mpRaceMain;       // RACE_MAIN
        FBurnMainHudState*    mpFBurnMain;      // FBURN_MAIN
        PausedHudState*       mpPaused;         // PAUSED
        CrashedHudState*      mpCrashed;        // CRASHED
        CrashedStuntHudState* mpCrashedStunt;   // CRASHEDSTNT
        IdleHudState*         mpIdle;           // IDLE
        PreRaceFlyByState*    mpPreRaceFlyBy;   // PRE_FLY_BY
    };
}
