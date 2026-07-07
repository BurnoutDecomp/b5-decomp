#include "GameSource/Gui/Flow/HUD/Components/BrnJunctionInfoComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT + CgsDev::Assert::{Begin,Fire,End}Assert
#include "GameShared/GameClasses/Core/CgsID.h"                                     // CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                            // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"                        // CgsDev::StrStream (streamed assert message)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                             // BrnFlapt::MovieClipRef (GotoAnd*Label)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h"  // AttachToTextFieldComponent

// BrnGui::JunctionInfoComponent -- the in-race junction/event-start HUD panel,
// reconstructed from BURNOUT_X360_ARTIST.XEX. This slice homes the eight functions whose
// bodies are fully grounded by the X360 asm: Construct / Prepare / HandleJunctionChange /
// Refresh / Run / GetMedalFrameNameFromMedal / TransitionInMainClip / TransitionOutMainClip /
// SetEventNameText. SetEventNameText's per-gamemode name-string table (gGameModeNameStringIds
// @0x82F27840) is referenced but declaration-only -- only entry 0 ("GAMEMODE_RACE") is
// attested by the export, so its full contents are defined by this class's data TU.
// SetupAptVariables is still declaration-only (it indexes further per-gamemode static tables
// whose contents are not in the export, and builds a GuiEventTickerCustomMessage whose exact
// size the ledger does not reconcile).

namespace BrnGui
{
    // @0x82F27840 -- per-gamemode event-name localisation string-id table, indexed by
    // GuiEventJunctionInfo::meGameModeType (0..E_MODE_OFFLINE_COUNT). Entry 0
    // (E_MODE_OFFLINE_RACE) is X360-attested "GAMEMODE_RACE"; the remaining entries are
    // serialised label data not present in this function export, so the table is
    // declaration-only here (defined by this class's data TU once every gamemode label is
    // recovered). Not a DWARF class member -- it is the .cpp's file-scope name table.
    extern const char* const gGameModeNameStringIds[];

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
