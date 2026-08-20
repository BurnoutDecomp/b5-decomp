// ===================================================================================
// BrnGui::HudMessageDirector -- LIFECYCLE + the per-frame DRAIN (gateui wave partfile 01).
//   Construct @0x82508FA0   (cpp:80/81)
//   Update    @0x82516100
//
// These are the two bodies that make the send path in partfile 00 actually reachable and
// actually leave the director. Round 1 landed AddMessage/FilterAndSendOffMessage/
// CheckMessageIsAvailable/IsMessageAllowed but nothing constructed the object and nothing
// drained mHudMessageQueue, so (a) the repeat-rate table mauMessagesLastTriggered was read
// uninitialised on the first hit of every index (the never-Constructed family from the
// wave preamble's checklist, one level up from a queue) and (b) every message published as
// event 154 stayed in the 18432-byte queue for ever -- ~21 messages to overflow.
//
// WHERE THE QUEUE GOES. Update hands mHudMessageQueue to the model module's INPUT buffer.
// The console spells that as two pure forwarders:
//     ModelModule::AddGuiEvents(mpModelModule, &mHudMessageQueue, lpModelInputBuffer)
//         @0x8285DF50 -- never touches its own `this`; asserts the buffer non-null
//         ("Not locked for input", CgsModelModule.cpp:373) then tail-calls
//     ModelIO::InputBuffer::AddGuiEvents(lpModelInputBuffer, &mHudMessageQueue)
//         -- asserts the WRITE lock, then
//     mGuiEvents.Append<18432,16>(mHudMessageQueue)   (the instantiation @0x823C80E0,
//         CgsModule::VariableEventQueue<32768,16>::Append<18432,16>)
// Neither forwarder has a body in this tree yet, and both live outside this wave's
// ownership (GameShared/GameClasses/Gui/**), so the call below is the DE-INLINED form:
// GetEventQueueNonConst() @0x8284F898 carries the identical write-lock assert and returns
// the identical this+4 queue handle, so the observable sequence (assert write lock ->
// Append -> Clear) is unchanged. shared_header_request in the report: declare/body
// ModelIO::InputBuffer::AddGuiEvents and route through it.
//
// ⚠ mfBlackBarSize is deliberately NOT initialised here: the X360 Construct writes
// +0x54C8, +0x54D0 and +0x54D4 and skips +0x54CC. Reproduced as written.
// ===================================================================================

#include "GameSource/Gui/BrnGuiHudMessageDirector.h"
#include "GameSource/Gui/BrnGuiCache.h"                        // GuiCache (friend access: mbGameplayHudActive)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                // GuiHudMessage (mCachedMessage)
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h" // CgsGui::ModelIO::InputBuffer

#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT

#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // gpDebugPrint ([DIAG] BRN_PROP_DIAG)
#include <stdlib.h>                                            // getenv    ([DIAG] BRN_PROP_DIAG)
#include <string.h>                                            // memset (the console's own call)

