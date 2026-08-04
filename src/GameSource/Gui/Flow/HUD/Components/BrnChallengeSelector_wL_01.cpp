// BrnChallengeSelector_wL_01.cpp
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
//   BrnGui::ChallengeSelector::SelectAvailableChallenge @ 0x82439BA0
//
// This is the last body of the BrnChallengeSelector TU and the SOLE definition of this function;
// the rest of the TU is in the sibling BrnChallengeSelector.cpp, which now carries only a
// do-not-redefine marker for it. It was previously parked as BLOCKED because its two variadic
// collaborators were unnamed and un-homed, so the { text, ParameterFormatType } pair marshalling
// could not be grounded. Both are homed now, and the arguments below are read off the caller asm:
//   sub_82866450 = CgsLanguage::LanguageManager::FormatAndAddText(const char*, const char*,
//                  ParameterFormatType, s32, ...)          -- CgsLanguageManager.h:211
//   sub_824E7800 = BrnGui::TextField::SetLocalisedText(const char*, ParameterFormatType,
//                  s32, ...)                               -- BrnGuiTextField.h:94

#include "GameSource/Gui/Flow/Hud/Components/BrnChallengeSelector.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface::GetLanguageManager / OutputGuiEvent
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"     // BrnGui::GuiChallengeSelectedEvent
#include "SharedClasses/DataLists/ChallengeList.h"        // BrnResource::ChallengeList
#include "SharedClasses/DataLists/ChallengeListEntry.h"   // BrnResource::ChallengeListEntry
#include "GameShared/GameClasses/Language/CgsLanguageManager.h" // LanguageManager::FormatAndAddText, ParameterFormatType
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SnPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

