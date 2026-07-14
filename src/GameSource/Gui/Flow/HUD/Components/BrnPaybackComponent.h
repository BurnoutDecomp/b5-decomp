#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base) + StateInterface tag
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                 // CgsNumeric::Random
#include "GameSource/BurnoutConstants.h"                              // EActiveRaceCarIndex
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"           // BrnNetwork::EPaybackType
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"        // BrnGui::HelpItem (embedded)

// BrnGui::PaybackComponent -- the in-race "payback available" HUD widget. It teases a
// randomised carousel of dirty-trick icons, settles on the trick that was actually
// awarded, then slides to the side and posts an availability event. A CgsGui::GuiComponent
// subclass (one vptr) that drives its Flash/apt movie exclusively through the base
// AddOutputAptViewState channel plus one direct OutputGuiEvent post.
//
// Shape (enums / member names / method set) is verbatim from the DecFIGS DWARF
// (BrnPaybackComponent.h). Member byte offsets are X360-attested from the ARTIST asm
// (Construct @0x8242E3A8, TriggerAptAnimation @0x82411758, RespondToTransitionComplete
// @0x8243DEB0): mpGuiCache @+0x8C, mRandom @+0x90, mbFirstHalfOfRotationAnim @+0xC0,
// mbDelayingIcon @+0xC1, mfCurrentSysTime_Seconds @+0xC4, mfTimeStampOfMessageEnd @+0xC8,
// muNumRandomLeftToShow @+0xCC, mePaybackComponentState @+0xD0, mePaybackAnimationState
// @+0xD4, mePreviouslyShownPayback @+0xD8, meActualPayback @+0xDC, meVictimRaceCarIndex
// @+0xE0, mHelpItem @+0xE4.

namespace BrnGui
{
    class GuiCache;   // pointer-held (BrnGuiCache.h); full type only needed by the .cpp

    struct PaybackComponent : public CgsGui::GuiComponent
    {
        // DWARF BrnPaybackComponent.h:88.
        enum EAwardTypes
        {
            E_AT_START        = 0,
            E_AT_SLASHDOWN    = 0,
            E_AT_LOCKDOWN     = 1,
            E_AT_TAKEOVERDOWN = 2,
            E_AT_EVIL_AXIS    = 3,
            E_AT_COUNT        = 4,
        };

        // DWARF BrnPaybackComponent.h:103.
        enum EPaybackComponentAnimations
        {
            E_PCA_INVISIBLE   = 0,
            E_PCA_ROTATE_IN   = 1,
            E_PCA_ROTATE_OUT  = 2,
            E_PCA_BOUNCE      = 3,
            E_PCA_WITHBIGTEXT = 4,
            E_PCA_GO_TO_SIDE  = 5,
            E_PCA_ATSIDE      = 6,
        };

        // DWARF BrnPaybackComponent.h:115.
        enum EPaybackComponentStates
        {
            E_PCS_INVISIBLE     = 0,
            E_PCS_RANDOMICONS   = 1,
            E_PCS_PROPERICON    = 2,
            E_PCS_ANIMATINGLEFT = 3,
            E_PCS_AVAILABLE     = 4,
            E_PCS_COUNT         = 5,
        };

        // ---- public surface (DWARF :70-:180) ----
        // @0x8242E3A8 -- base Construct, construct the embedded help item ("PaybackButton"),
        // prime + re-seed the icon RNG, and reset the payback FSM.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        void Initialize(GuiCache* lpGuiCache);                                        // @0x82428DF0
        void Update(f32 lfCurrentSysTime_Seconds);                                    // @0x8241FF38
        void BeginAwardAnimation(BrnNetwork::EPaybackType lePaybackType,              // @0x8243E148
                                 ::EActiveRaceCarIndex leVictimRaceCarIndex);
        void ShowAvailableInstantly(BrnNetwork::EPaybackType lePaybackType,           // @0x8241FF40
                                    ::EActiveRaceCarIndex leVictimRaceCarIndex);
        void BecomeInvisible();                                                       // @0x8241FFE8
        void RespondToTransitionComplete();                                           // @0x8243DEB0

    private:
        // ---- private helpers ----
        // Declared-only (not part of this TU's ledger set): bodies land with their own
        // functions. Kept in the class surface so the shape stays complete.
        void EnterState(EPaybackComponentStates lePaybackComponentState);            // :117 (other TU)
        void SendAwardTriggerableEvent();                                            // :491 (CgsGuiEvent.h path)

        void UpdateState();                                                          // @0x8241FE98
        void SetDisplayedIcon(EAwardTypes leAwardType);                              // @0x824116C8
        void TriggerAptAnimation(EPaybackComponentAnimations lePaybackComponentAnimation); // @0x82411758
        s32  ChooseRandomAward();                                                    // @0x824119B0

        // ---- member layout (DWARF order; X360 offsets in the file header) ----
        GuiCache*                    mpGuiCache;                 // +0x8C
        CgsNumeric::Random           mRandom;                    // +0x90 (48B, 8-aligned)
        bool                         mbFirstHalfOfRotationAnim;  // +0xC0
        bool                         mbDelayingIcon;             // +0xC1
        f32                          mfCurrentSysTime_Seconds;   // +0xC4
        f32                          mfTimeStampOfMessageEnd;    // +0xC8
        u32                          muNumRandomLeftToShow;      // +0xCC
        EPaybackComponentStates      mePaybackComponentState;    // +0xD0
        EPaybackComponentAnimations  mePaybackAnimationState;    // +0xD4
        EAwardTypes                  mePreviouslyShownPayback;   // +0xD8
        EAwardTypes                  meActualPayback;            // +0xDC
        ::EActiveRaceCarIndex        meVictimRaceCarIndex;       // +0xE0
        BrnGui::HelpItem             mHelpItem;                  // +0xE4
    };
}
