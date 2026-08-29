#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiCache.h"          // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"  // GuiEventOfflinePostEvent::OfflinePostEventData
#include "GameSource/Gui/BrnGuiTextField.h"      // BrnGui::TextField
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"
#include "GameSource/Gui/Flow/Screen/Components/BrnManufacturerIcon.h"
#include "GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.h"
#include "GameSource/Gui/Flow/Screen/Components/BrnPhotoBoothComponent.h"
#include "GameSource/Gui/Flow/Screen/Components/BrnLargeCarComponent.h"

// ==========================================================================================
// BrnGui::InstantResultsState -- the offline post-event instant-results presentation flow
// state. This is the screen an offline event's finish is supposed to put on the display.
//
// ⭐⭐ THIS HEADER USED TO BE 0x2340 BYTES OF `mPad*` WITH FOUR NAMED MEMBERS. It now carries
// the real member list. That mattered for more than tidiness: OnEnter @0x824C3398 CONSTRUCTS
// TWENTY-FOUR EMBEDDED COMPONENTS BY VALUE, and it cannot be written at all against a class
// that models them as padding.
//
// HOW THE COMPONENT LIST WAS RECOVERED (asm, not inference). The X360 constructor
// @0x825006D8 is nothing but inlined member default-construction: one `stw <vtable>` per
// embedded component, in address order. Reading those stores off gives both the ORDER and
// each component's STRIDE, and every stride matches the sizeof of the type the PS3 DWARF
// names in that slot:
//     +0x0038  mHelpItems[3]                428 = 0x1AC  (GuiComponent 0x8C + 2 x ButtonIcon 0x90)
//     +0x053C  mFinishedText                296 = 0x128  TextField
//     +0x0664  mTargetResultText            296
//     +0x078C  mUpgradeText                 296
//     +0x08B4  mUpgradeStateAnimator        140 = 0x8C   AnimationComponent
//     +0x0940  mShutdownText                296
//     +0x0A68  mCarUnlockManuIcon           140         ManufacturersIcon
//     +0x0AF4  mCarUnlockText               296
//     +0x0C1C  mCarUnlockDescText           296
//     +0x0D44  mLicense                    1996 = 0x7CC  LicenseComponent
//     +0x1510  mPhotoBoothComponent        1076 = 0x434  PhotoBoothComponent
//     +0x1944  mLargeEventIcon              148 = 0x94   IconComponent
//     +0x19D8  mLargeIconResource             8          sResourceTuple
//     +0x19E0  mNewRivalManuIcon            140
//     +0x1A6C  mNewRivalCarText             296
//     +0x1B94  mNewRivalDescText            296
//     +0x1CC0  mUnlockedXSCarComponent      200 = 0xC8   LargeCarComponent
//     +0x1D88  mUnlockedRivalCarComponent   200
//     +0x1E50  mUnlockedFreeCarComponent    200
//     +0x1F18  mResultsIcon                 148
//     +0x1FAC  mSecondResultsIcon           148
//     +0x2040  mCarUnlockIcon               148
//     +0x20D4  mRankUpIcon                  148
//     +0x2168  mNewRivalsIcon               148
// The 5-icon run at 0x1F18/0x1FAC/0x2040/0x20D4/0x2168 has an exact 0x94 stride and the
// 3-car run at 0x1CC0/0x1D88/0x1E50 an exact 0xC8 stride, which is what makes the
// identification a measurement rather than a reading of the DWARF.
//
// ⭐ CORRECTION TO THE PREVIOUS HEADER (it named +0x19D8 `muLargeEventIconResource`, "the
// resource-id slot of mLargeEventIcon"). +0x19D8 is NOT inside the icon: mLargeEventIcon
// starts at 0x1944 and IconComponent is 0x94 bytes, so it ENDS at 0x19D8. +0x19D8 is its own
// member, the `sResourceTuple mLargeIconResource` the DWARF declares next (h:402) -- and the
// binary says so in words: Update @0x824DF760 fires the assert whose stringized condition is
// literally "0 != mLargeIconResource.muId". SetEventIconResource writes the id half; Update
// reads BOTH halves (`PlayAptMovie(io, off_82F278E0[mLargeIconResource.muId], 2)`), which the
// old single-u32 model could not express.
//
// Layout/virtual shape from the DecFIGS DWARF (BrnOfflineInstantResults.h), every slot gated
// on an X360 store/load as above. The X360 byte offsets in the comments are DOCUMENTARY: the
// CgsGui::State base and every component widen on the x64 gate, so members are reached BY
// NAME. What the offsets pin is the ORDER, and the order is what the asm proves.
// ==========================================================================================
namespace BrnGui
{
    struct InstantResultsState : public CgsGui::State
    {
        // ---- enums (DWARF BrnOfflineInstantResults.h) -----------------------------------
        enum EResultsInternalStates                        // DWARF h:83
        {
            E_RESULTS_STATE_INVALID            = -1,
            E_RESULTS_STATE_UNLOADED           = 0,
            E_RESULTS_STATE_LOADING_RESOURCES  = 1,
            E_RESULTS_STATE_LOADING_COMPONENTS = 2,
            E_RESULTS_STATE_ACTIVE             = 3,
            E_RESULTS_STATE_PHOTO_INTERRUPT    = 4,
            E_RESULTS_STATE_COUNT              = 5,
        };
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
        enum EResultsSubStateStates                        // DWARF h:113
        {
            E_SUBSTATE_INVALID           = -1,
            E_SUBSTATE_SET_UP_COMPONENTS = 0,
            E_SUBSTATE_RUNNING           = 1,
            E_SUBSTATE_COUNT             = 2,
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
        enum EPhotoPresentationStages                      // DWARF h:163
        {
            E_PHOTO_PRESENTATION_WAITING_FOR_CLEANUP = 0,
            E_PHOTO_PRESENTATION_LOADING             = 1,
            E_PHOTO_PRESENTATION_INITIALISING        = 2,
            E_PHOTO_PRESENTATION_RUNNING             = 3,
            E_PHOTO_PRESENTATION_OLD_LICENSE_RECAP   = 4,
            E_PHOTO_PRESENTATION_DONE                = 5,
            E_PHOTO_PRESENTATION_COUNT               = 6,
        };
        enum ERankUpPresentationStages                     // DWARF h:175
        {
            E_RANKUP_PRESENTATION_WAITING_FOR_CLEANUP_ONE = 0,
            E_RANKUP_PRESENTATION_WAITING_FOR_CLEANUP_TWO = 1,
            E_RANKUP_PRESENTATION_LOADING                 = 2,
            E_RANKUP_PRESENTATION_INITIALISING            = 3,
            E_RANKUP_PRESENTATION_RUNNING                 = 4,
            E_RANKUP_PRESENTATION_LEAVING                 = 5,
            E_RANKUP_PRESENTATION_CLEANING_UP             = 6,
            E_RANKUP_PRESENTATION_RESTORING_STATE         = 7,
            E_RANKUP_PRESENTATION_DONE                    = 8,
            E_RANKUP_PRESENTATION_COUNT                   = 9,
        };
        enum ECarUnlockPresentationStages                  // DWARF h:190
        {
            E_CAR_UNLOCK_PRESENTATION_WAITING                 = 0,
            E_CAR_UNLOCK_PRESENTATION_INTRO_SET_UP            = 1,
            E_CAR_UNLOCK_PRESENTATION_SHOWING_CAR_SET_UP_ICON = 2,
            E_CAR_UNLOCK_PRESENTATION_SHOWING_CAR             = 3,
            E_CAR_UNLOCK_PRESENTATION_OUTRO_SET_UP            = 4,
            E_CAR_UNLOCK_PRESENTATION_OUTRO                   = 5,
            E_CAR_UNLOCK_PRESENTATION_OUTRO_ENDING            = 6,
            E_CAR_UNLOCK_PRESENTATION_CLEANING_UP             = 7,
            E_CAR_UNLOCK_PRESENTATION_DONE                    = 8,
            E_CAR_UNLOCK_PRESENTATION_COUNT                   = 9,
        };
        enum ENewRivalsPresentationStages                  // DWARF h:217
        {
            E_NEW_RIVALS_PRESENTATION_WAITING                    = 0,
            E_NEW_RIVALS_PRESENTATION_INTRO_SET_UP               = 1,
            E_NEW_RIVALS_PRESENTATION_INTRO                      = 2,
            E_NEW_RIVALS_PRESENTATION_SHOWING_RIVAL_SET_UP_TEXT  = 3,
            E_NEW_RIVALS_PRESENTATION_SHOWING_RIVAL_SET_UP_ICON  = 4,
            E_NEW_RIVALS_PRESENTATION_SHOWING_RIVAL              = 5,
            E_NEW_RIVALS_PRESENTATION_OUTRO_SET_UP               = 6,
            E_NEW_RIVALS_PRESENTATION_OUTRO                      = 7,
            E_NEW_RIVALS_PRESENTATION_OUTRO_ENDING               = 8,
            E_NEW_RIVALS_PRESENTATION_CLEANING_UP                = 9,
            E_NEW_RIVALS_PRESENTATION_DONE                       = 10,
            E_NEW_RIVALS_PRESENTATION_COUNT                      = 11,
        };
        enum EFreeCarPresentationStages                    // DWARF h:234
        {
            E_FREE_CAR_UNLOCK_PRESENTATION_WAITING                 = 0,
            E_FREE_CAR_UNLOCK_PRESENTATION_INTRO_SET_UP            = 1,
            E_FREE_CAR_UNLOCK_PRESENTATION_SHOWING_CAR_SET_UP_ICON = 2,
            E_FREE_CAR_UNLOCK_PRESENTATION_SHOWING_CAR             = 3,
            E_FREE_CAR_UNLOCK_PRESENTATION_OUTRO_SET_UP            = 4,
            E_FREE_CAR_UNLOCK_PRESENTATION_OUTRO                   = 5,
            E_FREE_CAR_UNLOCK_PRESENTATION_OUTRO_ENDING            = 6,
            E_FREE_CAR_UNLOCK_PRESENTATION_CLEANING_UP             = 7,
            E_FREE_CAR_UNLOCK_PRESENTATION_DONE                    = 8,
            E_FREE_CAR_UNLOCK_PRESENTATION_COUNT                   = 9,
        };

