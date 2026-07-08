// BrnChallengeSelector.cpp
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
//   Construct                   @ 0x824158F0 -- base Construct, build the "Text_mc" field,
//                                               clear the challenge/index/pointer/flag state.
//   GetChallengeIndex           @ 0x82415960 -- mapping-table lookup, range-guarded (private).
//   GetAvailableChallengeCount  @ 0x824159E0 -- return miAvailableChallenges.
//   HandleLoadNotification      @ 0x824159E8 -- apt component-load response (re-push text /
//                                               publish transition-in view-state).
//   SetAvailableChallenges      @ 0x8242C270 -- rebuild the available-challenge mapping table.
//
//   SelectAvailableChallengeByID @ 0x82440138 -- resolve a challenge id to its available
//                                               slot and select it (showing the selector).
//
// SelectAvailableChallenge @ 0x82439BA0 is intentionally NOT reconstructed in this TU -- see
// the block note at the bottom of this file. GetAvailableChallengeData is declared-only here.

#include "GameSource/Gui/Flow/Hud/Components/BrnChallengeSelector.h"

#include "SharedClasses/DataLists/ChallengeList.h"        // BrnResource::ChallengeList
#include "SharedClasses/DataLists/ChallengeListEntry.h"   // BrnResource::ChallengeListEntry
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

#include <cstring>   // std::strcmp (the X360 inlines the name compares)

namespace BrnGui
{

// DWARF: extern const char[8] KAC_TEXT_FIELD_NAME (BrnChallengeSelector.cpp:23); "Text_mc".
const char ChallengeSelector::KAC_TEXT_FIELD_NAME[8] = "Text_mc";

// @ 0x824158F0
void ChallengeSelector::Construct(const char* lpacName,
                                  CgsGui::StateInterface* lpStateInterface,
                                  const char* lpacParentName)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

    // The X360 dispatches this through mTextfield's vtable (**(this+0x8C)); semantically it
    // is the embedded field's Construct("Text_mc", mpStateInterface, <this name>).
    mTextfield.Construct(KAC_TEXT_FIELD_NAME, mpStateInterface, GetName());

    mpChallengeList                  = nullptr;   // stw 0, 0x115C
    mpGuiCache                       = nullptr;   // stw 0, 0x1160
    miAvailableChallenges            = 0;         // stw 0, 0x1154
    miCurrentAvailableChallengeIndex = 0;         // stw 0, 0x1158
    mbVisible                        = false;     // stb 0, 0x1165
    mbIsHost                         = false;     // stb 0, 0x1164
}

// @ 0x82415960
s32 ChallengeSelector::GetChallengeIndex(s32 liAvailableChallengeIndex)
{
    CGS_ASSERT(liAvailableChallengeIndex >= 0, "liAvailableChallengeIndex >= 0");
    CGS_ASSERT(liAvailableChallengeIndex < miAvailableChallenges,
               "liAvailableChallengeIndex < miAvailableChallenges");

    return mau16AvailableChallengeIndexToChallengeIndexMapping[liAvailableChallengeIndex];
}

// @ 0x824159E0
s32 ChallengeSelector::GetAvailableChallengeCount()
{
    return miAvailableChallenges;
}

// @ 0x824159E8
// An apt component finished loading; lpacName is its component name.
//   name != this component's name:
//       name == the text field's name -> re-push the field's stored text (SetText(GetText));
//   name == this component's name and the selector is visible:
//       publish the "transinHost"/"transinClient" apt view-state depending on mbIsHost.
void ChallengeSelector::HandleLoadNotification(const char* lpacName)
{
    if (std::strcmp(lpacName, GetName()) != 0)
    {
        if (std::strcmp(lpacName, mTextfield.GetName()) == 0)
        {
            mTextfield.SetText(mTextfield.GetText());
        }
    }
    else if (mbVisible)
    {
        if (mbIsHost)
        {
            AddOutputAptViewState("apt_state", "transinHost", false);
        }
        else
        {
            AddOutputAptViewState("apt_state", "transinClient", false);
        }
    }
}