namespace BrnGui
{

// @ 0x82508FA0
// Store map, in the console's order (offsets from the X360 body):
//   +0x4810 mpModelModule = lpModelModule                     (assert cpp:80)
//   +0x4818 mpGuiCache    = lpGuiCache                        (assert cpp:81)
//   +0x4814 mpController   = 0
//   +0x481C mbLastFrameHudMessageState = 0
//   +0x481D mbMessagePending           = 0
//   +0x4B68 mauMessagesLastTriggered[300] = 0   (the `v10 = 300; do { *v9++ = 0; }` loop)
//   +0x0000 mHudMessageQueue.Construct(); mHudMessageQueue.Clear();
//   +0x54C8 mbStopFlagBlackBar        = 0
//   +0x54D0 mbStopFlagCamera          = 0
//   +0x54D4 meStopFlagPaybackSequence = E_PAYBACK_NONE
void HudMessageDirector::Construct(CgsGui::ModelModule* lpModelModule,
                                   const GuiCache* lpGuiCache)
{
    CGS_ASSERT(lpModelModule != 0, "lpModelModule");   // cpp:80
    CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");         // cpp:81

    mpModelModule = lpModelModule;
    mpGuiCache    = lpGuiCache;
    mpController  = 0;

    mbLastFrameHudMessageState = false;
    mbMessagePending           = false;

    for (s32 liIndex = 0; liIndex < 300; ++liIndex)
        mauMessagesLastTriggered[liIndex] = 0u;

    mHudMessageQueue.Construct();
    mHudMessageQueue.Clear();

    mbStopFlagBlackBar        = false;
    mbStopFlagCamera          = false;
    meStopFlagPaybackSequence = E_PAYBACK_NONE;
}

// @ 0x82516100
void HudMessageDirector::Update(CgsGui::ModelIO::InputBuffer* lpModelInputBuffer)
{
    // ---- 1. the deferred ("wait until the gameplay HUD is up") message ---------------
    // The X360 reads the gate byte twice off the cache (`lwz r11, 0x4818(r31)` then
    // `lbz r11, 0x407C(r11)`), so it is re-read after the send arm.
    if (mpGuiCache->mbGameplayHudActive
        && !mbLastFrameHudMessageState
        && mbMessagePending
        && mpController != 0)
    {
        // `li r5, 0; addi r4, r31, 0x4820` -- the CACHED message, not forced.
        FilterAndSendOffMessage(&mCachedMessage, false);
    }

    mbLastFrameHudMessageState = mpGuiCache->mbGameplayHudActive;

    if (mbMessagePending && mpGuiCache->mbGameplayHudActive)
    {
        // `li r5, 0x348` -- the X360 sizeof(GuiHudMessage). The host record is wider, so
        // the wipe rides sizeof (semantic parity: clear the WHOLE cached message).
        memset(&mCachedMessage, 0, sizeof(mCachedMessage));
        mbMessagePending = false;
    }

    // ---- 2. drain the published messages into the model module's input buffer --------
    const s32 liLength = mHudMessageQueue.GetLength();
    if (liLength != 0)
    {
        lpModelInputBuffer->LockForWrite();
        // See the banner: the console reaches this through ModelModule::AddGuiEvents ->
        // ModelIO::InputBuffer::AddGuiEvents; both are pure forwarders onto this Append,
        // and GetEventQueueNonConst carries the same write-lock assert.
        lpModelInputBuffer->GetEventQueueNonConst()->Append(mHudMessageQueue);
        lpModelInputBuffer->UnlockForWrite();

        // [DIAG] NOT IN THE X360 BINARY. The final rung of the gateui ladder (set
        // BRN_PROP_DIAG -- one knob for the whole chain). This is the point the director
        // hands its messages to the model/view layer.
        // [gateui r3] The round-2 text here claimed the next hop was "still a Slice-B
        // deferral in BrnFBurnMainHudState::RecvEvent case 154". IT IS NOT, and it was not
        // then either: BrnFBurnMainHudState::UpdateRunning case 154 now calls
        // InGameMessagesComponent::AddMessage @0x8243DC68 for real, the component is
        // Constructed + Prepared in OnEnter, and its five missing private bodies
        // (Construct/SetIcon/RefreshString/SendGameMessage/StartMessage/EndMessage) landed
        // in round 3. Two things gated the Apt draw, and [gateui r4] BOTH ARE NOW CLOSED:
        //   * GuiCache::mpHudMessageController had no writer, so the send path one file over
        //     (FilterAndSendOffMessage's cpp:222 assert) rejected every message. The PRODUCER
        //     landed in round 3 (GameDataModule::PrepareHudMessages) and the HAND-OFF landed
        //     in round 4: BrnGui::GuiModule::Construct now takes the console's `a2`
        //     (@0x82518028) from BrnGameModule::Construct and stores it through
        //     GuiCache::SetHudMessageController.
        //   * the shared InGameMessagesQueue -- a by-value BrnGui::GuiCache member at guest
        //     +0x4080 -- is homed as BrnGuiCache.h :: mInGameMessagesQueue with a
        //     GetInGameMessagesQueue() accessor, and FBurnMainHudState::OnEnter runs the
        //     console's SetInGameMessagesQueue against it.
        // (The round-3 banner pointed at a "gateui r3 report" that was never written; the
        // measurement behind these two items is in
        // scratch/gateui_wave/verify_r3_fix3hud/VERDICT.md.)
        // First-N guarded (the latch is evaluated ONCE) so a busy HUD cannot flood the log.
        {
            static const bool sbUiGateDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
            static s32        siPrinted    = 0;
            const s32         KI_DIAG_MAX  = 32;
            if (sbUiGateDiag && CgsDev::Log::gpDebugPrint != 0 && siPrinted < KI_DIAG_MAX)
            {
                ++siPrinted;
                *CgsDev::Log::gpDebugPrint
                    << "[UI-gate] hud message sent n=" << liLength << "\n";
            }
        }

        mHudMessageQueue.Clear();
    }
}

} // namespace BrnGui
