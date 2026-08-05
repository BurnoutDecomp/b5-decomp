#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"      // class home (+ FastBitArray + EventTypeDefs)

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"           // GuiHudMessage

namespace BrnGui
{

// @ 0x82520488 -- fires the parked developer-challenge message (X360-only sibling).
// Update's deferred pass (@0x825277A8..0x82527814) gates on the flag, calls this, then
// clears both the flag (stb 0 -> 0x4F9) and the bit set (std 0 -> 0x500).
void HudMessageAnalyzer::TriggerDeveloperChallengeMessageDEBUG()
{
    // Non-gating tripwire; the string is verbatim X360 assert rodata (@0x825204B8,
    // cpp:0x176A == 5994) -- it is what names the bool member.
    CGS_ASSERT(mbDEBUGDeveloperChallengeComplete, "mbDEBUGDeveloperChallengeComplete");

    GuiHudMessage lMessage;
    lMessage.Construct("Generic");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, "DEVELOPER CHALLENGE COMPLETE");

    // Index of the FIRST completed challenge. MEASURED: the X360 folds
    // FastBitArray<15>::Begin() + Iterator::GetIndex() inline @0x825204EC..0x82520660
    // (bit-0 fast path; else a cntlzd test that parks an EMPTY set at 15 == End();
    // else a mask<<=1 / ++index walk to the lowest set bit). The two asserts inside
    // that inline are the CONTAINER's own (CgsFastBitArray.h:235 "Attempt to get index
    // when out of range" and h:282 "Internal mask has wrapped - expected to find valid
    // bit"), so per the committed container-header policy their StrStream scaffolding
    // is deliberately NOT reproduced here. Update only reaches this function after
    // Handle @0x824F9D48 parked a non-zero set.
    const s32 liChallengeIndex = mCompletedDeveloperChallenges.Begin().GetIndex();

    // MEASURED @0x82520664: SPrintf's length argument is 10, on a 16-byte stack slot.
    char lacIndexText[16];
    CgsCore::SPrintf(lacIndexText, 10, "%d", liChallengeIndex);

    // Both AddParam calls are the STRING (type 1) overload; the MIDDLE argument is the
    // display-string index (0 then 1), NOT a second param type.
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lacIndexText);

    TriggerMessage(&lMessage);   // bl sub_82517AF8 == TriggerMessage(const GuiHudMessage*)
}

}   // namespace BrnGui
