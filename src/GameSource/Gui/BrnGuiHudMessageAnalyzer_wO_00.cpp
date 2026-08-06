#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"      // class home (+ FastBitArray + EventTypeDefs)

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"           // GuiDeveloperChallengesCompleted (after request B)

namespace BrnGui
{

// @ 0x824F9D48 -- Update's case-596 drain target (X360-only sibling; no PS3-DWARF row).
// Update @0x82525FC0 calls it @0x825275D4 with the raw queue record in r4.
void HudMessageAnalyzer::HandleDeveloperChallengeMessageDEBUG(
        const GuiDeveloperChallengesCompleted* lpDeveloperChallengeEvent)
{
    // Non-gating tripwires. MEASURED: both strings and both line numbers are verbatim
    // X360 assert rodata (@0x824F9D78 cpp:0x1755 == 5973, @0x824F9DC4 cpp:0x1756 == 5974) --
    // that rodata is where the parameter name and the event's member name come from.
    CGS_ASSERT(lpDeveloperChallengeEvent != NULL, "lpDeveloperChallengeEvent");

    // @0x824F9D88..0x824F9DB4 is FastBitArray<15>::IsZero() folded inline: a ldx/cmpldi
    // scan over the bit fields (one, for <15>), with the assert firing when the scan
    // completes without finding a non-zero field (r11 = 1 at 0x824F9DA8 -> fire;
    // the early bne at 0x824F9D98 lands on r11 = 0 -> skip).
    CGS_ASSERT(!lpDeveloperChallengeEvent->mCompletedDeveloperChallenges.IsZero(),
               "!lpDeveloperChallengeEvent->mCompletedDeveloperChallenges.IsZero()");

    // Park the flag and OR-accumulate the event's completed-challenge bits.
    // MEASURED @0x824F9DD4..0x824F9DE8: li r11,1 / stb r11,0x4F9(r30), then
    // ld 0x500(r30), ld 0(r31), or, std 0x500(r30) -- the inlined SetOr over the one
    // u64 field. The console offsets 0x4F9 / 0x500 are COMMENTARY: both members are
    // reached by name, so the host layout stays host-correct.
    mbDEBUGDeveloperChallengeComplete = true;
    mCompletedDeveloperChallenges.SetOr(mCompletedDeveloperChallenges,
                                        lpDeveloperChallengeEvent->mCompletedDeveloperChallenges);
}

}   // namespace BrnGui