        static const s32 KI_HELPITEMS = 3;                 // DWARF h:259

        InstantResultsState();                             // @0x825006D8

        virtual void OnEnter();                            // @0x824C3398 (cpp:265)
        virtual void OnLeave();                            // @0x824C3930 (cpp:399)
        virtual void Update();                             // @0x824DF760 (cpp:504)

        // @0x82500808 - hands the instant-results state's static resource list to the loader.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // ---- BODIED in BrnOfflineInstantResults.cpp -------------------------------------
        void                    AppendAllExpectedComponents();     // @0x824BB458 (cpp:753)
        void                    AppendExpectedScreenComponents();  // @0x824B3CB0 (cpp:772)
        void                    HandleIncomingEvents();            // @0x824DBAD8 (cpp:812)
        void                    SelectSubstates();                 // @0x824D59B0
        void                    UpdateSubstate();                  // @0x824DC188
        bool                    HasSubstateTimedOut();             // @0x824B48C8 (cpp:3250)
        bool                    TickSubstateAndEndIfDone();        // @0x824BB4D8
        void                    TriggerExitResults();              // @0x824D58A8 (cpp:3294)
        EResultsActiveSubStates GetNextSubstate();                 // @0x824B3820  (DWARF h:601)
        void                    ResetStateTimer();                 // @0x824B38C0  (DWARF h:633)
        void                    SetEventIconResource();            // @0x824B39B0  (DWARF h:709)

