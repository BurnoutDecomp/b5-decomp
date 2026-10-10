#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                  // Vector2
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                        // CgsNumeric::Random
#include "GameSource/Gui/BrnGuiTextField.h"                                  // BrnGui::TextField
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavBorough.h"        // BrnGui::CrashNavBorough
#include "GameSource/Gui/Flow/Screen/Components/BrnCursor.h"                 // BrnGui::GuiCursor
#include "GameSource/Gui/Flow/Screen/Components/BrnSelectRoutes.h"           // BrnGui::SelectRoutes
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"     // BrnGui::AnimationComponent
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpBar.h"                // BrnGui::HelpBar
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"          // BrnGui::MenuComponent
#include "GameSource/Gui/SatNav/BrnMainMap.h"                                // BrnGui::MainMapComponent
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                         // BrnGui::MapIconManager::OwnerId

// BrnGui::OnlineSelectRoute - the online route-selection screen state.
//
// Class shape and member order are the debug-info's (BrnOnlineSelectRoute.h), checked
// against the console constructor and OnEnter, which names
// every embedded component. The console carries six animators where the debug-info lists three:
// the three extra ones ("infobackground_anim", "distance_anim", "time_anim") sit between
// the cursor and event-name animators and are named here after their apt components.
// Members are reached by name; the console offsets in the comments are documentary.
namespace BrnGui
{
    class GuiCache;

    struct OnlineSelectRoute : public CgsGui::State
    {
        // debug-info BrnOnlineSelectRoute.h (sub-state machine).
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN         = 0,
            E_SUBSTATE_LOADING_COMPONENTS     = 1,
            E_SUBSTATE_SELECTING_MAIN_OPTION  = 2,
            E_SUBSTATE_SELECTING_CHECKPOINT   = 3,
            E_SUBSTATE_SELECTING_POINT        = 4,
            E_SUBSTATE_SELECTING_PRESET_EVENT = 5,
            E_SUBSTATE_WAIT_IN_GAME           = 6,
            E_SUBSTATE_LEAVING_STATE          = 7,
            E_SUBSTATE_COUNT                  = 8,
        };

        // compiler-emitted: constructs the members below and nothing else.
        OnlineSelectRoute();

        // @ 0x8251AEF8 - hands the route-select screen's static resource list to the loader
        // (X360: *r4 = &maResourceTuplesToLoad; *r5 = miNumResourcesToLoad, count = 2).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

    private:
        // Pop the "route too short" overlay with the current route length and the minimum.
        // Body in the .cpp.
        void DisplayTooShortRouteMessage();

        static const CgsGui::sResourceTuple maResourceTuplesToLoad[]; // (.rdata)
        static const s32                    miNumResourcesToLoad;     // (.rdata) == 2

        // ---- data members (debug-info order; console offsets documentary) --------------------
        SelectRoutes            mSelectRoutes;            // +0x0038 "SelectRoutes"
        MenuComponent           mMenuOptions;             // +0x1978 "MenuItem"
        TextField               mCurrentRoundDisplay;     // +0x2A38 "CurrentRoundTxt"
        TextField               mEventNameValue;          // +0x2B60 "EventValue"
        TextField               mTimeValue;               // +0x2C88 "TimeValue"
        TextField               mDistanceValue;           // +0x2DB0 "DistanceValue"
        TextField               mTitleText;               // +0x2ED8 "TitleTxt"
        AnimationComponent      mCheckpointMenuAnimator;  // +0x3000 "CheckpointMenu_anim"
        AnimationComponent      mCursorAnimator;          // +0x308C "cursor_anim"
        AnimationComponent      mInfoBackgroundAnimator;  // +0x3118 "infobackground_anim" (FLAG name)
        AnimationComponent      mDistanceAnimator;        // +0x31A4 "distance_anim" (FLAG name)
        AnimationComponent      mTimeAnimator;            // +0x3230 "time_anim" (FLAG name)
        AnimationComponent      mEventNameAnimator;       // +0x32BC "event_name_anim"
        HelpBar                 mHelpBar;                 // +0x3350 "Button"
        GuiCache*               mpGuiCache;               // +0x51A0
        CgsNumeric::Random      mRandom;                  // +0x51B0
        MapIconManager*         mpIconManager;            // +0x51E0
        MapIconManager::OwnerId mIconManagerOwnerId;      // +0x51E4
        MainMapComponent        mMainMapComponent;        // +0x51F0
        Vector2                 mv2WorldCenterPoint;      // +0x5870
        CrashNavBorough         mCrashNavBorough;         // +0x5880 "Borough_mc"
        GuiCursor               mCursor;                  // +0x5910 "cursor_mc"
        ESubState               meSubState;               // +0x5A00
        u32                     muSavedCheckpointId;      // +0x5A04
        u32                     muCurrentPresetEventID;   // +0x5A08
        bool                    mbWaitingForRoute;        // +0x5A0C
        bool                    mbEditingNewCheckpoint;   // +0x5A0D
        bool                    mbRouteEdited;            // +0x5A0E
    };
}
