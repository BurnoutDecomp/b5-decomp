// ===================================================================================
// BrnGui::AchievementPopupComponent -- the "achievement unlocked" front-end pop-up
//   GameSource/Gui/Flow/Permanent/Components/BrnAchievementPopupComponent.cpp
//
//   Construct                          @ 0x82430110
//   Prepare                            @ 0x82415C20
//   Initialize                         @ 0x8242C310
//   Update                             @ 0x82424A98
//   SetTime                            @ 0x82415D60
//   DisplayNewAchievementNotification  @ 0x8242C338
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. All are non-static members (asm:
// r3 = this). Member access is BY NAME throughout; the X360 Begin/Fire/End (and the
// inlined BitArray bounds / StrStream "invalid index") dev-assert sequences fold into
// CGS_ASSERT(cond,"msg") per the module house style (see CgsAttribSysVaultAllocator /
// BrnGuiFlaptComponent exemplars).
//
// The component owns the achievement icon + the "awarded"/"name" text fields, plus a
// pending-achievements bit set (mAchievementsToShow) and a two-shot animation state
// machine. The X360 inlined the per-field BitArray IsBitSet/UnSetBit and the OR-merge
// (ORArrays); those fold back onto the named BitArray<60> methods here.
//
// IMPORTANT (rung-1 fidelity): the ARTIST Update body services ONLY the VISIBLE and
// INTERVAL states. The INVISIBLE -> "pick the next pending achievement and show it"
// path the DecFIGS .cpp hints sketch (GetFirstNonZeroBit / GetProgressionProfile /
// StrStream id formatting) is NOT present in this build's machine code, so it is NOT
// reconstructed here -- the X360 asm is authoritative for behaviour.
// ===================================================================================
#include "GameSource/Gui/Flow/Permanent/Components/BrnAchievementPopupComponent.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Permanent/Components/BrnAchievementPopupComponent.cpp"
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"             // BrnFlapt::FileRef (Prepare param)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"        // MovieClipRef lookups / GotoAndPlayLabel
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace BrnGui
{
    // The embedded icon's child-clip name within the pop-up movie (X360 .rdata
    // "AchievementIcon_mc"); the DWARF class static KAC_ACHIEVEMENTICON_NAME (h:114, char[]).
    const char AchievementPopupComponent::KAC_ACHIEVEMENTICON_NAME[] = "AchievementIcon_mc";

    // The clip frame labels (DWARF class statics h:94..96, `const char *`): ARTIST keeps the three
    // pointers in .data (0x82F24F80 "AnimIn", 0x82F24F84 "AnimOut", 0x82F24F88 "Blank") and Update
    // reloads KAPC_ANIM_OUT_FRAME from its word (0x82424B0C).
    const char* AchievementPopupComponent::KAPC_ANIM_IN_FRAME  = "AnimIn";
    const char* AchievementPopupComponent::KAPC_ANIM_OUT_FRAME = "AnimOut";
    const char* AchievementPopupComponent::KAPC_BLANK_FRAME    = "Blank";

    // "None showing" sentinel for miCurrentAchievementShowing -- one past the last
    // valid achievement id (== the bit-set capacity). Construct / Initialize / the
    // Update anim-out path all reset the index to this.
    static const s32 KI_NO_ACHIEVEMENT_SHOWING = 60;

    // Animation timings (seconds). KF_TIME_BETWEEN_POPUPS_SECS is the gap Update adds
    // to the current time when the visible pop-up finishes animating out (asm:
    // fadds mfCurrentGameTime + flt_8204C554). KF_TIME_FOR_DISPLAYING_POPUP_SECS is
    // declared per the DWARF but is only used by the (compiled-out) show path, so its
    // exact magnitude is not recoverable from this build -- see the file note.
    static const f32 KF_TIME_BETWEEN_POPUPS_SECS      = 1.5f;
    static const f32 KF_TIME_FOR_DISPLAYING_POPUP_SECS = 4.0f;

    // @ 0x82430110 -- bind the state interface, invalidate the apt clip + text-field
    // handles, zero the runtime state, and construct the embedded achievement icon.
    void AchievementPopupComponent::Construct(const char* /*lpcMovieClipName*/,
                                              CgsGui::StateInterface* lpStateInterface,
                                              s32 /*liFlags*/)
    {
        // The header-inline base Construct (BrnGuiFlaptComponent.h:113 assert, li r5,0x71):
        // bind the state channel and invalidate the apt clip handle.
        BrnFlaptComponent::Construct(lpStateInterface);

        // Idle runtime state (ARTIST stores +0x20, +0x1C, then +0x10, +0x18 @0x82430178..8C).
        meComponentState            = E_CS_INVISIBLE;
        mfCurrentGameTime_Seconds   = 0.0f;
        mAchievementsToShow.UnSetAll();
        miCurrentAchievementShowing = KI_NO_ACHIEVEMENT_SHOWING;

        // Build the embedded icon (X360 inlined its base init through the vtable call).
        mAchievementIcon.Construct(KAC_ACHIEVEMENTICON_NAME, lpStateInterface, 0);

        // Invalidate the two text-field handles.
        mAchievementAwardedRef.SetInvalid();

        mAchievementNameRef.SetInvalid();
    }

    // @ 0x82415C20 -- bind the pop-up's own apt clip, the embedded icon, and the two
    // child text fields out of the loaded Flapt file.
    void AchievementPopupComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
    {
        // Bind + reset this component's own movie clip (bare-name lookup, no parent);
        // the base asserts lacName and the bound clip instance.
        BrnFlaptComponent::Prepare(lacName, lFile, 0);

        // Prepare the achievement icon as the "<lacName>_AchievementIcon_mc" child.
        mAchievementIcon.Prepare(KAC_ACHIEVEMENTICON_NAME, lFile, lacName);

        // The two text fields live under the "AchievementsBox_mc" child clip.
        // DWARF local lAchievementPopupBox (cpp:80), copy-initialised: ARTIST builds the found
        // clip straight into it (0x82415CE8).
        BrnFlapt::MovieClipRef lAchievementPopupBox = mAptRef.FindChildMovieClip("AchievementsBox_mc");

        mAchievementAwardedRef = lAchievementPopupBox.FindChildTextField("AchievementAwarded_txt");
        mAchievementNameRef = lAchievementPopupBox.FindChildTextField("AchievementName_txt");
    }

    // @ 0x8242C310 -- reset the runtime state to its idle defaults.
    void AchievementPopupComponent::Initialize()
    {
        mfCurrentGameTime_Seconds   = 0.0f;
        meComponentState            = E_CS_INVISIBLE;
        mAchievementsToShow.UnSetAll();
        miCurrentAchievementShowing = KI_NO_ACHIEVEMENT_SHOWING;
    }

    // @ 0x82415D60 -- record the current game time (seconds).
    void AchievementPopupComponent::SetTime(f32 lfGameTime_Seconds)
    {
        mfCurrentGameTime_Seconds = lfGameTime_Seconds;
    }

    // @ 0x8242C338 -- queue the supplied achievements for display by OR-merging the
    // pending set into mAchievementsToShow.
    void AchievementPopupComponent::DisplayNewAchievementNotification(
            const AchievementsBitArray* lpAchievementsToShow)
    {
        CGS_ASSERT_AT(lpAchievementsToShow != 0, "lpAchievementsToShow", "..\\..\\..\\GameSource\\Gui/Flow/Permanent/Components/BrnAchievementPopupComponent.cpp", 223 /* ARTIST 0x8242C374 */);

        mAchievementsToShow.ORArrays(&mAchievementsToShow, lpAchievementsToShow);
    }

    // @ 0x82424A98 -- advance the pop-up's two-shot animation state machine.
    void AchievementPopupComponent::Update()
    {
        // A switch on the state: ARTIST tests VISIBLE first with a `beq` to its arm placed
        // last and falls into the INTERVAL arm (0x82424AAC..0x82424AD8).
        switch (meComponentState)
        {
        case E_CS_VISIBLE:
            // While visible: once the game time passes the anim-out time, play the
            // "AnimOut" clip, switch to the post-display interval, and clear the
            // achievement that was on screen.
            if (mfCurrentGameTime_Seconds > mfAnimOutTime_Seconds)
            {
                meComponentState       = E_CS_INTERVAL;
                mfIntervalTime_Seconds = mfCurrentGameTime_Seconds + KF_TIME_BETWEEN_POPUPS_SECS;
                mAptRef.GotoAndPlayLabel(KAPC_ANIM_OUT_FRAME);   // the class static's .data word (0x82F24F84)

                // Inlined BitArray<60>::IsBitSet (its bound assert CgsBitArray.h:203) + probe.
                CGS_ASSERT_AT(mAchievementsToShow.IsBitSet(
                               static_cast<u32>(miCurrentAchievementShowing)),
                           "mAchievementsToShow.IsBitSet( miCurrentAchievementShowing )", CGS_ARTIST_FILE, 173);

                // Inlined BitArray<60>::UnSetBit bounds (CgsBitArray.h:241) + clear.
                mAchievementsToShow.UnSetBit(static_cast<u32>(miCurrentAchievementShowing));
                miCurrentAchievementShowing = KI_NO_ACHIEVEMENT_SHOWING;
            }
            break;

        case E_CS_INTERVAL:
            // After the interval gap elapses, return to invisible.
            if (mfCurrentGameTime_Seconds > mfIntervalTime_Seconds)
            {
                meComponentState = E_CS_INVISIBLE;
            }
            break;

        default:
            break;
        }
    }
}