        // ---- ⛔ NOT RECONSTRUCTED YET. Declared so the dispatch above can be written
        //      faithfully; bodied as LOGGED stubs in BrnScreenStatesDataLinkStubs.cpp so the
        //      gap prints in BrnGame.log instead of being a silent no-op. Instruction counts
        //      are the X360 export's. NONE of them is on the path that puts the results movie
        //      on screen -- that is Update's E_RESULTS_STATE_LOADING_RESOURCES arm, which runs
        //      before any sub-state does.
        void SetupComponents();            // @0x824B3FF0 (492)  fills the result text/icons
        void HandleAptTriggers(const void* lpAptTrigger);   // @0x824BDAB8 (475)
        void HandleControllerInput(const void* lpInputEvent); // @0x824B3E00 (124)
        void UpdateEventResults();         // @0x824BE228 (184)
        void UpdateSecondResultsPage();    // @0x824BE508 (103)
        void UpdateTakePhotoPage();        // @0x824C3C70
        void UpdateRankUp();               // @0x824BE6A8 (77)
        void UpdateLicense();              // @0x824C4F98
        void UpdateCarUnlock();            // @0x824C4870
        void UpdateFreeCarUnlock();        // @0x824C41B0
        void UpdateShowingRivals();        // @0x824C5598
        void UpdateLeaving();              // @0x824BE7E0 (68)
        void UpdatePhoto();                // @0x824B47C8
        bool IsXSCarInUnlockedArray();     // @0x824C0730
        bool WillShowCredits();            // @0x824C5C38 (59)  needs WorldDataController::
                                           //   GetProgressionData, which has no home yet

        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @0x82F26AFC (.rdata)
        static const u32                    muNumResourcesToLoad; // @0x82F26B0C (.rdata)

