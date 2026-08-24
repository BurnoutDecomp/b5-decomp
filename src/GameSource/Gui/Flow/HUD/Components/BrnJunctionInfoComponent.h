#ifndef BRN_GUI_JUNCTION_INFO_COMPONENT_H
#define BRN_GUI_JUNCTION_INFO_COMPONENT_H

// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnJunctionInfoComponent.h
//
//   BrnGui::JunctionInfoComponent -- the in-race "junction info" HUD panel that
//   appears at a junction/event start: the game-mode icon, medal state, event-name
//   text (one- or two-line), and the two "start hint" controller-button prompts.
//   Derives from BrnFlaptComponent.
//
// Class shape, nested typedefs, member NAMES + ORDER and the KAC_/KAPC_ statics are
// verbatim from the DecFIGS DWARF (BrnJunctionInfoComponent.h). Byte offsets below are
// X360-attested by Construct @0x82423DE0 / Prepare @0x8242BCC0 / HandleJunctionChange
// @0x824400B8 / SetupAptVariables @0x824398A0 / SetEventNameText @0x82414E60; the
// FlaptAnimatorComponent stride 0x38, FlaptButtonIconComponent stride 0x18 and
// TextFieldRef stride 0x0C are attested by the committed sibling headers.
//     +0x00  base BrnFlaptComponent (mpStateInterface @+0x00, mAptRef @+0x04)  0x0C
//     +0x0C  mGameModeIconAnimator    (FlaptAnimatorComponent, 0x38)
//     +0x44  mMedalAnimator           (FlaptAnimatorComponent, 0x38)
//     +0x7C  mEventNameTextfield      (TextFieldRef, 0x0C)
//     +0x88  mEventNameTextfield2Line (TextFieldRef, 0x0C)
//     +0x94  mStartHintAnimator       (FlaptAnimatorComponent, 0x38)
//     +0xCC  mStartHintButton1        (FlaptButtonIconComponent, 0x18)
//     +0xE4  mStartHintButton2        (FlaptButtonIconComponent, 0x18)
//     +0x100 mJunctionInfo            (GuiEventJunctionInfo, 0x20)   [pad 0xFC..0xFF]
//     +0x120 mbInJunction  +0x121 mbShowingStartHint  +0x122 mbShowing2LineName
//     +0x123 mbGameComplete
//     +0x128 mCurrentCarId            (CgsID, 8)   [4-byte pad at +0x124]
//
// GuiEventJunctionInfo is the canonical GUI-event payload homed in
// GameSource/Gui/BrnGuiEventTypeDefs.h (DWARF: struct GuiEventJunctionInfo : public
// GuiEvent<309>); included, not duplicated.
//
// WAVE58 SLICE: the two data-driven methods SetupAptVariables (@0x824398A0) and
// SetEventNameText (@0x82414E60) are DECLARED but bodied by a later slice -- their X360
// bodies index the per-gamemode static tables KAPC_GAMEMODE_ICON_FRAMENAMES /
// gGameModeNameStringIds whose full contents are NOT in the function export (only entries
// 0/5 attested), and SetupAptVariables also constructs GuiEventTickerCustomMessage whose
// exact size the ledger does not reconcile. Those statics are likewise declaration-only
// here (defined by the class's data TU when the tables are fully recovered). Every other
// method below is bodied in BrnJunctionInfoComponent.cpp. All member access is BY NAME.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                                        // CgsID
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                                     // BrnGui::GuiEventJunctionInfo (canonical home)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                             // BrnFlapt::TextFieldRef
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                                  // BrnFlapt::FileRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"       // BrnFlaptComponent (base) + MovieClipRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"   // FlaptAnimatorComponent
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnFlaptButtonIcon.h"         // FlaptButtonIconComponent

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    class JunctionInfoComponent : public BrnFlaptComponent
    {
    public:
        // DWARF :51/:53/:52 nested typedefs.
        typedef BrnGui::FlaptAnimatorComponent   JunctionInfoAnimatorComponent;
        typedef BrnFlapt::TextFieldRef           JunctionInfoTextComponent;
        typedef FlaptButtonIconComponent         JunctionInfoButtonIconComponent;

        // ---- methods bodied by this slice ----------------------------------
        void Construct(const char* lacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lacParentName, s32 liParentAptLayer);           // @0x82423DE0
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);          // @0x8242BCC0
        void HandleJunctionChange(const GuiEventJunctionInfo* lpEvent,
                                  CgsID lCurrentCarId);                             // @0x824400B8
        void Refresh(const char* lpComponentName);                                 // @0x82414D58
        void Run(const char* lpcAnimation);                                        // @0x82423F40

        // ---- methods bodied by sibling TUs (declared for a coherent class) ---
        void HandleTransitionComplete(const char* lpcComponentName, s32 liUniqueId);
        bool IsShowingStartHint();
        void SetGameComplete(bool lbComplete);

    private:
        // GetMedalFrameNameFromMedal bodied by this slice.
        const char* GetMedalFrameNameFromMedal(s8 li8Medal);                       // @0x82414DA0
        void TransitionInMainClip();                                               // @0x82414FD8 (this slice)
        void TransitionOutMainClip();                                              // @0x82415030 (this slice)
        void SetupAptVariables();                                                  // @0x824398A0 (bodied 2026-08-25; tables recovered from the image)
        void SetEventNameText();                                                   // @0x82414E60

        // ---- member layout (DWARF :124-:155 order; offsets above) -----------
        JunctionInfoAnimatorComponent   mGameModeIconAnimator;       // +0x0C
        JunctionInfoAnimatorComponent   mMedalAnimator;              // +0x44
        JunctionInfoTextComponent       mEventNameTextfield;         // +0x7C
        JunctionInfoTextComponent       mEventNameTextfield2Line;    // +0x88
        JunctionInfoAnimatorComponent   mStartHintAnimator;          // +0x94
        JunctionInfoButtonIconComponent mStartHintButton1;           // +0xCC
        JunctionInfoButtonIconComponent mStartHintButton2;           // +0xE4
        GuiEventJunctionInfo            mJunctionInfo;                // +0x100
        bool                            mbInJunction;                // +0x120
        bool                            mbShowingStartHint;          // +0x121
        bool                            mbShowing2LineName;          // +0x122
        bool                            mbGameComplete;              // +0x123
        CgsID                           mCurrentCarId;               // +0x128

        // DWARF statics (:125-:146) -- per-gamemode apt frame-name tables. DEFINED in
        // BrnJunctionInfoComponent.cpp since 2026-08-25: the wave58 "serialised label data
        // not in the function export" park was recoverable-after-all -- the 11 pointers at
        // 0x82F24ECC read cleanly off the image (headless idat; scratch h1_dump2.txt).
        static const char* const KAPC_GAMEMODE_ICON_FRAMENAMES[11];
        static const char* const KAC_CURRENT_BURNING_ROUTE_ICON_FRAMENAME;
    };
}

#endif // BRN_GUI_JUNCTION_INFO_COMPONENT_H
