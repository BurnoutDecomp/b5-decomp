#pragma once

// ===================================================================================
// BrnGui::Intro  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnIntro.h
//
// The SCREEN flow's first-boot intro state (SCREEN state id "INTRO", numeric 128 in
// BRNSCREENFSM). Entered from INGAME on "TO_INTRO", which BrnGui::InGame::Update fires
// while BrnProgression::Profile::mbIsNewProfile is set (GUI event 288 can also fire it).
// It runs the welcome-text -> photo-booth -> driver-licence presentation, then opens the
// free camera's fly-by window, then leaves to car-select ("ADVANCE") or back ("GO_BACK").
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Intro (ctor)                      @0x824FFF58
//   Construct                         @0x824B8FE8   OnEnter     @0x824D1488
//   OnLeave                           @0x824D1640   Update      @0x824DF0B0
//   GetResourcesToLoad                = the ICF fold @0x825151A8 (vtable @0x820740E0 slot 8)
//   HandleIncomingEvents              @0x824C1F68   HandleStateTransitions   @0x824DAA48
//   SetupComponents                   @0x824D1718   AppendExpectedComponents @0x824B90A0
//   HandleControllerInput             @0x824D1988   (vtable slot 9)
//   HandleControllerPressedPhotoBooth @0x824D1B98   (vtable slot 10)
//   HandleTransitionFromWelcomeText   @0x824D1B00   HandleTransitionFromLicense @0x824D1C18
//
// CLASS SHAPE / member names / member order verbatim from the DecFIGS DWARF
// (BrnIntro.h:47..:190), gated on the X360 ledger. Guest byte offsets below are the
// 32-bit-pointer ABI offsets the asm proves; the gate compiles 64-bit so every member is
// reached BY NAME and the x64 sizeof legitimately drifts:
//   +0x38 meIntroState   +0x3C mpProfile   +0x40 mpGuiCache
//   +0x44 mScreenAnim (AnimationComponent, guest 0x8C)
//   +0xD0 mLicenseComponent (guest 0x7CC)   +0x89C mPhotoBoothComponent (guest 0x434)
//   +0xCD0 mWelcomeText1Anim   +0xD5C mWelcomeText2Anim
//   +0xDE8 mfFlybyTimer   +0xDEC mfPauseTimer   +0xDF0 mbVoiceOverPlaying
//   +0xDF4 mMonthTextField   +0xF1C mDayTextField   +0x1044 mYearTextField (TextField 0x128)
// (The three AnimationComponent strides here are one of the four independent proofs that
// BrnGui::AnimationComponent adds no members over CgsGui::GuiComponent -- see
// GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h.)
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                   // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"    // CgsGui::sResourceTuple
#include "GameSource/Gui/BrnGuiTextField.h"                                       // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"          // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.h"            // BrnGui::LicenseComponent (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnPhotoBoothComponent.h"         // BrnGui::PhotoBoothComponent (by value)

namespace CgsModule { struct Event; }
namespace BrnProgression { class Profile; }   // GameSource/GameState/Progression/BrnProfile.h

namespace BrnGui
{
    class GuiCache;   // GameSource/Gui/BrnGuiCache.h (held by pointer only)

    struct Intro : public CgsGui::State
    {
        // DWARF BrnIntro.h:78. Only 0 and 3..9 are live in the ARTIST build: Update's jump
        // table @0x824DF0F0 routes cases 1 and 2 to the "Unhandled IntroState" assert, and no
        // store of the constant 1 to meIntroState exists anywhere in the image -- so
        // PLAYING_VIDEO is unreachable, and WAITING_FOR_VIDEO (written only by
        // HandleControllerInput's case-1 "skip the intro video" branch) is unreachable with
        // it. The intro-video path was removed from Update while its vestiges survived in
        // HandleControllerInput and in the event-510 subscription; both are reproduced.
        enum EIntroState
        {
            E_INTROSTATE_NONE                  = 0,
            E_INTROSTATE_PLAYING_VIDEO         = 1,   // dead in this build (see above)
            E_INTROSTATE_WAITING_FOR_VIDEO     = 2,   // dead in this build (see above)
            E_INTROSTATE_START_FLYBY           = 3,
            E_INTROSTATE_WAIT_FOR_FLYBY_FINISH = 4,
            E_INTROSTATE_LOADINGRESOURCES      = 5,
            E_INTROSTATE_WAITINGFORCOMPONENTS  = 6,
            E_INTROSTATE_WELCOMETEXT           = 7,
            E_INTROSTATE_PHOTOBOOTH            = 8,
            E_INTROSTATE_LICENCE               = 9,
            E_INTROSTATE_COUNT                 = 10,
        };

