#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiCache.h"          // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"  // GuiEventOfflinePostEvent::OfflinePostEventData
#include "GameSource/Gui/BrnGuiTextField.h"      // BrnGui::TextField
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"

// Pointer-only parameter of HandleAptTriggers (its real home is
// GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h, which the .cpp
// includes). Same treatment as BrnOfflineInstantResults.h / BrnPreRaceFlyBy.h.
namespace CgsGui { struct GuiEventAptTriggerPayload; }

// ==========================================================================================
// BrnGui::ShowtimeInstantResultsState -- the post-event SHOWTIME instant-results
// presentation flow state. This is the screen a finished showtime session lands on, and it
// is where the session's damage total, distance and score multiplier get counted up.
//
// ⭐⭐ THIS HEADER USED TO BE FOUR NAMED MEMBERS BURIED IN 0xB6C BYTES OF `mPad*`, AND ONLY
// THREE OF THE CLASS'S NINETEEN FUNCTIONS EXISTED. OnEnter / OnLeave / Update were LOGGING
// STUBS in BrnScreenStatesDataLinkStubs.cpp, so a finished showtime session terminated
// correctly and then drew nothing at all -- the terminator handed over to a state that had
// no body. That is the gap this header exists to close.
//
// HOW THE MEMBER LIST WAS RECOVERED (asm, not inference). The class has no exported
// constructor in the ARTIST set, so the offline sibling's "read the ctor's vtable stores"
// trick was unavailable. Instead every slot below is pinned by a load or a store in a
// function this TU reconstructs, and the run closes with ZERO unexplained bytes:
//     +0x0038  mHelpItems[3]      OnEnter's construct loop: base r31+0x38, stride 0x1AC,
//                                 three iterations (`addi r28,r28,0x1AC` vs &unk_82F26BE0)
//     +0x053C  mFinishedText      OnEnter `addi r3, r31, 0x53C` + "Finished"     (0x128)
//     +0x0664  mTotalScoreText    OnEnter `addi r3, r31, 0x664` + "TargetResult" (0x128)
//     +0x078C  mMultiplierText    OnEnter `addi r3, r31, 0x78C` + "ShowtimeMult" (0x128)
//     +0x08B4  mMultSymbolText    OnEnter `addi r3, r31, 0x8B4` + "ShowtimeEx"   (0x128)
//     +0x09DC  mResultsIcon       OnEnter IconComponent::Construct(r31+0x9DC, "Medal") (0x94)
//     +0x0A70  mpGuiCache         the "mpGuiCache" assert's operand, everywhere
// 0x9DC + 0x94 == 0xA70 EXACTLY, so the component block is closed on both sides by the asm;
// there is no room for an unnamed member and none is needed. The scalar tail is closed the
// same way: mResults is 192 bytes at +0xA88 and 0xA88 + 0xC0 == 0xB48 == mfTimeRemaining
// (`stfs f0, 0xB48`), after which eight consecutive words run to miCurrentMultiplier at
// +0xB68 (`stw r30, 0xB68` in OnEnter, the last byte of the class).
//
// ⚠️ WHY AppendExpectedComponents' OFFSETS LOOK FOUR BYTES HIGHER. It registers each
// component as `cache->AppendExpectedAptComponent(flow, component + 4)` -- 0x540 / 0x668 /
// 0x790 / 0x8B8 / 0x9E0 and helpItem base 0x3C. Those are not different members: +4 is the
// GuiComponent base's `macName` buffer, i.e. GetName(). The previous revision of this header
// read that shift as the members' own offsets and placed mMultiplierText at +0x78C-4; the
// OnEnter Construct calls settle it, because they pass the component ITSELF.
//
// The X360 byte offsets in the comments are DOCUMENTARY: the CgsGui::State base and every
// component widen on the x64 gate, so members are reached BY NAME. What the offsets pin is
// the ORDER, and the order is what the asm proves.
// Layout/virtual shape corroborated by the DecFIGS DWARF (BrnShowtimeInstantResults.h).
// ==========================================================================================
namespace BrnGui
{
    struct ShowtimeInstantResultsState : public CgsGui::State
    {
        // ---- enums (DWARF BrnShowtimeInstantResults.h) -----------------------------------
        // DWARF h:75. Update @0x824DFB48 switches on meCurrentState after `addi r11,r11,1`
        // against 4 -- i.e. exactly the five values -1..3 below.
        enum EResultsInternalStates
        {
            E_RESULTS_STATE_INVALID            = -1,
            E_RESULTS_STATE_UNLOADED           = 0,
            E_RESULTS_STATE_LOADING_RESOURCES  = 1,
            E_RESULTS_STATE_LOADING_COMPONENTS = 2,
            E_RESULTS_STATE_ACTIVE             = 3,
            E_RESULTS_STATE_COUNT              = 4,
        };

