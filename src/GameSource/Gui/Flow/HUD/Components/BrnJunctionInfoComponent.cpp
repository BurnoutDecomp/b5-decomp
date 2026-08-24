#include "GameSource/Gui/Flow/HUD/Components/BrnJunctionInfoComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT + CgsDev::Assert::{Begin,Fire,End}Assert
#include "GameShared/GameClasses/Core/CgsID.h"                                     // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                            // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"                        // CgsDev::StrStream (streamed assert message)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                             // BrnFlapt::MovieClipRef (GotoAnd*Label)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h"  // AttachToTextFieldComponent
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                                // CgsGui::GuiEvent<N> (the 537 ticker wire)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"           // StateInterface out-queue (SetupAptVariables ticker post)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                   // AddEvent
#include <cstring>                                                                  // std::strncpy / std::memset (the ticker wire)

// BrnGui::JunctionInfoComponent -- the in-race junction/event-start HUD panel,
// reconstructed from BURNOUT_X360_ARTIST.XEX. COMPLETE as of 2026-08-25: all ten ledger
// functions are bodied here (Construct / Prepare / HandleJunctionChange / Refresh / Run /
// GetMedalFrameNameFromMedal / TransitionInMainClip / TransitionOutMainClip /
// SetEventNameText / SetupAptVariables). The two former parks were both recoverable:
// the per-gamemode tables (gGameModeNameStringIds @0x82F27840, KAPC_GAMEMODE_ICON_FRAMENAMES
// @0x82F24ECC) read cleanly off the image, and the GuiEventTickerCustomMessage wire is the
// attested 2072-byte 537 record the livery/stunt producers already model TU-locally.

namespace BrnGui
{
    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT = 40;   // GuiEventOut (the OutputGuiEvent channel)

        // The 0x818-byte custom ticker message + its GuiEvent<537> wire header -- the
        // TU-local model of BrnGui::GuiEventTickerCustomMessage's on-queue record (the
        // canonical BrnGuiDemangledEventTypes.h entry is an opaque 12B-header shape that
        // does NOT match the wire; posting the exact record through the out queue is the
        // standing accommodation -- BrnCarSelectLivery_Components.cpp is the precedent,
        // and carries the layout attestation: AddString @0x823A6940, count @+0x810,
        // types stride 4 @+0, strings stride 512 @+0x10).
        // SetupAptVariables @0x824398A0 zero-seeds the whole payload (memset + the five
        // tail-byte stores), so ALL flags stay 0 here -- unlike the livery producer's
        // {1,0,1,0} seed.
        struct GuiTickerCustomMessagePayload537
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;
            static const s32 KI_MAX_STRING_LENGTH = 512;