        // @ 0x824FFF58 -- construct the intro state and its embedded component widgets.
        Intro();

        virtual void Construct(CgsID lId, CgsFsm::ScriptedFsm* lpFsm);   // @0x824B8FE8
        virtual void OnEnter();                                          // @0x824D1488
        virtual void OnLeave();                                          // @0x824D1640
        virtual void Update();                                           // @0x824DF0B0

        // vtable slot 8 == the ICF fold @0x825151A8 (shared with
        // BrnGui::CompletedGame::GetResourcesToLoad): `li r11,0; stw r11,0(r4);
        // stw r11,0(r5); blr` -- the state hands the generic loader an EMPTY list and drives
        // maResourcesToLoad itself through the GuiCache in Update case 5.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

        // vtable slots 9 / 10. Virtual on X360: HandleIncomingEvents dispatches slot 9 for
        // event 6, and HandleControllerInput dispatches slot 10 for the PHOTOBOOTH state.
        virtual void HandleControllerInput(const CgsModule::Event* lpEvent, s32 leEventType);
        virtual bool HandleControllerPressedPhotoBooth(const CgsModule::Event* lpEvent);

    private:
        void HandleIncomingEvents();              // @0x824C1F68
        void HandleStateTransitions();            // @0x824DAA48
        void SetupComponents();                   // @0x824D1718
        void AppendExpectedComponents();          // @0x824B90A0
        void HandleTransitionFromWelcomeText();   // @0x824D1B00
        void HandleTransitionFromLicense();       // @0x824D1C18

        // ---- statics (DWARF BrnIntro.cpp:32..:91; values read from the X360 image) ----
        static const CgsGui::sResourceTuple maResourcesToLoad[2];   // @0x82F272E0
        static const u32 muNumResourcesToLoad;                      // == 2
        static const s32 maiEventToObserve[8];                      // @0x8206670C
        static const s32 miNumEventsObserved;                       // == 8

        static const char KAC_SCREENANIM_NAME[15];
        static const char* const KAPC_SCREEN_FRAMENAMES[E_INTROSTATE_COUNT];  // @0x82F272F0
        static const char KAC_LICENSECOMP_NAME[12];
        static const char KAC_PHOTOBOOTHCOMP_NAME[15];
        static const char KAC_BUTTONPROMPTCOMP_NAME[10];
        static const char KAC_WELCOMETEXT1COMP_NAME[18];
        static const char KAC_WELCOMETEXT2COMP_NAME[18];
        static const char KAC_GOTOPHOTOBOOTH_STRINGID[25];
        static const char KAC_CONTINUE_STRINGID[25];
        static const char KAC_MONTHTEXTFIELD_NAME[14];
        static const char KAC_DAYTEXTFIELD_NAME[12];
        static const char KAC_YEARTEXTFIELD_NAME[13];

        // ---- members (DWARF order; guest offsets in the header note above) ----
        EIntroState              meIntroState;          // +0x38
        BrnProgression::Profile* mpProfile;             // +0x3C   (GUI event 350)
        GuiCache*                mpGuiCache;            // +0x40   (GUI event 64)
        AnimationComponent       mScreenAnim;           // +0x44   "ScreenAnim_cpt"
        LicenseComponent         mLicenseComponent;     // +0xD0   "License_cpt"
        PhotoBoothComponent      mPhotoBoothComponent;  // +0x89C  "PhotoBooth_cpt"
        AnimationComponent       mWelcomeText1Anim;     // +0xCD0  "WelcomeText1_anim"
        AnimationComponent       mWelcomeText2Anim;     // +0xD5C  "WelcomeText2_anim"
        f32                      mfFlybyTimer;          // +0xDE8
        f32                      mfPauseTimer;          // +0xDEC
        bool                     mbVoiceOverPlaying;    // +0xDF0  (GUI events 466 / 467)
        TextField                mMonthTextField;       // +0xDF4  "MonthText_cpt"
        TextField                mDayTextField;         // +0xF1C  "DayText_cpt"
        TextField                mYearTextField;        // +0x1044 "YearText_cpt"
    };
}
