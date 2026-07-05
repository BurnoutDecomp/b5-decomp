#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiCache.h"   // BrnGui::GuiCache (mpGuiCache->GetGameMode())

// BrnGui::InstantResultsState - the offline post-event instant-results presentation
// flow state. This header carries the class shape and the inline resource accessor; the
// large component/substate machinery is reconstructed with the class TU.
// Layout/virtuals from the DecFIGS DWARF (BrnOfflineInstantResults.h). The CgsGui::State
// base prefix widens on the x64 gate so absolute X360 byte offsets are DOCUMENTARY only --
// members are reached BY NAME; untouched spans are opaque padding.
namespace BrnGui
{
    struct InstantResultsState : public CgsGui::State
    {
        // ---- enums (DWARF BrnOfflineInstantResults.h) -----------------------------------
        enum EResultsActiveSubStates                       // DWARF h:95
        {
            E_ACTIVE_SUBSTATE_EVENT_NONE            = -1,
            E_ACTIVE_SUBSTATE_EVENT_RESULTS         = 0,
            E_ACTIVE_SUBSTATE_EVENT_RESULTS_TWO     = 1,
            E_ACTIVE_SUBSTATE_EVENT_TAKE_PHOTO      = 2,
            E_ACTIVE_SUBSTATE_EVENT_RANK_UP_TEXT    = 3,
            E_ACTIVE_SUBSTATE_EVENT_RANK_UP_LICENSE = 4,
            E_ACTIVE_SUBSTATE_EVENT_CAR_UNLOCK      = 5,
            E_ACTIVE_SUBSTATE_EVENT_FREE_CAR_UNLOCK = 6,
            E_ACTIVE_SUBSTATE_EVENT_RANK_UP_SHOWING_RIVALS = 7,
            E_ACTIVE_SUBSTATE_EVENT_LEAVING         = 8,
            E_ACTIVE_SUBSTATE_EVENT_DONE            = 9,
            E_ACTIVE_SUBSTATE_EVENT_COUNT           = 10,
        };
        enum EResultsAnimations                            // DWARF h:122
        {
            E_RESULTS_DETAILED_WIN      = 0,
            E_RESULTS_WIN_WITH_TARGETS  = 1,
            E_RESULTS_PLAIN_WIN         = 2,
            E_RESULTS_DETAILED_LOSS     = 3,
            E_RESULTS_LOSS_WITH_TARGETS = 4,
            E_RESULTS_PLAIN_LOSS        = 5,
            E_RESULTS_COUNT             = 6,
        };

        InstantResultsState();                             // @0x825006D8

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82500808 - hands the instant-results state's static resource list to the loader.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        EResultsActiveSubStates GetNextSubstate();         // @0x824B3820  (DWARF h:601)
        void                    ResetStateTimer();         // @0x824B38C0  (DWARF h:633)
        void                    SetEventIconResource();    // @0x824B39B0  (DWARF h:709)

        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @0x82F26AFC (.rdata)
        static const u32                    muNumResourcesToLoad; // @0x82F26B0C (.rdata)

        // ---- instance members reached by this TU (X360 byte offsets, DOCUMENTARY) --------
        // DWARF member NAMES; opaque spans model the untouched component/substate machinery
        // owned by the (not-yet-reconstructed) rest of the class TU.
        u8   mPadToLargeEventIcon[0x19D8];   // [State base .. +0x19D8) opaque prefix
        u32  muLargeEventIconResource;       // +0x19D8  resource-id slot of mLargeEventIcon
                                             //          (IconComponent, DWARF h:401);
                                             //          SetEventIconResource writes 116..120 here
        u8   mPad19DC[0x1FA8 - 0x19DC];      // +0x19DC .. +0x1FA8 opaque
        EResultsAnimations meWinState;       // +0x1FA8  (DWARF h:424)
        u8   mPad1FAC[0x21FC - 0x1FAC];      // +0x1FAC .. +0x21FC opaque
        GuiCache* mpGuiCache;                // +0x21FC  (DWARF h:419)
        u8   mPad2200[0x220C - 0x2200];      // +0x2200 .. +0x220C opaque/align
        EResultsActiveSubStates meActiveSubState; // +0x220C  (DWARF h:426)
        bool mabSubStateFlags[10];           // +0x2210  (DWARF h:427; == meActiveSubState+4)
        u8   mPad221A[0x22E8 - 0x221A];      // +0x221A .. +0x22E8 opaque
        s32  mCarUnlockId;                   // +0x22E8  CgsID (DWARF h:443); ctor writes -1
        u8   mPad22EC[0x2310 - 0x22EC];      // +0x22EC .. +0x2310 opaque
        s32  mPendingRivalId;                // +0x2310  CgsID (DWARF h:448); ctor writes -1
        u8   mPad2314[0x233C - 0x2314];      // +0x2314 .. +0x233C opaque
        f32  mfTimeRemaining;                // +0x233C  (DWARF h:462, last member)
    };
}
