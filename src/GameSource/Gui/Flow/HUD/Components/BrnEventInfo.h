// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnEventInfo.h
//
//   BrnGui::EventInfoComponent -- the in-race HUD "event info" panel (position /
//   medal text-state, score/banking, road-rage takedown digits, timers, distance
//   interpolator, etc). Derives from BrnFlaptComponent.
//
// Class shape, member NAMES + ORDER, nested typedefs and the KI_ statics are
// verbatim from the DecFIGS DWARF (BrnEventInfo.h). This header is authored from the
// two X360 functions this batch homes; only the members those functions touch are
// pinned to attested byte offsets:
//   mTextStateAnimatorRace   X360 this+0x0F8   (SetPositionTextState animator)
//   mTextStateAnimatorRRage  X360 this+0x130   (+0x38 FlaptAnimatorComponent stride)
//   mDistanceAnimatorRace    X360 this+0x168   (SetTakedownsDigitsState animator;
//                                                +0x38 stride past mTextStateAnimatorRRage)
//   miCurrentTakedowns       X360 this+0x400   (lwz 0x400(this) gate)
// The remaining members are declared in DWARF order for a coherent layout; their
// absolute offsets are not independently attested by this batch. FlaptAnimatorComponent
// sizeof (0x38) is attested by the committed BrnGuiFlaptIconComponent.h.
//
// NOTE: SetTakedownsDigitsState is NOT in the (PS3-derived) DWARF method list (which
// instead carries the distinct SetTakedownsTextState StrStream method); it is homed
// here from the X360 ledger under its IDA name.
// ============================================================================
#ifndef BRN_GUI_EVENT_INFO_COMPONENT_H
#define BRN_GUI_EVENT_INFO_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                        // CgsID
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "GameSource/GameState/BrnGameStateSharedIO.h"                             // BrnGameState::GameStateModuleIO::EGameModeType, ECurrentMedalTargetTime
#include "GameSource/GameState/BrnGameStateTypes.h"                                // BrnGameState::LandmarkIndex
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                             // BrnFlapt::TextFieldRef
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                                  // BrnFlapt::FileRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"       // BrnFlaptComponent (base) + MovieClipRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"   // FlaptAnimatorComponent
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptInterpolatorComponent.h" // FlaptInterpolatorComponent

namespace BrnGui
{
    class GuiCache;   // pointer-held

    // DWARF BrnEventInfo.h:70.
    class EventInfoComponent : public BrnFlaptComponent
    {
    public:
        // DWARF :50/:49/:51 nested typedefs.
        typedef BrnFlapt::TextFieldRef          TextFieldComponentType;
        typedef BrnGui::FlaptAnimatorComponent  AnimatorComponentType;
        typedef FlaptInterpolatorComponent      InterpolatorComponentType;

    private:
        // ---- statics (DWARF :127/:197) --------------------------------------
        static const s32 KI_TEXTFIELD_COUNT             = 6;    // :127
        static const s32 KI_POSITION_WARNING_THRESHOLD  = 4;    // :197

        // ---- methods this TU homes ------------------------------------------
        // @0x8241EDF8 -- set the position/medal text-state (Gold/Normal/Warning). DWARF :373.
        void SetPositionTextState(s32 liPosition);
        // @0x824218A8 -- set the road-rage takedown digit layout (Single/Multiple).
        // X360-only (not in DWARF method list).
        void SetTakedownsDigitsState();

        // ---- member layout (DWARF :135-:230 order) --------------------------
        BrnFlapt::MovieClipRef mEventMovieClip;              // :135
        const char*            mpEventInfoComponentName;     // :136
        BrnFlapt::FileRef      mHUDFileRef;                  // :137
        TextFieldComponentType maTextField[6];               // :140
        TextFieldComponentType mAddScoreTextField;           // :145
        TextFieldComponentType mBankScoreTextField;          // :146
        AnimatorComponentType  mAddScoreAnimator;            // :147
        AnimatorComponentType  mScoreBackgroundAnimator;     // :149
        AnimatorComponentType  mTextStateAnimatorRace;       // :150 (X360 +0x0F8)
        AnimatorComponentType  mTextStateAnimatorRRage;      // :151 (X360 +0x130)
        AnimatorComponentType  mDistanceAnimatorRace;        // :152 (X360 +0x168)
        AnimatorComponentType  mTimeAnimatorRRage;           // :153
        AnimatorComponentType  mTimeAnimatorBRoute;          // :154
        AnimatorComponentType  mTimeAnimatorStuntRun;        // :155
        AnimatorComponentType  mScoreAnimator;               // :156
        AnimatorComponentType  mBankingAnimator;             // :157
        AnimatorComponentType  mStuntAnimator;               // :158
        AnimatorComponentType  mMultiplierAnimator;          // :159
        InterpolatorComponentType mDistanceInterpolator;     // :161
        f32                    mfDistanceWarningThreshold;   // :199
        BrnGameState::GameStateModuleIO::EGameModeType meCurrentEventType;      // :205
        BrnGameState::GameStateModuleIO::EGameModeType meCurrentOnlineGameMode; // :206
        BrnGameState::ECurrentMedalTargetTime meCurrentTargetMedal;            // :208
        f32                    mfCurrentTimeSinceLastPositionChange; // :210
        bool                   mbTimeRemainingFlashing;      // :212
        s32                    miCurrentPosition;            // :215
        s32                    miTotalRacers;                // :216
        BrnGameState::LandmarkIndex mCurrentLandmark;        // :217
        f32                    mfDistToCheckpoint;           // :218
        bool                   mbNearFinish;                 // :219
        f32                    mfTimeLeft;                   // :222
        s32                    miCurrentTakedowns;           // :223 (X360 +0x400)
        s32                    miTargetTakedowns;            // :224
        CgsID                  mRivalCarID;                  // :227
        s32                    miTargetDamageCount;          // :228
        s32                    miCurrentDamageCount;         // :229
        f32                    mfRivalDistanceFromTarget;    // :230
    };
}

#endif // BRN_GUI_EVENT_INFO_COMPONENT_H