        // ---- embedded components, in X360 constructor order (see the banner) -------------
        HelpItem            mHelpItems[KI_HELPITEMS];      // +0x0038  (DWARF h:384)
        TextField           mFinishedText;                 // +0x053C  (h:385)
        TextField           mTargetResultText;             // +0x0664  (h:386)
        TextField           mUpgradeText;                  // +0x078C  (h:388)
        AnimationComponent  mUpgradeStateAnimator;         // +0x08B4  (h:389)
        TextField           mShutdownText;                 // +0x0940  (h:391)
        ManufacturersIcon   mCarUnlockManuIcon;            // +0x0A68  (h:394)
        TextField           mCarUnlockText;                // +0x0AF4  (h:395)
        TextField           mCarUnlockDescText;            // +0x0C1C  (h:396)
        LicenseComponent    mLicense;                      // +0x0D44  (h:398)
        PhotoBoothComponent mPhotoBoothComponent;          // +0x1510  (h:399)
        IconComponent       mLargeEventIcon;               // +0x1944  (h:401)
        CgsGui::sResourceTuple mLargeIconResource;         // +0x19D8  (h:402) -- see the
                                                           //   "0 != mLargeIconResource.muId"
                                                           //   assert in Update.
        ManufacturersIcon   mNewRivalManuIcon;             // +0x19E0  (h:405)
        TextField           mNewRivalCarText;              // +0x1A6C  (h:406)
        TextField           mNewRivalDescText;             // +0x1B94  (h:407)
        LargeCarComponent   mUnlockedXSCarComponent;       // +0x1CC0  (h:409)
        LargeCarComponent   mUnlockedRivalCarComponent;    // +0x1D88  (h:410)
        LargeCarComponent   mUnlockedFreeCarComponent;     // +0x1E50  (h:411)
        IconComponent       mResultsIcon;                  // +0x1F18  (h:413)
        IconComponent       mSecondResultsIcon;            // +0x1FAC  (h:414)
        IconComponent       mCarUnlockIcon;                // +0x2040  (h:415)
        IconComponent       mRankUpIcon;                   // +0x20D4  (h:416)
        IconComponent       mNewRivalsIcon;                // +0x2168  (h:417)

        // ---- scalar tail. Every offset below is a store in OnEnter @0x824C3398 or a load in
        //      Update @0x824DF760 / SelectSubstates @0x824D59B0, so the run is contiguous and
        //      fully accounted for -- there is no unexplained gap between 0x21FC and 0x2340.
        GuiCache*                    mpGuiCache;                  // +0x21FC (h:419)
        EResultsInternalStates       meCurrentState;              // +0x2200 (h:420) -- the
                                                                  //   "(meCurrentState = " print
        BrnGuiResourceId             meCurrentMainMovie;          // +0x2204 (h:422)
        EResultsAnimations           meWinState;                  // +0x2208 (h:424)
        EResultsActiveSubStates      meActiveSubState;            // +0x220C (h:426)
        bool                         mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_COUNT];  // +0x2210 (h:427)
        EResultsSubStateStates       meSubStateState;             // +0x221C (h:429)
        f32                          mfTimeToShowLicense;         // +0x2220 (h:431)
        bool                         mbLicenseShown;              // +0x2224 (h:432)
        f32                          mfTimeToIncrementWin;        // +0x2228 (h:434)
        bool                         mbWinsIncremented;           // +0x222C (h:435)
        EPhotoPresentationStages     mePhotoPresentationStage;    // +0x2230 (h:437)
        ERankUpPresentationStages    meRankUpPresentationStage;   // +0x2234 (h:439)
        bool                         mbStartedUpgradeTransOut;    // +0x2238 (h:440)
        ECarUnlockPresentationStages meCarUnlockPresentationStage;// +0x223C (h:442)
        CgsID                        mCarUnlockId;                // +0x2240 (h:443)
        f32   mfCarUnlockPresentationTimeRemaining;               // +0x2248 (h:444)
        s32                          miCurrentCarUnlockIndex;     // +0x224C (h:445)
        ENewRivalsPresentationStages meNewRivalsPresentationStage;// +0x2250 (h:447)
        CgsID                        mPendingRivalId;             // +0x2258 (h:448)
        f32   mfRivalPresentationTimeRemaining;                   // +0x2260 (h:449)
        EFreeCarPresentationStages   meFreeCarPresentationStages; // +0x2264 (h:452)
        CgsID                        mPendingFreeCarId;           // +0x2268 (h:453)
        f32   mfFreeCarPresentationTimeRemaining;                 // +0x2270 (h:454)
        GuiEventOfflinePostEvent::OfflinePostEventData mResults;  // +0x2278 (h:458), 192 bytes
        const char*                  mpcAnimatingComponentName;   // +0x2338 (h:460)
        f32                          mfTimeRemaining;             // +0x233C (h:462, last member)
    };
}
