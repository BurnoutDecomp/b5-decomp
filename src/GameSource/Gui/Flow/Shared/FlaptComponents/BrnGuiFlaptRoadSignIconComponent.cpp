// ===================================================================================
// BrnGui::FlaptRoadSignIconComponent  -- apt sat-nav road-sign component
//   GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptRoadSignIconComponent.cpp
//
//   Construct             @ 0x8241CB68
//   Prepare               @ 0x8241CC18
//   FindRoadFromName      @ 0x8241CD08
//   DisplayRoad(char*)    @ 0x82427E58
//   SetColour             @ 0x82427F30
//   DisplayRoadFromCgsID  @ 0x8242DBD0
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. All are non-static members (asm:
// r3 = this). Member access is BY NAME throughout (the flat attested layout lives in
// the header). The X360 Begin/StrStream/Fire/End dev-assert sequences fold into
// CGS_ASSERT(cond,"msg") per the module house style (file/line via __FILE__/__LINE__);
// the asserts whose message the X360 streamed together from literal fragments + a value
// (FindRoadFromName's "Unable to find a valid icon name (...)" and SetColour's
// "Invalid colour (...)") are folded to a single CGS_ASSERT carrying the literal
// message, matching the sibling FlaptIconComponent / FlaptTimerFieldComponent style.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptRoadSignIconComponent.h"

#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"            // BrnFlapt::FileRef::FindComponent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"  // MovieClipInstance::ResetTimeline
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

namespace BrnGui
{
    // off_82F253DC (X360) -- the per-colour "bg" timeline-label names, indexed by
    // ESignColour (the asm tags index 0 == "Green"); SetColour goto-and-plays the
    // "bg" child clip on this label. Recovered from the ESignColour names + the X360
    // "Green" hint (label strings, not byte-uncertain data).
    static const char* const KAPC_SIGN_COLOURS[FlaptRoadSignIconComponent::E_SIGN_COLOUR_COUNT] =
    {
        "Green",   // E_SIGN_COLOUR_GREEN
        "Red",     // E_SIGN_COLOUR_RED
        "Silver",  // E_SIGN_COLOUR_SILVER
        "Gold",    // E_SIGN_COLOUR_GOLD
    };

    // unk_82FB29A0 (X360) -- the per-colour road-text tint, indexed by ESignColour;
    // SetColour pushes KAV4_SIGN_TEXT_COLOURS[meSignColour] into the "RoadText" clip's
    // MovieClipRef::SetColour. The four Vector4 RGBA rows are serialised .rodata that is
    // NOT present in the function export (it lives in the X360 data segment, which we
    // cannot dump here), so the table is declared extern and OWNED by this TU's data
    // home; its float values are filled in when that segment is recovered. Declared (not
    // guessed) so the compile gate resolves the reference without inventing colour data.
    extern const Vector4 KAV4_SIGN_TEXT_COLOURS[FlaptRoadSignIconComponent::E_SIGN_COLOUR_COUNT];

    // off_82F253F0[0] (X360) == "RD_" -- the road-sign label prefix DisplayRoadFromCgsID
    // prepends to the special "EXIT" road name (DWARF mpRoadPrefix, .cpp:48).
    static const char* const KPC_ROAD_PREFIX = "RD_";

    // The EXIT road's CgsID. The X360 builds this 64-bit literal inline
    // (0x684561A1 << 32 | 0x68000000) and compares the incoming road id against it;
    // it is CgsIDCompress("EXIT"). Special-cased because the EXIT sign uses the fixed
    // "RD_EXIT" label rather than a numeric road code.
    static const CgsID KID_ROAD_EXIT = 0x684561A168000000ULL;

    // Scratch buffer length the X360 passes to SPrintf for the formatted road id
    // (32 for the "RD_<name>" path, 31 + explicit NUL for the "%llu" path).
    static const u32 KU_ROAD_NAME_BUFFER_LEN = 32;

    // @ 0x8241CB68 -- bind the state interface and clear every clip/id/flag.
    void FlaptRoadSignIconComponent::Construct(const void* /*lpDEBUGName*/,
                                               CgsGui::StateInterface* lpStateInterface,
                                               const void* /*lpcParentName*/)
    {
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");

        mpStateInterface        = lpStateInterface;
        mAptRef.mpMovieClipInst = 0;
        mAptRef.mpTransform     = 0;

        mv2WorldPos.x = 0.0f;
        mv2WorldPos.y = 0.0f;
        mv2WorldPos.z = 0.0f;
        mv2WorldPos.w = 0.0f;

        mRoadId       = 0;
        meIconType    = E_ICON_TYPE_COUNT;
        meSignColour  = E_SIGN_COLOUR_GREEN;
        mbTimeRuled   = false;
        mbCrashRuled  = false;
        mbSignVisible = false;

        mPostMovieClip.mpMovieClipInst = 0;
        mPostMovieClip.mpTransform     = 0;
    }

    // @ 0x8241CC18 -- bind the named component clip, reset its timeline, then bind the
    // "SignPole" child clip into mPostMovieClip.
    void FlaptRoadSignIconComponent::Prepare(const char* lacName,
                                             const BrnFlapt::FileRef& lFile)
    {
        CGS_ASSERT(lacName != 0, "lacName");
        CGS_ASSERT(lacName != 0, "lacName != NULL");

        lFile.FindComponent(&mAptRef, lacName);

        CGS_ASSERT(mAptRef.mpMovieClipInst != 0, "mpMovieClipInst");

        mAptRef.mpMovieClipInst->ResetTimeline();

        mAptRef.FindChildMovieClip(&mPostMovieClip, "SignPole");
    }

    // @ 0x82427E58 -- play the road clip on lpcRoadName, show the post if present, and
    // make the sign clip visible the first time it is displayed.
    void FlaptRoadSignIconComponent::DisplayRoad(const char* lpcRoadName, bool lbVisible)
    {
        CGS_ASSERT(lpcRoadName != 0, "NULL != lpcRoadName");
        CGS_ASSERT(lpcRoadName[0] != '\0', "'\\0' != lpcRoadName[0]");

        if (mAptRef.mpMovieClipInst != 0)
        {
            mAptRef.GotoAndPlayLabel(lpcRoadName);

            if (mPostMovieClip.mpMovieClipInst != 0)
            {
                mPostMovieClip.SetVisible(lbVisible);
            }

            if (mAptRef.mpMovieClipInst != 0 && !mbSignVisible)
            {
                mAptRef.SetVisible(true);
                mbSignVisible = true;
            }
        }
    }

    // @ 0x8242DBD0 -- format the road id and route to the right DisplayRoad overload.
    // EXIT is special-cased to the fixed "RD_EXIT" label; every other road formats its
    // numeric id, resolves it to an ERoadIcon and uses the icon-id overload.
    void FlaptRoadSignIconComponent::DisplayRoadFromCgsID(CgsID lRoadId, bool lbVisible)
    {
        char lacRoadName[KU_ROAD_NAME_BUFFER_LEN];

        if (lRoadId == KID_ROAD_EXIT)
        {
            CgsCore::SPrintf(lacRoadName, KU_ROAD_NAME_BUFFER_LEN, "%s%s",
                             KPC_ROAD_PREFIX, "EXIT");
            DisplayRoad(lacRoadName, lbVisible);
        }
        else
        {
            CgsCore::SPrintf(lacRoadName, KU_ROAD_NAME_BUFFER_LEN - 1, "%llu", lRoadId);
            lacRoadName[KU_ROAD_NAME_BUFFER_LEN - 1] = '\0';

            ERoadIcon leRoadIcon = FindRoadFromName(lacRoadName);
            DisplayRoad(leRoadIcon, lbVisible);
        }
    }

    // @ 0x82427F30 -- store the sign colour and, if the sign clip is bound, recolour the
    // "bg" clip's label and tint the "RoadText" clip from the colour tables.
    void FlaptRoadSignIconComponent::SetColour(ESignColour leSignColour)
    {
        CGS_ASSERT(static_cast<s32>(leSignColour) >= 0
                       && static_cast<s32>(leSignColour) < E_SIGN_COLOUR_COUNT,
                   "Invalid colour ( ) \n");

        meSignColour = leSignColour;

        if (mAptRef.mpMovieClipInst != 0)
        {
            BrnFlapt::MovieClipRef lBackgroundClip;
            mAptRef.FindChildMovieClipOnFrame(&lBackgroundClip, "bg");
            lBackgroundClip.GotoAndPlayLabel(KAPC_SIGN_COLOURS[meSignColour]);

            BrnFlapt::MovieClipRef lRoadTextClip;
            mAptRef.FindChildMovieClipOnFrame(&lRoadTextClip, "RoadText");
            lRoadTextClip.SetColour(KAV4_SIGN_TEXT_COLOURS[meSignColour]);
        }
    }

    // @ 0x8241CD08 -- find the ERoadIcon whose name matches lpIconName (linear search of
    // the shared road-icon name table); assert if no road matches.
    ERoadIcon FlaptRoadSignIconComponent::FindRoadFromName(const char* lpIconName)
    {
        CGS_ASSERT(lpIconName != 0, "lpIconName");

        s32 liIndex = 0;
        while (liIndex < E_ROADICON_COUNT)
        {
            const char* lpcCandidate = gapcRoadIconNames[liIndex];

            // Inline string compare the X360 folded in: equal up to and including the
            // shared NUL terminator.
            const char* lpcLhs = lpIconName;
            const char* lpcRhs = lpcCandidate;
            s32 liDelta = 0;
            do
            {
                liDelta = static_cast<u8>(*lpcLhs) - static_cast<u8>(*lpcRhs);
                if (*lpcLhs == '\0')
                {
                    break;
                }
                ++lpcLhs;
                ++lpcRhs;
            }
            while (liDelta == 0);

            if (liDelta == 0)
            {
                break;
            }
            ++liIndex;
        }

        CGS_ASSERT(liIndex < E_ROADICON_COUNT,
                   "Unable to find a valid icon name ( looking for )");

        return static_cast<ERoadIcon>(liIndex);
    }
}