namespace BrnGui
{

// The selector/action code THIS function stamps into the posted GuiChallengeSelectedEvent
// (X360 `li r11, 2` @0x82439E10). The sibling code 3 is KI_SELECTOR_ACTION_HIDE in
// BrnChallengeSelector.cpp; both are file-static there/here (internal linkage, no ODR tie).
// Naming precedent + the 0..3 validation range come from the GameBridge case-573 consumer.
static const s32 KI_SELECTOR_ACTION_SELECT = 2;

// The number of `(text, format)` parameter slots the X360 stack-builds for the description
// string: `char lacParamBuffers[4][64]` at sp+0xC0/0x100/0x140/0x180 and a 4-slot format
// array at sp+0x80..0x8F. The "Too Many Params" assert below bounds the fill against it.
static const s32 KI_MAX_DESCRIPTION_PARAMS = 4;

// Per-slot buffer size -- X360 `li r4, 0x40` on both SnPrintf calls into lacParamBuffers.
static const u32 KU_PARAM_BUFFER_SIZE = 64;

// @ 0x82439BA0
// Select the available challenge in slot liAvailableChallengeIndex: resolve it to a
// ChallengeListEntry, build the challenge's parameterised description string (the party
// player count is always %1, then one parameter per challenge action that carries targets),
// register it in the localisation database under "CHALLENGE_STRING_DESCRIPTION", and push
// "<slot+1>:<localised title>.<that description>" into the embedded text field. When
// lbSelect is set, also post a GuiChallengeSelectedEvent with selector action 2.
void ChallengeSelector::SelectAvailableChallenge(s32 liAvailableChallengeIndex, bool lbSelect)
{
    CGS_ASSERT(mpChallengeList, "mpChallengeList");   // line 212 (asm `li r5, 0xD4`)

    const s32 liChallengeIndex = GetChallengeIndex(liAvailableChallengeIndex);

    const BrnResource::ChallengeListEntry* lpChallengeEntry =
        mpChallengeList->GetChallengeData(liChallengeIndex);

    CGS_ASSERT(lpChallengeEntry, "lpChallengeEntry");   // line 215 (asm `li r5, 0xD7`)

    miCurrentAvailableChallengeIndex = liAvailableChallengeIndex;   // stw r31, 0x1158(r27)

    char                                              lacParamBuffers[KI_MAX_DESCRIPTION_PARAMS][KU_PARAM_BUFFER_SIZE];
    CgsLanguage::LanguageManager::ParameterFormatType laeParamFormats[KI_MAX_DESCRIPTION_PARAMS];

    // HOST HYGIENE (not an X360 store): the X360 leaves the format slots it never fills
    // uninitialised and they are never read (liNumParams caps what FormatTextV consumes),
    // but all four are passed by value below, so seed them with the same 11 the X360 writes
    // into every slot it does populate. Cannot diverge -- the unread slots are unread.
    for (s32 liFormatIndex = 0; liFormatIndex < KI_MAX_DESCRIPTION_PARAMS; ++liFormatIndex)
    {
        laeParamFormats[liFormatIndex] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;
    }

    // %1 is always the party player count -- the low nibble of muNumPlayers
    // (asm: lbz r11, 0xD3(r30) / clrlwi r6, r11, 28).
    CgsCore::SnPrintf(lacParamBuffers[0], KU_PARAM_BUFFER_SIZE, "%d",
                      lpChallengeEntry->GetNumPlayers() & 0xF);
    lacParamBuffers[0][KU_PARAM_BUFFER_SIZE - 1] = 0;                                  // stb r24, sp+0xFF
    laeParamFormats[0] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;               // stw r18(=0xB), sp+0x80

    s32 liNumParams = 1;   // li r29, 1 -- the player count already occupies slot 0

    // %2.. are the target values of every action that has targets. muNumActions is re-read
    // from the entry on each iteration (asm re-does `lbz r11, 0xD4(r30)` at the loop tail).
    for (s32 liActionIndex = 0; liActionIndex < lpChallengeEntry->GetNumActions(); ++liActionIndex)
    {
        // The const GetAction is inlined here on the X360 -- its two index guards
        // (ChallengeListEntry.h:941/942 == asm `li r5, 0x3AD` / `0x3AE`) are already
        // reconstructed inside the committed inline, so calling it reproduces them.
        const BrnResource::ChallengeListEntryAction* lpAction =
            lpChallengeEntry->GetAction(liActionIndex);

        if (lpAction->GetNumTargets() != 0)   // lbz r11, -4(r25) == action+0x30
        {
            CGS_ASSERT(liNumParams < KI_MAX_DESCRIPTION_PARAMS,
                       "Too Many Params in Challenge Entry Action");   // line 238 (`li r5, 0xEE`)

            CgsCore::SnPrintf(lacParamBuffers[liNumParams], KU_PARAM_BUFFER_SIZE, "%d",
                              lpAction->GetTargetValue(0));            // lwz r6, 0(r25) == action+0x34
            lacParamBuffers[liNumParams][KU_PARAM_BUFFER_SIZE - 1] = 0;
            laeParamFormats[liNumParams] = CgsLanguage::LanguageManager::E_FORMAT_INTEGER;
            ++liNumParams;
        }
    }

    // Build the localised description and register it under "CHALLENGE_STRING_DESCRIPTION".
    // ALL FOUR (buffer, format) pairs are passed unconditionally -- measured: r8/r9 = pair 0,
    // r10 + the sp+0x54 slot = pair 1, sp+0x5C/0x64 = pair 2, sp+0x6C/0x74 = pair 3. Only the
    // first liNumParams of them are consumed by FormatTextV.
    mpStateInterface->GetLanguageManager()->FormatAndAddText(
        "CHALLENGE_STRING_DESCRIPTION",                                  // r4
        lpChallengeEntry->GetDescriptionStringID(),                      // r5 (entry+0xA0)
        CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,                // r6 = 9
        liNumParams,                                                     // r7
        lacParamBuffers[0], laeParamFormats[0],
        lacParamBuffers[1], laeParamFormats[1],
        lacParamBuffers[2], laeParamFormats[2],
        lacParamBuffers[3], laeParamFormats[3]);

    // The 1-based slot number shown in front of the title. The X360 re-reads the member it
    // just wrote (lwz r11, 0x1158(r27)) rather than reusing the argument.
    char lacIndexBuffer[16];
    CgsCore::SnPrintf(lacIndexBuffer, 16, "%d", miCurrentAvailableChallengeIndex + 1);
    lacIndexBuffer[15] = 0;   // stb r24, sp+0x9F

    // "<slot+1>:<localised title>.<the description just AddText'd above, fetched back by id>".
    mTextfield.SetLocalisedText(
        "%1:%2.%3",                                                      // r4
        CgsLanguage::LanguageManager::E_FORMAT_TEXT,                     // r5 = 0
        3,                                                               // r6
        lacIndexBuffer,                     CgsLanguage::LanguageManager::E_FORMAT_INTEGER,    // r7, r8 = 0xB
        lpChallengeEntry->GetTitleStringID(), CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, // r9 (entry+0xB0), r10 = 9
        "CHALLENGE_STRING_DESCRIPTION",     CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);  // sp+0x54, sp+0x5C = 9

    if (lbSelect)   // clrlwi r11, r16, 24 -- r5 arrived as a bool
    {
        // The X360 inlines StateInterface::OutputGuiEvent<GuiChallengeSelectedEvent> (the
        // out-of-line copy is @0x82436778). MEASURED at 0x82439E04..0x82439E60: it stack-builds
        // a GuiEventWrapper<T,40> at sp+0xA0 -- { 0x10 @+0x00, 0x23D(573) @+0x04, 0x10 @+0x08 },
        // pad word @+0x0C never stored -- followed by the 16-byte payload
        // { mChallengeID, 2, GetChallengeStyle() } @+0x10, then calls
        // mOutEventQueue.AddEvent(&wrapper, /*channel*/40, /*size*/32) (`li r5,0x28` / `li r6,0x20`).
        //
        // KNOWN DIVERGENCE, owned elsewhere -- do NOT "fix" it here or at the call below.
        // GuiChallengeSelectedEvent is one of the RAW payload types (no CgsModule::Event /
        // GuiEvent<N> base; BrnGuiDemangledEventTypes.h:304), and the committed OutputGuiEvent<T>
        // body direct-passes the event rather than wrapping it, so what this queues today is
        // AddEvent(&lEvent, 573, 16) -- not the record measured above. Identical situation to the
        // reviewed Hide in BrnChallengeSelector.cpp. The raw-vs-GuiEvent<N> split is FLAG-owned
        // by CgsGuiStateInterface.h and has to be resolved in that header (and the payload
        // headers it names), never by editing this call site.
        GuiChallengeSelectedEvent lEvent;
        lEvent.mChallengeID     = lpChallengeEntry->GetChallengeID();   // ld r11, 0xC0(r30)
        lEvent.miSelectorAction = KI_SELECTOR_ACTION_SELECT;            // li r11, 2
        // bl 0x823542A0 -- IDB name `BrnResource::ChallengeListEntry::GetChall` is the
        // 41-char-truncated form of GetChallengeStyle (see ChallengeListEntry.h).
        lEvent.miChall          = static_cast<s32>(lpChallengeEntry->GetChallengeStyle());

        mpStateInterface->OutputGuiEvent(lEvent);
    }
}

} // namespace BrnGui