        // DWARF h:87 -- which event sub-state is showing. This screen has only one real
        // page, so the list is much shorter than the offline sibling's ten.
        enum EResultsActiveSubStates
        {
            E_ACTIVE_SUBSTATE_EVENT_NONE    = -1,
            E_ACTIVE_SUBSTATE_EVENT_RESULTS =  0,
            E_ACTIVE_SUBSTATE_EVENT_DONE    =  1,
            E_ACTIVE_SUBSTATE_EVENT_COUNT   =  2,
        };

        // DWARF h:97 -- the phase within the active sub-state. ResetStateTimer primes
        // mfTimeRemaining from the KF_*_DURATION matching this.
        enum EResultsSubStateStates
        {
            E_SUBSTATE_INVALID           = -1,
            E_SUBSTATE_SET_UP_COMPONENTS = 0,
            E_SUBSTATE_TOTALLING         = 1,
            E_SUBSTATE_SUMMARY           = 2,
            E_SUBSTATE_LEAVING           = 3,
            E_SUBSTATE_COUNT             = 4,
        };

        // DWARF h:108 -- the count-up ladder inside E_SUBSTATE_TOTALLING. UpdateScoreTotalling
        // switches on `meTotallingStage - 1` against 4, i.e. the five live values 1..5.
        enum EResultsTotallingSubstates
        {
            E_TOTALLING_SUBSTATE_INVALID                  = 0,
            E_TOTALLING_SUBSTATE_SHOWING_DISTANCE_METRES  = 1,
            E_TOTALLING_SUBSTATE_SHOWING_DISTANCE_DOLLARS = 2,
            E_TOTALLING_SUBSTATE_BANKING_DISTANCE         = 3,
            E_TOTALLING_SUBSTATE_SHOWING_MULTIPLICATION   = 4,
            E_TOTALLING_SUBSTATE_BANKING_SCORE            = 5,
            E_TOTALLING_COUNT                             = 6,
        };

        static const s32 KI_HELPITEMS = 3;         // DWARF h:130

        // Per-sub-state timer durations (X360 .rdata; defined in the .cpp, cpp:33/34/35).
        static const f32 KF_TOTALLING_DURATION;   // 10.0f  flt_82065B68
        static const f32 KF_SUMMARY_DURATION;     //  3.0f  flt_82065654
        static const f32 KF_TRANS_OUT_DURATION;   //  1.0f  flt_82001C98

        virtual void OnEnter();                    // @0x824C5D28 (cpp:112)
        virtual void OnLeave();                    // @0x824C5FD8 (cpp:183)
        virtual void Update();                     // @0x824DFB48 (cpp:223)