// @ 0x8242C270
// Rebuild the available-challenge mapping: walk every challenge in the list and keep those
// whose player count (low nibble) matches liNumPlayers AND whose downloadable content has
// been bought, recording each such challenge's list index in the mapping table. Returns the
// resulting available-challenge count.
s32 ChallengeSelector::SetAvailableChallenges(s32 liNumPlayers)
{
    miAvailableChallenges = 0;

    for (s32 liListIndex = 0; liListIndex < mpChallengeList->GetChallengeCount(); ++liListIndex)
    {
        const BrnResource::ChallengeListEntry* lpEntry = mpChallengeList->GetChallengeData(liListIndex);

        if ((lpEntry->GetNumPlayers() & 0xF) == liNumPlayers
            && mpChallengeList->IsChallengeContentBought(liListIndex))
        {
            mau16AvailableChallengeIndexToChallengeIndexMapping[miAvailableChallenges] =
                static_cast<u16>(liListIndex);
            ++miAvailableChallenges;
        }
    }

    return miAvailableChallenges;
}

// @ 0x82440138
// Resolve a challenge id to its full list index; if the challenge exists, show the selector
// (when hidden) and select the matching available slot. When the id is not in the list but a
// select was requested, show the selector and select slot 0.
void ChallengeSelector::SelectAvailableChallengeByID(CgsID lID, bool lbSelect)
{
    CGS_ASSERT(mpChallengeList, "mpChallengeList");   // line 166

    if (mpChallengeList->GetChallengeIndex(lID) < 0)
    {
        if (lbSelect)
        {
            if (!mbVisible)
            {
                Show();
            }
            SelectAvailableChallenge(0, lbSelect);
        }
    }
    else
    {
        if (!mbVisible)
        {
            Show();
        }

        // Re-resolve the id (the X360 recomputes it here) and find its available slot.
        const s32 liChallengeIndex = mpChallengeList->GetChallengeIndex(lID);
        const s32 liCount          = miAvailableChallenges;

        s32 liAvailableIndex = 0;
        for (; liAvailableIndex < liCount; ++liAvailableIndex)
        {
            if (mau16AvailableChallengeIndexToChallengeIndexMapping[liAvailableIndex] == liChallengeIndex)
            {
                break;
            }
        }

        CGS_ASSERT(liAvailableIndex < miAvailableChallenges,
                   "liAvailableIndex < miAvailableChallenges");   // line 184

        SelectAvailableChallenge(liAvailableIndex, lbSelect);
    }
}

// -----------------------------------------------------------------------------
// BLOCKED (not reconstructed in this TU):
//   ChallengeSelector::SelectAvailableChallenge @ 0x82439BA0
//
// SelectAvailableChallenge marshals two parameterised-localised-string calls into
// stack-built parameter-descriptor arrays and forwards them to two collaborators that are
// UNNAMED and UN-HOMED in every export we have:
//   * sub_82866450(languageManager, "CHALLENGE_STRING_DESCRIPTION", &entry.macDescriptionStringID,
//                  9, paramCount, paramBuffers, 11, ...)   -- a CgsLanguage::LanguageManager
//                  variadic "build localised string with parameters" method;
//   * sub_824E7800(&mTextfield, "%1:%2.%3", 0, 3, buf, 11, &entry.macTitleStringID, 9, ...)
//                  -- a BrnGui::TextField variadic "set localised text with parameters" method.
// Hex-Rays reports "local variable allocation has failed, the output may be wrong" for BOTH
// call sites, so their argument counts/types and the { ParameterFormatType==11, value } pair
// layout of the marshalled descriptor arrays cannot be grounded. Reconstructing the variadic
// contract would be a guess, and a wrong parameter marshalling is worse than an honest gap --
// so this function (and SelectAvailableChallengeByID, which tail-calls it) are left declared-
// only until those two collaborators are homed with real signatures.
// -----------------------------------------------------------------------------

} // namespace BrnGui
