// ===================================================================================
// BrnGui::FlaptIconComponent / BrnGui::FlaptAnimatorComponent
//   GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.cpp
//
//   FlaptIconComponent::Construct          @ 0x8241C1F0
//   FlaptIconComponent::Prepare            @ 0x8241C250
//   FlaptIconComponent::SetState           @ 0x8241C2C0
//   FlaptIconComponent::GotoAndStopLabel   @ 0x8241C3D8
//   FlaptAnimatorComponent::Construct                      @ 0x8241C4E8
//   FlaptAnimatorComponent::Prepare                        @ 0x8241C568
//   FlaptAnimatorComponent::RefreshControlledMovieClips    @ 0x8241C668
//   FlaptAnimatorComponent::Run                            @ 0x8241C710
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. All are non-static members (asm:
// r3 = this). Member access is BY NAME throughout; the X360 Begin/Fire/End (and the
// StrStream "mystical string" hash-collision) dev-assert sequences fold into
// CGS_ASSERT(cond,"msg") per the module house style.
//
// The "state" of an icon is the hash of its current timeline label (muCurrentStateHash):
// switching state only re-drives the apt clip when the hashed label actually changes.
// The animator additionally drives up to KI_MAX_ANIMATOR_CHILD_MOVIE_CLIPS named child
// clips, the names coming from the clip's published TriggerParameters.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"

#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"            // BrnFlapt::FileRef
#include "GameShared/GameClasses/Containers/CgsHash.h"       // CgsHash::CalculateHash
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"            // BrnFlapt::TriggerParameters
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

namespace BrnGui
{
    // Inline strlen the X360 folds before each CalculateHash: count up to (not
    // including) the NUL terminator -- the byte count CalculateHash expects.
    static int IconLabelLength(const char* lpcName)
    {
        int liLength = 0;
        while (lpcName[liLength] != '\0')
        {
            ++liLength;
        }
        return liLength;
    }

    // ---- FlaptIconComponent ------------------------------------------------------

    // @ 0x8241C1F0 -- bind the state interface and clear the icon's clip + state.
    void FlaptIconComponent::Construct(const void* /*lpDEBUGName*/,
                                       CgsGui::StateInterface* lpStateInterface,
                                       const void* /*lpcParentName*/)
    {
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");

        mpStateInterface = lpStateInterface;
        mAptRef.mpMovieClipInst = 0;
        mAptRef.mpTransform     = 0;
        muCurrentStateHash      = 0;
    }

    // @ 0x8241C250 -- bind the named clip through the base, then reset the state hash.
    void FlaptIconComponent::Prepare(const char* lacName,
                                     const BrnFlapt::FileRef& lFile,
                                     const char* lacParentName)
    {
        CGS_ASSERT(lacName != 0, "lacName");

        BrnFlaptComponent::Prepare(lacName, lFile, lacParentName);
        muCurrentStateHash = 0;
    }

    // @ 0x8241C2C0 -- play the clip on the hashed state label, if it changed.
    void FlaptIconComponent::SetState(const char* lpcStateName)
    {
        CGS_ASSERT(lpcStateName != 0, "lpcStateName");

        u32 luStateHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpcStateName), IconLabelLength(lpcStateName));
        CGS_ASSERT(luStateHash != 0,
                   "You've discovered a mystical string that hashes to zero, "
                   "This is bad. But well done. Call AlexM or RichG!");

        if (luStateHash != muCurrentStateHash)
        {
            mAptRef.GotoAndPlayLabel(luStateHash, lpcStateName);
            muCurrentStateHash = luStateHash;
        }
    }

    // @ 0x8241C3D8 -- stop the clip on the hashed state label, if it changed.
    void FlaptIconComponent::GotoAndStopLabel(const char* lpcStateName)
    {
        CGS_ASSERT(lpcStateName != 0, "lpcStateName");

        u32 luStateHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpcStateName), IconLabelLength(lpcStateName));
        CGS_ASSERT(luStateHash != 0,
                   "You've discovered a mystical string that hashes to zero, "
                   "This is bad. But well done. Call AlexM or RichG!");

        if (luStateHash != muCurrentStateHash)
        {
            mAptRef.GotoAndStopLabel(lpcStateName);
            muCurrentStateHash = luStateHash;
        }
    }

    // ---- FlaptAnimatorComponent --------------------------------------------------

    // @ 0x8241C4E8 -- as the base icon, plus clear the controlled-clip array + count.
    void FlaptAnimatorComponent::Construct(const void* /*lpDEBUGName*/,
                                           CgsGui::StateInterface* lpStateInterface,
                                           const void* /*lpcParentName*/)
    {
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");

        mpStateInterface = lpStateInterface;
        mAptRef.mpMovieClipInst = 0;
        mAptRef.mpTransform     = 0;
        muCurrentStateHash      = 0;

        miNumChildMovieClips = 0;
        for (int liClip = 0; liClip < KI_MAX_ANIMATOR_CHILD_MOVIE_CLIPS; ++liClip)
        {
            maChildMovieClips[liClip].mpMovieClipInst = 0;
            maChildMovieClips[liClip].mpTransform     = 0;
        }
    }

    // @ 0x8241C568 -- bind the icon clip, resolve the controlled child clips named by
    // the clip's trigger parameters (under the clip's parent), then hide the clip.
    void FlaptAnimatorComponent::Prepare(const char* lacName,
                                         const BrnFlapt::FileRef& lFile,
                                         const char* lacParentName)
    {
        CGS_ASSERT(lacName != 0, "lacName");

        BrnFlaptComponent::Prepare(lacName, lFile, lacParentName);
        muCurrentStateHash = 0;

        const BrnFlapt::TriggerParameters* lpTriggerParameters =
            mAptRef.GetTriggerParameters();

        BrnFlapt::MovieClipRef lParentClip;
        mAptRef.GetParent(&lParentClip);

        CGS_ASSERT(lpTriggerParameters != 0, "NULL != lpTriggerParameters");

        int liCount = 0;
        do
        {
            const char* lpcChildName = lpTriggerParameters->mapcParameters[liCount];
            if (lpcChildName == 0)
            {
                break;
            }

            BrnFlapt::MovieClipRef lChildClip;
            lParentClip.FindChildMovieClip(&lChildClip, lpcChildName);
            maChildMovieClips[liCount] = lChildClip;
            ++liCount;
        }
        while (liCount < KI_MAX_ANIMATOR_CHILD_MOVIE_CLIPS);

        miNumChildMovieClips = liCount;
        mAptRef.SetVisible(false);
    }

    // @ 0x8241C668 -- re-resolve the controlled child clips against the clip's current
    // frame (same trigger-parameter name list).
    void FlaptAnimatorComponent::RefreshControlledMovieClips()
    {
        const BrnFlapt::TriggerParameters* lpTriggerParameters =
            mAptRef.GetTriggerParameters();

        BrnFlapt::MovieClipRef lParentClip;
        mAptRef.GetParent(&lParentClip);

        CGS_ASSERT(lpTriggerParameters != 0, "NULL != lpTriggerParameters");

        int liCount = 0;
        do
        {
            const char* lpcChildName = lpTriggerParameters->mapcParameters[liCount];
            if (lpcChildName == 0)
            {
                break;
            }

            BrnFlapt::MovieClipRef lChildClip;
            lParentClip.FindChildMovieClipOnFrame(&lChildClip, lpcChildName);
            maChildMovieClips[liCount] = lChildClip;
            ++liCount;
        }
        while (liCount < KI_MAX_ANIMATOR_CHILD_MOVIE_CLIPS);

        miNumChildMovieClips = liCount;
    }

    // @ 0x8241C710 -- goto-and-play lpcFrameName on every controlled child clip.
    void FlaptAnimatorComponent::Run(const char* lpcFrameName)
    {
        CGS_ASSERT(lpcFrameName != 0, "NULL != lpcFrameName");

        for (int liClip = 0; liClip < miNumChildMovieClips; ++liClip)
        {
            maChildMovieClips[liClip].GotoAndPlayLabel(lpcFrameName);
        }
    }
}