            s32  maiStringTypes[KI_MAX_NUM_STRINGS];                       // +0x000
            char maacStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH];    // +0x010
            s8   mi8NumStrings;                                            // +0x810
            u8   maFlags[4];                                               // +0x811
            u8   maPad815[3];                                              // +0x815

            // X360 0x823A6940, transcribed (the console's own bounds asserts, then the
            // 512-byte strncpy + type store + count bump).
            void AddString(const char* lpString, s32 liType)
            {
                CGS_ASSERT(mi8NumStrings >= 0, "mi8NumStrings >= 0");                   // h:390
                CGS_ASSERT(mi8NumStrings < KI_MAX_NUM_STRINGS,
                           "mi8NumStrings < KI_MAX_NUM_STRINGS");                       // h:391
                CGS_ASSERT(lpString != 0, "lpString");                                  // h:392
                std::strncpy(maacStrings[mi8NumStrings], lpString,
                             static_cast<size_t>(KI_MAX_STRING_LENGTH));
                maiStringTypes[mi8NumStrings] = liType;
                ++mi8NumStrings;
            }
        };

        // { 0x818, 537, 12, <the message> }, channel 40.
        struct GuiTickerCustomMessageWire537 : public CgsGui::GuiEvent<537>
        {
            GuiTickerCustomMessagePayload537 mMessage;   // +0x0C
            GuiTickerCustomMessageWire537()
                : CgsGui::GuiEvent<537>(
                      static_cast<u32>(sizeof(GuiTickerCustomMessagePayload537)), 12)
            {
                std::memset(&mMessage, 0, sizeof(mMessage));
            }
        };
    }

    // @0x82F27840 -- per-gamemode event-name localisation string-id table, indexed by
    // GuiEventJunctionInfo::meGameModeType. Contents RECOVERED from the image 2026-08-25
    // (headless idat read of the 10 pointers at 0x82F27840, which run flush into the
    // plural table at 0x82F27868; scratch h1_dump.txt). The old "declaration-only, defined
    // by the data TU" banner was recoverable-after-all. Not a DWARF class member -- the
    // .cpp's file-scope name table.
    const char* const gGameModeNameStringIds[10] =
    {
        "GAMEMODE_RACE",            // 0  E_MODE_OFFLINE_RACE (X360-attested by the export)
        "GAMEMODE_FACEOFF",         // 1
        "GAMEMODE_CRASH",           // 2  (showtime)
        "GAMEMODE_ROADRAGE",        // 3
        "GAMEMODE_PURSUIT",         // 4
        "GAMEMODE_BURNINGROUTE",    // 5
        "GAMEMODE_ELIMINATOR",      // 6
        "GAMEMODE_STUNTATTACK",     // 7
        "GAMEMODE_SURVIVAL",        // 8
        "GAMEMODE_TRAFFICATTACK",   // 9
    };

    // @0x82F24ECC -- the per-gamemode apt icon frame-name table SetupAptVariables runs the
    // GameModeIcon animator to (DWARF static KAPC_GAMEMODE_ICON_FRAMENAMES; the X360 body
    // indexes it as off_82F24ECC[mode] with the mode-5 arm special-cased). Contents
    // RECOVERED from the image 2026-08-25 (11 pointers at 0x82F24ECC; scratch h1_dump2.txt).
    const char* const JunctionInfoComponent::KAPC_GAMEMODE_ICON_FRAMENAMES[11] =
    {
        "race",           // 0  E_MODE_OFFLINE_RACE
        "default",        // 1  (faceoff)
        "default",        // 2  (showtime)
        "roadrage",       // 3
        "pursuit",        // 4
        "burningroute",   // 5  (overridden to KAC_CURRENT_BURNING_ROUTE_ICON_FRAMENAME
                          //     when the junction's car is the player's original car)
        "default",        // 6  (eliminator)
        "stunt",          // 7
        "survival",       // 8
        "default",        // 9
        "default",        // 10
    };

    // @0x8204B138-adjacent literal in SetupAptVariables ("Currentburningroute") -- the
    // mode-5 override frame name (DWARF static KAC_CURRENT_BURNING_ROUTE_ICON_FRAMENAME).
    const char* const JunctionInfoComponent::KAC_CURRENT_BURNING_ROUTE_ICON_FRAMENAME =
        "Currentburningroute";

    // @ 0x82423DE0 -- base init (adopt the state interface, invalidate the clip; the
    // h:113 lpStateInterface tripwire fires here), zero the pending junction-info event,
    // construct the three animator children and the two start-hint button icons under this
    // state interface, invalidate the two event-name text fields, and seed the bool flags.
    // The X360 inlines every base/child Construct and the ref invalidations.
    void JunctionInfoComponent::Construct(const char* lacName,
                                          CgsGui::StateInterface* lpStateInterface,
                                          const char* lacParentName,
                                          s32 liParentAptLayer)
    {
        (void)lacName;
        (void)lacParentName;
        (void)liParentAptLayer;

        BrnFlaptComponent::Construct(lpStateInterface);   // inlined on the X360

        mJunctionInfo = GuiEventJunctionInfo();

        mGameModeIconAnimator.Construct(0, lpStateInterface, 0);
        mMedalAnimator.Construct(0, lpStateInterface, 0);
        mEventNameTextfield.SetInvalid();
        mEventNameTextfield2Line.SetInvalid();
        mStartHintAnimator.Construct(0, lpStateInterface, 0);
        mStartHintButton1.Construct(0, lpStateInterface, 0);
        mStartHintButton2.Construct(0, lpStateInterface, 0);

        mbInJunction        = false;
        mbShowingStartHint  = false;
        mbShowing2LineName  = false;
        mbGameComplete      = false;
    }

    // @ 0x8242BCC0 -- bind this panel's root apt clip out of lFile (inlined base Prepare:
    // resolve+bind mAptRef and reset its timeline), prepare the three animators and two
    // start-hint button icons under the "JunctionInfo_mc" parent, resolve the two event-
    // name text fields, then hide the root clip by stopping it on its "invisible" label.
    void JunctionInfoComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
    {
        BrnFlaptComponent::Prepare(lacName, lFile, 0);   // inlined on the X360

        mGameModeIconAnimator.Prepare("GameModeIcon_cpt", lFile, "JunctionInfo_mc");
        mMedalAnimator.Prepare("Medal_anim", lFile, "JunctionInfo_mc");
        mStartHintAnimator.Prepare("StartHint_anim", lFile, "JunctionInfo_mc");

        mStartHintButton1.Prepare("StartPromptLeft_cpt", lFile, "JunctionInfo_mc");
        mStartHintButton1.Setup();
        mStartHintButton2.Prepare("StartPromptRight_cpt", lFile, "JunctionInfo_mc");
        mStartHintButton2.Setup();

        BrnFlapt::TextFieldRef lTextField;
        mEventNameTextfield = *AttachToTextFieldComponent(
            &lTextField, "EventName_txt", "EventNameText_cpt", lacName, lFile);
        mEventNameTextfield2Line = *AttachToTextFieldComponent(
            &lTextField, "EventName2Line_txt", "EventNameText2Line_cpt", lacName, lFile);

        mAptRef.GotoAndStopLabel("invisible");
    }

    // @ 0x824400B8 -- adopt a freshly-arrived junction-info event: copy it wholesale into
    // mJunctionInfo, clear its difficulty byte, record the player's current car id, then
    // re-derive the panel's apt state from the new data.
    void JunctionInfoComponent::HandleJunctionChange(const GuiEventJunctionInfo* lpEvent,
                                                     CgsID lCurrentCarId)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");

        mJunctionInfo = *lpEvent;
        mJunctionInfo.mi8Difficulty = 0;
        mCurrentCarId = lCurrentCarId;

        SetupAptVariables();
    }

    // @ 0x824398A0 -- (re)derive the panel's whole apt state from mJunctionInfo. Reconstructed
    // from the asm (scratch h1_dump.txt; the Hex-Rays view of the mode-5 car compare shows two
    // dword loads, but the asm is `ld 0x100 / ld 0x128 / cmpld` -- a FULL 64-bit CgsID compare).
    //   * At a junction (mbOnEntry): an enterable event shows the start hint once -- the hint
    //     animator plays "transin", the two hint buttons are pinned to LTRIGGER/"ltrigger" and
    //     RTRIGGER/"rtrigger" (the X360 pokes meButton/mAptButtonRef inline; friend-granted, the
    //     FlaptHelpItem::SetItem precedent), and, when the event has no medal yet and the game
    //     is not complete, the one-shot "WINNING_WONT_CONTRIBUTE" ticker line is posted (the
    //     X360 calls OutputGuiEvent<GuiEventTickerCustomMessage> @0x82436C40; the host builds
    //     the same 537 wire record and posts it at its own sizeof -- the standing accommodation
    //     for the ticker family, BrnCarSelectLivery_Components.cpp precedent). A non-enterable
    //     event transitions a showing hint back out.
    //   * Then, if the panel is not up yet: assert the gamemode into [0,10], run the gamemode
    //     icon animator to its frame (mode 5 == burning route compares the event's car id with
    //     the player's ORIGINAL car id -- both CgsIDs -- and shows "Currentburningroute" on a
    //     match), refresh + run the medal animator, set the event-name text and transition the
    //     main clip in.
    //   * Off a junction with the panel up: transition a showing start hint out, then the main
    //     clip out.
    void JunctionInfoComponent::SetupAptVariables()
    {
        if (mJunctionInfo.mbOnEntry)
        {
            if (mJunctionInfo.mbCanEnterEvent)
            {
                if (!mbShowingStartHint)
                {
                    mStartHintAnimator.Run("transin");

                    // The X360 pokes the two hint buttons inline (no SetButton symbol in the
                    // ledger): meButton := LTRIGGER(10)/RTRIGGER(11), then the glyph clip is
                    // stopped on the matching label.
                    mStartHintButton1.meButton = FlaptButtonIconComponent::E_PADBUTTON_LTRIGGER;
                    mStartHintButton1.mAptButtonRef.GotoAndStopLabel("ltrigger");
                    mStartHintButton2.meButton = FlaptButtonIconComponent::E_PADBUTTON_RTRIGGER;
                    mStartHintButton2.mAptButtonRef.GotoAndStopLabel("rtrigger");

                    mbShowingStartHint = true;

                    if (mJunctionInfo.mi8MedalAchieved == 0 && !mbGameComplete)
                    {
                        // The 2072-byte GUI-event-537 wire record ({ types[4], strings[4][512],
                        // count, flags[4] } behind the 12-byte GuiEvent header), zero-seeded as
                        // the X360 stack build is, with the one string added.
                        GuiTickerCustomMessageWire537 lTicker;
                        lTicker.mMessage.AddString("WINNING_WONT_CONTRIBUTE", 2);
                        mpStateInterface->GetOutputEventQueue()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lTicker),
                            KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lTicker)));
                    }
                }
            }
            else if (mbShowingStartHint)
            {
                mStartHintAnimator.Run("transout");
                mbShowingStartHint = false;
            }

            if (!mbInJunction)
            {
                CGS_ASSERT(mJunctionInfo.meGameModeType >= 0,
                           "mJunctionInfo.meGameModeType >= 0");                              // cpp:230
                CGS_ASSERT(mJunctionInfo.meGameModeType <= 10,
                           "mJunctionInfo.meGameModeType <= BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_COUNT");   // cpp:231

                const char* lpIconFrameName;
                if (mJunctionInfo.meGameModeType == 5)   // E_MODE_BURNING_ROUTE
                {
                    // `ld 0x100 / ld 0x128 / cmpld` -- the full 64-bit CgsID compare.
                    if (mJunctionInfo.mSpecialEventCarId == mCurrentCarId)
                        lpIconFrameName = KAC_CURRENT_BURNING_ROUTE_ICON_FRAMENAME;
                    else
                        lpIconFrameName = KAPC_GAMEMODE_ICON_FRAMENAMES[5];
                }
                else
                {
                    lpIconFrameName = KAPC_GAMEMODE_ICON_FRAMENAMES[mJunctionInfo.meGameModeType];
                }

                mGameModeIconAnimator.Run(lpIconFrameName);
                mMedalAnimator.RefreshControlledMovieClips();
                mMedalAnimator.Run(GetMedalFrameNameFromMedal(mJunctionInfo.mi8MedalAchieved));
                SetEventNameText();
                TransitionInMainClip();
            }
        }
        else if (mbInJunction)
        {
            if (mbShowingStartHint)
            {
                mStartHintAnimator.Run("transout");
                mbShowingStartHint = false;
            }
            TransitionOutMainClip();
        }
    }

    // @0x82414D58 (assert at BrnJunctionInfoComponent.cpp:289) -- name tripwire only. The
    // panel state is driven from HandleJunctionChange/SetupAptVariables; the X360 body of
    // Refresh is just the argument assert.
    void JunctionInfoComponent::Refresh(const char* lpComponentName)
    {
        CGS_ASSERT(lpComponentName != NULL, "lpComponentName != NULL");
    }

    // @0x82423F40 -- play the named animation on the panel's own apt clip (base mAptRef)
    // and forward it to the start-hint animator's controlled child clips.
    void JunctionInfoComponent::Run(const char* lpcAnimation)
    {
        mAptRef.GotoAndPlayLabel(lpcAnimation);   // @0x8246F3E8 (BrnFlaptComponent base mAptRef @+0x04)
        mStartHintAnimator.Run(lpcAnimation);     // mStartHintAnimator @+0x94
    }

    // @ 0x82414DA0 -- map a medal-achieved code to the medal animator's frame label.
    // -1 (no medal) -> "NoMedal"; 0 and 1 -> "Gold"; 2 -> "Bronze". The X360 compiles this
    // as a (li8Medal+1) jump table; any other value trips the assert and returns NULL.
    const char* JunctionInfoComponent::GetMedalFrameNameFromMedal(s8 li8Medal)
    {
        switch (li8Medal)
        {
        case -1:
            return "NoMedal";
        case 0:
        case 1:
            return "Gold";
        case 2:
            return "Bronze";
        default:
            CGS_ASSERT(false, "Unhandled medal index in JunctionInfoComponent::GetMedalFrameNameFromMedal\n");
            return 0;
        }
    }

    // TransitionInMainClip @ 0x82414FD8 -- play the main junction-info clip's "transition
    // in" label (the 2-line variant when a two-line event name is showing) and mark the
    // panel as in-junction. GetMovieClipRef() is the base mAptRef.
    void JunctionInfoComponent::TransitionInMainClip()
    {
        const char* lpTransInFrameName = "transin";
        if (mbShowing2LineName)
        {
            lpTransInFrameName = "transin_2line";
        }
        GetMovieClipRef().GotoAndPlayLabel(lpTransInFrameName);
        mbInJunction = true;
    }

    // TransitionOutMainClip @ 0x82415030 -- play the main junction-info clip's "transition
    // out" label (2-line variant when a two-line name is showing) and clear the in-junction
    // flag. Mirror of TransitionInMainClip.
    void JunctionInfoComponent::TransitionOutMainClip()
    {
        const char* lpTransOutFrameName = "transout";
        if (mbShowing2LineName)
        {
            lpTransOutFrameName = "transout_2line";
        }
        GetMovieClipRef().GotoAndPlayLabel(lpTransOutFrameName);
        mbInJunction = false;
    }

    // @ 0x82414E60 -- (re)populate the event-name text field(s) for the current junction.
    // Both name fields are first cleared and the 2-line flag reset. A special-event car
    // challenge takes precedence: the car id is stringified, formatted as "CAR_CAPS_<car>",
    // and passed as the single positional parameter of the JNC_INFO_SPECIAL_EVENT_X_CHALLENGE
    // localised string into the TWO-line field (setting mbShowing2LineName). Otherwise an
    // unlocked event shows its per-gamemode localised name in the one-line field; a locked,
    // non-special junction is a design error -- the X360 streams a diagnostic assert message
    // (gamemode value interpolated) and falls back to the literal "LOCKED EVENT".
    void JunctionInfoComponent::SetEventNameText()
    {
        mbShowing2LineName = false;
        mEventNameTextfield.ClearText();
        mEventNameTextfield2Line.ClearText();

        if (mJunctionInfo.mSpecialEventCarId != 0)
        {
            const s32 KI_TEMP_STRING_LENGTH = 31;

            char lacCarID[13];
            CgsIDConvertToString(mJunctionInfo.mSpecialEventCarId, lacCarID);

            char lacTempCarStringID[32];
            CgsCore::SnPrintf(lacTempCarStringID, KI_TEMP_STRING_LENGTH, "CAR_CAPS_%s", lacCarID);
            lacTempCarStringID[KI_TEMP_STRING_LENGTH] = 0;

            const char* lapStringParams[1]     = { lacTempCarStringID };
            s32         laStringFormatTypes[1]  = { 9 };
            mEventNameTextfield2Line.SetLocalisedText(
                "JNC_INFO_SPECIAL_EVENT_X_CHALLENGE", 9, 1, lapStringParams, laStringFormatTypes);

            mbShowing2LineName = true;
        }
        else if (mJunctionInfo.mbEventUnlocked)
        {
            mEventNameTextfield.SetLocalisedText(
                gGameModeNameStringIds[mJunctionInfo.meGameModeType], 9);
        }
        else
        {
            // Streamed diagnostic message (gamemode value interpolated). Lowered to the committed
            // CgsID.cpp house idiom: build into a local KI_MESSAGEBUFFERSIZE buffer, then
            // BeginAssert/FireAssert/EndAssert. (The X360 streams into the global gpcMessageBuffer,
            // which the reconstruction folds to this local buffer, as every committed streamed-assert
            // site does; gpcMessageBuffer is not a materialised symbol here.)
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Junction with gamemode ";
            lStrStream << (s32)mJunctionInfo.meGameModeType;
            lStrStream << " is locked (and is not a car special event) - is this correct?\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                lacMessage,
                "..\\..\\..\\GameSource\\Gui/Flow/HUD/Components/BrnJunctionInfoComponent.cpp",
                429);
            CgsDev::Assert::EndAssert();

            mEventNameTextfield.SetText("LOCKED EVENT", 0);
        }
    }
}