        // @0x82500930 -- hand the Showtime instant-results state's static resource list to
        // the loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // ---- BODIED in BrnShowtimeInstantResults.cpp -------------------------------------
        void AppendExpectedComponents();           // @0x824B4998 (cpp:303)
        void HandleIncomingEvents();               // @0x824D5A78 (cpp:330)
        void SetupComponents();                    // @0x824B4B00 (cpp:519)
        void SetupTotalling();                     // @0x824BB548 (cpp:543)
        void SetupSummary();                       // @0x824B4BC0 (cpp:569)
        void UpdateSubstate();                     // @0x824DC3C8 (cpp:597)
        void UpdateEventResults();                 // @0x824D6008 (cpp:642)
        void UpdateScoreTotalling();               // @0x824C60B8 (cpp:732)
        bool TickSubstateAndEndIfDone();           // @0x824B4C40 (cpp:891)
        void TriggerExitResults();                 // @0x824C6430 (cpp:933)
        s32  CalculateMultiplier();                // @0x824B4D60 (cpp:951)
        EResultsActiveSubStates GetNextSubstate(); // @0x824B3B30 (DWARF h:271)
        void ResetStateTimer();                    // @0x824B3BD0 (DWARF h:303)
        void SetMultiplierText();                  // @0x824B3C38 (DWARF h:354)

        // @0x824B4A58 (cpp:482) -- the apt movie's own trigger callback.
        // ⚠️ THE PARAMETER TYPE IS `GuiEventAptTriggerPayload`, NOT `GuiEventAptTrigger`: the
        // queued event-21 record is the BARE 20-byte payload, and the X360 reads meEventType
        // at +0 / mpacComponentName at +8, which only holds without the 12-byte GuiEvent<21>
        // header. Same finding, same shape, as the offline sibling.
        void HandleAptTriggers(const CgsGui::GuiEventAptTriggerPayload* lpEvent);

        // ---- ⛔ NOT RECONSTRUCTED YET. Declared so the HandleIncomingEvents dispatch above
        //      can be written faithfully. HandleControllerInput has NO X360 export of its own
        //      in this class (the DWARF declares it at cpp:507 but the ARTIST build inlined or
        //      dropped it and no arm of HandleIncomingEvents reaches it), so there is nothing
        //      to transcribe and nothing calls it. Left undeclared rather than stubbed --
        //      declaring a method whose definition does not exist is what the ledger's
        //      "reviewed" default has cost five separate waves.

        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @0x82F26BB8 (.rdata)
        static const u32                    muNumResourcesToLoad; // @0x82F26BD0 (.rdata) == 3

        // ---- embedded components, in X360 order (see the banner) --------------------------
        HelpItem      mHelpItems[KI_HELPITEMS];    // +0x0038  (DWARF h:159)
        TextField     mFinishedText;               // +0x053C  (h:160)
        TextField     mTotalScoreText;             // +0x0664  (h:161)
        TextField     mMultiplierText;             // +0x078C  (h:162)
        TextField     mMultSymbolText;             // +0x08B4  (h:163)
        IconComponent mResultsIcon;                // +0x09DC  (h:166)

        // ---- scalar tail ------------------------------------------------------------------
        GuiCache*                  mpGuiCache;         // +0x0A70 (h:168)
        EResultsInternalStates     meCurrentState;     // +0x0A74 (h:169)
        EResultsActiveSubStates    meActiveSubState;   // +0x0A78 (h:171)
        bool  mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_COUNT];   // +0x0A7C (h:172)
        EResultsSubStateStates     meSubStateState;    // +0x0A80 (h:174)
        EResultsTotallingSubstates meTotallingStage;   // +0x0A84 (h:176)
        GuiEventOfflinePostEvent::OfflinePostEventData mResults;  // +0x0A88 (h:178), 192 bytes
        f32   mfTimeRemaining;                     // +0x0B48 (h:180)
        s32   miLastCrashedCars;                   // +0x0B4C (h:181)
        s32   miCrashExtensionsRemaining;          // +0x0B50 (h:182)
        s32   miDistanceDollars;                   // +0x0B54 (h:183)
        s32   miScoreMultiplierDollars;            // +0x0B58 (h:184)
        s32   miBankingDollarTotal;                // +0x0B5C (h:185)
        s32   miBankingDistanceDollars;            // +0x0B60 (h:186)
        s32   miBankingScoreMultiplier;            // +0x0B64 (h:187)
        s32   miCurrentMultiplier;                 // +0x0B68 (h:189, last member)
    };
}
