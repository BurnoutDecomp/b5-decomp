// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_EventStartsGuiEvents.cpp
//
// ⭐⭐ [event-starts producer wave 2026-08-27] THE EVENT-START TABLE HOP.
//
// The event-start slice of BrnGame::BrnGameModule::BridgeGameStateToGui @0x823EE880 -- the ONE
// console producer of GUI event 203, and the middle link of a three-link chain that is useless
// unless all three are present:
//
//     GameStateModule::SendSetUpAllEventStartsMessage @0x823759D0   (the table)
//         -> OutputBuffer::mSetUpAllEventStartsInterface + its valid flag
//         -> THIS (GuiEventUpdateEventStarts, id 203, size 8416)
//         -> GuiCache::RecEvent @0x8250DDF0 case 203  (the cache's copy at GuiCache+0x5690)
//         -> GuiCache::GetProfileEventDisplayInfo / GetPresetEventDisplayInfo
//         -> SatNavRenderer::RefreshSatNavIconInfo   (the EVENT ICONS on the map)
//
// THE CONSOLE ARM, instruction for instruction (r15 == lpGameStateOutput, r24 == the embedded
// CgsGui::GuiModule, r19 == the GUI input buffer):
//     0x823EF1A4  bl GetSetUpAllEventStartsInterfaceIsValid
//     0x823EF1B4  beq -> skip
//     0x823EF1B8  addis r4, r15, 3 / addi r4, r4, -0x4F10   ; src = out + 0x2B0F0 (176368)
//     0x823EF1C0  li r5, 0x20E0                             ; 8416 bytes
//     0x823EF1C8  addi r3, r1, var_2180 ; bl memcpy         ; onto a bare stack local
//     0x823EF1DC  bl AddGuiEvent<GuiEventUpdateEventStarts> ; which queues sizeof(T) from +0
// -- note there is NO GuiEvent header built in front of the copy. The queued record IS the
// interface; see the GuiEventUpdateEventStarts banner in BrnGuiEventTypeDefs.h for why the old
// opaque placeholder's 12-byte-header assumption was wrong.
//
// ⚠️ THE COPY IS MEMBER-WISE HERE, not a byte blit, for the reason GuiCache::RecEvent's case-207
// arm already states: a raw memcpy silently couples two layouts. Both ends are the SAME type
// (SetUpAllEventStartsInterface), so this is a plain assignment of the named member -- which the
// compiler will render as the console's memcpy anyway, and which cannot drift.
//
// ⛔ WHY A SIBLING TU, and it is the SAME reason GameBridgeGameStateToX_EventStatusGuiEvents.cpp
// and _StuntGuiEvents.cpp give: the DWARF home GameBridgeGameStateToX.cpp compiles but CANNOT BE
// MOUNTED (six symbols in its other bodies have no definition anywhere in src). Landing this
// slice there would make it unreachable. Folding all of them back once those six are homed is a
// delete, not a duplicate-symbol hunt.
//
// ⓘ THE VALID FLAG IS NOT CLEARED HERE, and that is the console's shape, not an oversight:
// SetSetUpAllEventStartsInterfaceIsValid(true) is a latch the producer raises once (its one-shot
// PreWorldUpdate latch), and the console's OutputBuffer is re-Constructed by the module scheduler
// every frame -- which is what makes the post one-shot there. On PC the buffer is persistent
// (BrnGameStateModule.cpp:68, itself a flagged bring-up seam), so this arm re-posts the table
// every GUI frame. That is IDEMPOTENT -- RecEvent's case-203 arm is a whole-table overwrite, not
// an append -- but it is 8416 bytes of queue traffic per frame, so the re-post is suppressed
// below on a copy of the table's own length, which only ever changes when the producer re-runs.
// DELETE-WHEN the module's real Prepare/Swap DataStructure path lands and the flag goes back to
// being naturally one-shot.
// ============================================================================

#include "GameSource/Game/GameBridgeGameStateToX.h"          // BrnGame::PushGuiEvent<T>
#include "GameSource/GameState/BrnGameStateModuleIO.h"       // OutputBuffer + the interface accessor
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // SetUpAllEventStartsInterface
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"              // BrnGui::GuiEventUpdateEventStarts (id 203)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"       // CgsGui::CgsGuiModuleIO::InputBuffer
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] the one-shot publish line

namespace BrnGame
{

// ============================================================================
// X360 0x823EE880, the arm at 0x823EF1A0..0x823EF1DC. See the banner.
// ============================================================================
void BridgeGameStateToGui_EventStarts(
        const BrnGameState::GameStateModuleIO::OutputBuffer* lpGameStateOutput,
        CgsGui::CgsGuiModuleIO::InputBuffer*                 lpGuiInput)
{
    if (lpGameStateOutput == 0 || lpGuiInput == 0)
    {
        return;
    }

    // `bl GetSetUpAllEventStartsInterfaceIsValid ; beq` -- the console's whole gate.
    if (!lpGameStateOutput->GetSetUpAllEventStartsInterfaceIsValid())
    {
        return;
    }

    const BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface& lrInterface =
        lpGameStateOutput->GetSetUpAllEventStartsInterface();

    // ⚠️ [FLAG PC bring-up] NOT IN THE X360 BINARY -- the re-post suppressor. See the ⓘ in the
    // banner: the console's per-frame OutputBuffer Construct makes this post naturally one-shot;
    // this build's persistent buffer would otherwise re-post 8416 bytes every GUI frame forever.
    // Keyed on the table's own length because the producer is itself one-shot, so the length is
    // the only thing that can change (0 -> N once), and a length of 0 is never posted at all --
    // a zero-length table carries no information the consumer does not already have, and posting
    // it would overwrite nothing with nothing.
    // Held as a file static rather than a bridge member because this bridge has no object (the
    // slice is a free function over the two buffers), the same shape the StuntManager's
    // district-map retry budget uses. DELETE-WHEN the module's Prepare/Swap path lands.
    static u32 suLastPostedCount = 0;
    const u32  luCount           = lrInterface.GetNumEventStarts();
    if (luCount == 0 || luCount == suLastPostedCount)
    {
        return;
    }
    suLastPostedCount = luCount;

    // `memcpy(local, out + 0x2B0F0, 0x20E0)` -- member-wise, see the banner's ⚠️.
    BrnGui::GuiEventUpdateEventStarts lEvent;
    lEvent.mEventStarts = lrInterface;

    // `bl AddGuiEvent<GuiEventUpdateEventStarts>` (id 203, size 8416).
    PushGuiEvent(lEvent, lpGuiInput);

    // [DIAG] NOT IN THE X360 BINARY -- the line that proves the hop ran, once per new table.
    // DELETE-WHEN the event-icon path has a regression test behind it.
    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[event-starts] posted GUI event 203 with " << static_cast<s32>(luCount)
            << " event starts\n";
    }
}

} // namespace BrnGame
