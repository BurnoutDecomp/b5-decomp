// ===================================================================================
// BrnGui::HudMessageDirector -- implementation (stop-flag / payback setters)
//   class:BrnGui::HudMessageDirector
//
// Reconstructed store-for-store from the X360 ARTIST build. Member-by-name access; the
// stop-flag events are published through mHudMessageQueue (the first member, so the X360
// passes the director `this` straight to VariableEventQueue<18432,16>::AddEvent).
// ===================================================================================
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // gpDebugPrint, gxMessageFilterFlags
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/BrnGuiHudMessageDirector.cpp"
namespace BrnGui
{
    namespace
    {
        // The three queuing functions build a stop-flag event on the stack (a 16-byte record,
        // X360 `addi r4, r1, var_20`), then AddEvent it with type 0x9C and payload size 1. The
        // record bytes are not initialised by these functions (the event carries no payload of
        // its own beyond the size-1 marker), matching the X360.
        struct StopFlagEvent : public CgsModule::Event
        {
            u8 maRecord[16];
        };

        inline void QueueStopFlagEvent(CgsModule::VariableEventQueue<18432, 16>& lrQueue)
        {
            StopFlagEvent lEvent;
            lrQueue.AddEvent(&lEvent, HudMessageDirector::KI_STOP_FLAG_EVENT_TYPE, 1);
        }
    }

    // @ 0x8250C830 -- record the camera-sequence stop flag (stb arg, 0x54D0). When set, log
    // (gated on the message filter) and queue the stop-flag event.
    void HudMessageDirector::SetCameraSequenceFilter(bool lbShowing)
    {
        mbStopFlagCamera = lbShowing;
        if (lbShowing == true)   // ARTIST 0x8250C848 cmplwi r11,1
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "HUD MSGS - D: MSG FILTER SET : CAMERA SEQUENCE IS SHOWING \n\n";
            }
            QueueStopFlagEvent(mHudMessageQueue);
        }
    }

    // @ 0x824EBEF8 -- store the active controller (stw arg, 0x4814) after asserting it is
    // non-null (BrnGuiHudMessageDirector.h:174).
    void HudMessageDirector::SetController(const BrnResource::HudMessageController* lpController)
    {
        CGS_ASSERT(lpController != 0, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid controller passed in to HudMessageDirector::SetController", "..\\..\\..\\GameSource\\Gui/BrnGuiHudMessageDirector.h", 174));

        mpController = lpController;
    }

    // @ 0x8250C8A8 -- begin the aggressor payback sequence (stw 1, 0x54D4). Log (gated) and
    // queue the stop-flag event.
    void HudMessageDirector::StartPaybackAggressor()
    {
        meStopFlagPaybackSequence = E_PAYBACK_AGGRESSOR;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "HUD MSGS - D: MSG FILTER SET : AGGRESSOR SEQUENCE PLAYING \n\n";
        }
        QueueStopFlagEvent(mHudMessageQueue);
    }

    // @ 0x8250C920 -- begin the victim payback sequence (stw 2, 0x54D4). Log (gated) and
    // queue the stop-flag event.
    void HudMessageDirector::StartPaybackVictim()
    {
        meStopFlagPaybackSequence = E_PAYBACK_VICTIM;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "HUD MSGS - D: MSG FILTER SET : VICTIM SEQUENCE PLAYING \n\n";
        }
        QueueStopFlagEvent(mHudMessageQueue);
    }
}

// FOLDED FROM BrnGuiHudMessageDirector_gUI_00.cpp
// ===================================================================================
// BrnGui::HudMessageDirector -- the SEND path (gateui wave partfile 00).
//   AddMessage              @0x825161E8   (cpp:162/163)
//   FilterAndSendOffMessage @0x82511640   (cpp:211/222)
//   CheckMessageIsAvailable @0x824F2B28   (cpp:307)
//   IsMessageAllowed        @0x824F2D30   (cpp:384)
//
// This is the hop between HudMessageAnalyzer::TriggerMessage and the on-screen HUD: the
// analyzer builds a GuiHudMessage on its stack, TriggerMessage hands it to AddMessage, and
// the four functions below decide whether it may show and, if so, publish the whole
// message record onto mHudMessageQueue as event 154. HudMessageDirector::Update (partfile
// 01) drains that queue into the model module's input buffer.
//
// [gateui r2] The two round-1 gaps this file carried are closed elsewhere in the wave:
//   * the director IS constructed and controller-bound now -- BrnGuiModule.cpp owns
//     mHudMessageDirector (X360 GuiModule +639264) and reproduces GuiModule::Construct
//     @0x82518028 lines 277/376 + GuiModule::Update @0x82527A58 line 928 (SetController);
//   * Construct/Update themselves are in BrnGuiHudMessageDirector_gUI_01.cpp.
//
// [gateui r3] THE ROUND-2 PARK THAT USED TO SIT HERE WAS WRONG ON BOTH COUNTS, so here is
// the truth. It said the last rung was blocked because "that component's Construct/Prepare
// TU is unreconstructed": in fact BrnInGameMessagesComponent.cpp already bodied Prepare,
// AddMessage, Update and TerminateMessages, and its Construct call site
// (BrnFBurnMainHudState.cpp OnEnter) plus five private bodies were the only gaps -- all
// landed in round 3, together with the case-154 arm.
//
// WHAT ACTUALLY BLOCKED THIS FILE was one rung EARLIER and is named in
// FilterAndSendOffMessage's own assert below: `mpController` (cpp:222). Nothing in the
// tree produced a BrnResource::HudMessageController, so every message died here. Round 3
// landed the producer -- BrnResource::GameDataModule::PrepareHudMessages @0x8266C8E0
// (Prepare stage 13: "HudMessages.hm" -> pool 11 -> HudMessageController::AddMessages
// @0x8267D580).
//
// [gateui r4] AND THE HAND-OFF IS LANDED TOO. GuiCache::SetHudMessageController -- which
// the console performs with GuiModule::Construct's own `a2` (@0x82518028 pseudocode lines
// 362-368, assert "lpController" BrnGuiCache.h:2405) -- now runs for real:
// BrnGameModule::Construct passes mGameDataModule.GetHudMessageController() into
// BrnGui::GuiModule::Construct, which stores it into the cache immediately before the
// SetHudMessageDirector line. So `mpController` is non-null from the first frame and
// FilterAndSendOffMessage's cpp:222 assert below is a tripwire, not a standing block.
// (The round-3 banner pointed at a "gateui r3 report" that was never written; the
// measurement it referred to is in scratch/gateui_wave/verify_r3_fix3hud/VERDICT.md.)
// ===================================================================================

#include "GameSource/Gui/BrnGuiHudMessageDirector.h"
#include "GameSource/Gui/BrnGuiCache.h"                       // GuiCache (friend access: meGameModeType / miGameFlowState)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"               // GuiHudMessage
#include "SharedClasses/DataLists/BrnHudMessageController.h"  // HudMessageController accessors

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                // CgsIDCompress / CgsIDConvertToString
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // gpDebugPrint, gxMessageFilterFlags
#include <cstdlib>                                              // std::getenv ([hudmsg] switch)

namespace
{
    // [hudmsg] PC witness (BRN_HUDMSG_DIAG): every exit of FilterAndSendOffMessage by name.
}

// The platform high-resolution timer pair (GameShared/GameClasses/System/Timer/
// CgsTimeUtils.cpp @0x828D75A0 / @0x828D75C8). Declared locally, exactly as the two other
// committed consumers do (CgsMoviePlayer.cpp:18-19, CgsDebugManager.cpp:21) -- the TU has
// no owning header in the tree.
// [gateui r2] The 64-BIT pair, which is the console's own return width: both X360 bodies
// end `ld r3, var_10(r1)` (an 8-byte load of the whole LARGE_INTEGER). The rate gate below
// stores and compares these stamps, and the u32 truncation the host used to return wraps
// roughly every 7 minutes at a 10 MHz QPC -- after each wrap `last + wait > now` held for
// minutes and every rate-limited HUD message was silently suppressed. See the banner in
// CgsTimeUtils.cpp for why the u32 entry points still exist.
namespace CgsSystem { u64 GetSystemTimerBaseTime(); u64 GetSystemTimerFrequency(); }

namespace BrnGui
{

// [gateui r2] DWARF h:122 -- the black-bar "is the bar up?" threshold. X360
// SetBlackBarSize @0x82511848 loads flt_82004014 == 0.1f once (`lfs f0, flt_82004014@l(r11)`
// @0x82511868) and compares both the incoming size (f31) and the stored mfBlackBarSize
// (`lfs f13, 0x54CC(r31)`) against it. Declared but NEVER defined before this wave --
// an LNK2001 the day SetBlackBarSize lands (round-1 verify NOTE-4).
const f32 HudMessageDirector::KF_MINIMUM_BLACK_BAR_STOP_SIZE = 0.1f;

// ARTIST @0x82511848. Incoming event 221 carries each bar's normalized height.
void HudMessageDirector::SetBlackBarSize(f32 lfBlackBarSize)
{
    if (lfBlackBarSize < KF_MINIMUM_BLACK_BAR_STOP_SIZE)
    {
        if (!(mfBlackBarSize < KF_MINIMUM_BLACK_BAR_STOP_SIZE))
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint << "HUD MSGS - D: MSG FILTER CLEARED : BLACK BARS ARE HIDDEN\n\n";
            mbStopFlagBlackBar = false;
        }
    }
    else if (mfBlackBarSize < KF_MINIMUM_BLACK_BAR_STOP_SIZE)
    {
        mbStopFlagBlackBar = true;
        const u8 lu8Marker = 0;
        mHudMessageQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lu8Marker), 156, 1);
    }
    mfBlackBarSize = lfBlackBarSize;
}

// ---- the two stop-flag exemption tables (DWARF h:124/:127) -------------------------
// Both are .data blocks that are ZERO in the image and filled by their own dynamic
// initialisers; the initialiser pseudocode gives the strings verbatim. Reproduced as
// dynamically initialised statics, the same treatment as the analyzer's
// KA_SKILLZ_MESSAGE_IDS / KA_STUNT_INFO_MESSAGES.

// @0x82FB5610, initialiser @0x82C55E48 -- the messages that may still show while a payback
// cinematic is playing.
const CgsID HudMessageDirector::KA_PAYBACK_EXEMPT_MSGS[HudMessageDirector::KI_NUM_PAYBACK_EXEMPT_MSGS] =
{
    CgsIDCompress("Crashed"),
    CgsIDCompress("DTScalped"),
    CgsIDCompress("DTGotScalp"),
    CgsIDCompress("TDBdGeneral"),
    CgsIDCompress("PbkBdPayBack"),
    CgsIDCompress("PbkBdSlowDn"),
    CgsIDCompress("PbkBdPowerDn"),
};

// @0x82FB5668, initialiser @0x82C55F00 -- the messages that may still show while the
// black bars are up (the takedown family plus the two jump lines).
const CgsID HudMessageDirector::KA_BLACK_BAR_EXEMPT_MSGS[HudMessageDirector::KI_NUM_BLACK_BAR_EXEMPT_MSGS] =
{
    CgsIDCompress("TDGdGeneral"),
    CgsIDCompress("TDBdGeneral"),
    CgsIDCompress("TDGdVert"),
    CgsIDCompress("TDGdTBone"),
    CgsIDCompress("TDGdAfter"),
    CgsIDCompress("TDGdTraffCh"),
    CgsIDCompress("TDGdPsyche"),
    CgsIDCompress("TDGdDouble"),
    CgsIDCompress("TDGdCar"),
    CgsIDCompress("TDGdVan"),
    CgsIDCompress("TDGdBus"),
    CgsIDCompress("TDGdBigRig"),
    CgsIDCompress("StuntPartJmp"),
    CgsIDCompress("SuperJump"),
};

namespace
{
    // Both stop-flag scans are the same linear "is this id exempt?" search; the X360
    // emitted the 14-entry one unrolled by two (checking table[2n] and table[2n+1] per
    // iteration) and the 7-entry one straight, but both fall out of the loop with the
    // index == the table length when the id is absent, which is what the callers test.
    bool IsIdInTable(const CgsID* lpTable, s32 liCount, CgsID lId)
    {
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            if (lpTable[liIndex] == lId)
                return true;
        }
        return false;
    }
}

// @ 0x824F2D30
// Three stop flags, in the console's order. NOTE the black-bar arm is an ALLOW-LIST test,
// not a block test: while the bar is up everything EXCEPT the fourteen exempt messages is
// suppressed (`if (index == 14) return false`), and likewise for the payback arm.
bool HudMessageDirector::IsMessageAllowed(CgsID lMessageId) const
{
    // Non-gating tripwire (cpp:384).
    CGS_ASSERT_AT(lMessageId != static_cast<CgsID>(0), "kCGSID_NULL != lMessageID", CGS_ARTIST_FILE, 384);

    if (mbStopFlagBlackBar)
    {
        if (!IsIdInTable(KA_BLACK_BAR_EXEMPT_MSGS, KI_NUM_BLACK_BAR_EXEMPT_MSGS, lMessageId))
            return false;
    }

    if (mbStopFlagCamera)
        return false;

    if (meStopFlagPaybackSequence == E_PAYBACK_NONE)
        return true;

    return IsIdInTable(KA_PAYBACK_EXEMPT_MSGS, KI_NUM_PAYBACK_EXEMPT_MSGS, lMessageId);
}

// @ 0x824F2B28
// Test one message's availability bitset against the cache's live context. Bit 3 (0x08) is
// "allowed offline", bit 4 (0x10) "allowed online", bit 5 (0x20) "allowed while crashed"
// -- the three masks the X360 tests (`andi. 8` / `andi. 0x10` / `andi. 0x20`), named for
// the log line each failure prints.
//
// ⚠ FAITHFUL ODDITY: game mode 1 is NOT in the offline case list (the X360 jump table
// covers -1, 0, 2..9 offline and 10..17 online), so mode 1 falls into the "invalid or
// unsupported game mode" arm and every message is suppressed. Reproduced as written.
bool HudMessageDirector::CheckMessageIsAvailable(u32 luAvailabilityBitSet) const
{
    CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 307);   // cpp:307

    const s32 leGameModeType = mpGuiCache->meGameModeType;

    const bool lbOffline = (leGameModeType == -1) ||
                           (leGameModeType == 0)  ||
                           (leGameModeType >= 2 && leGameModeType <= 9);
    const bool lbOnline  = (leGameModeType >= 10 && leGameModeType <= 17);

    if (!lbOffline && !lbOnline)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "!!!ERROR!!! : Invalid or unsupported game mode : " << leGameModeType
                << " \nin HudMessageDirector::CheckMessageIsAvailable() \n";
        }
        return false;
    }

    if (lbOffline && (luAvailabilityBitSet & KU_AVAILABLE_OFFLINE) == 0)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint << "Not available offline : ";
        return false;
    }

    if (lbOnline && (luAvailabilityBitSet & KU_AVAILABLE_ONLINE) == 0)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint << "Not available online : ";
        return false;
    }

    // The shared tail: state 0 and state 2 are the crashed states (the X360 tests
    // `flow && flow != 2`), and only messages carrying the crash bit survive them.
    const s32 liGameFlowState = mpGuiCache->miGameFlowState;
    if ((liGameFlowState != 0 && liGameFlowState != 2) ||
        (luAvailabilityBitSet & KU_AVAILABLE_WHILE_CRASHED) != 0)
    {
        return true;
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        *CgsDev::Log::gpDebugPrint << "Not available while crashed\n";
    return false;
}

// @ 0x82511640
// Resolve the message, apply the availability + repeat-rate + stop-flag gates, then
// publish the whole message record. Returns whether it was published.
bool HudMessageDirector::FilterAndSendOffMessage(const GuiHudMessage* lpMessage, bool lbForce)
{
    // Non-gating tripwire (cpp:211).
    CGS_ASSERT(lpMessage != NULL, "Invalid message to send");

    // The X360 stringises the id up front purely so the not-found log line below can name
    // it (`CgsIDConvertToString(msg->mMessageIdHash, var_78)` @0x82511710, before the
    // controller assert).
    char lacMessageId[24];
    CgsIDConvertToString(lpMessage->mMessageIdHash, lacMessageId);

    CGS_ASSERT_AT(mpController != 0, "mpController", CGS_ARTIST_FILE, 222);   // cpp:222

    const s32 liIndex = mpController->GetIndexFromMessageHash(lpMessage->mMessageIdHash);
    if (liIndex == -1)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint << "Unable to find message : " << lacMessageId << "\n";
        return false;
    }

    if (!lbForce)
    {
        if (!CheckMessageIsAvailable(mpController->GetMessageAvailabilityBitset(liIndex)))
        {
            return false;
        }

        // The per-message repeat rate: mfTimeToWait SECONDS converted to timer ticks
        // against the platform counter frequency (the X360 does the multiply in double
        // precision then `fctidz` to a 64-bit tick count, and compares unsigned 64-bit).
        const f32 lfTimeToWait = mpController->GetMessageTimeToWaitFromIndex(liIndex);
        const u64 lu64WaitTicks = static_cast<u64>(
            static_cast<f64>(CgsSystem::GetSystemTimerFrequency()) * static_cast<f64>(lfTimeToWait));

        // 64-bit throughout, matching the console's ldx/stdx/cmpld at 0x825117E4..0x825117F0.
        if (mauMessagesLastTriggered[liIndex] + lu64WaitTicks > CgsSystem::GetSystemTimerBaseTime())
        {
            return false;
        }
    }

    if (!IsMessageAllowed(lpMessage->mMessageIdHash))
    {
        return false;
    }

    mauMessagesLastTriggered[liIndex] = CgsSystem::GetSystemTimerBaseTime();

    // `li r5, 0x9A; li r6, 0x348` -- event type 154, size 840 == the X360 sizeof
    // (GuiHudMessage). The host record is wider, so the size rides sizeof rather than the
    // console literal (semantic parity: the queue must carry the WHOLE message).
    mHudMessageQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(lpMessage),
                              KI_HUD_MESSAGE_EVENT_TYPE,
                              static_cast<s32>(sizeof(GuiHudMessage)));
    return true;
}

// @ 0x825161E8
// The public entry point: two tripwires, then the filter. (The X360 tail-calls
// FilterAndSendOffMessage and so returns its bool in r3, but the DWARF declares AddMessage
// `void` -- rung 2 decides the declaration shape, so the result is dropped here.)
void HudMessageDirector::AddMessage(const GuiHudMessage* lpMessage, bool lbForce)
{
    CGS_ASSERT(lpMessage != NULL, "Message is invalid");   // cpp:162
    CGS_ASSERT_AT(mpController != 0, "mpController", CGS_ARTIST_FILE, 163);         // cpp:163

    FilterAndSendOffMessage(lpMessage, lbForce);
}

} // namespace BrnGui

// FOLDED FROM BrnGuiHudMessageDirector_gUI_01.cpp
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
// CP16 supplies both real providers. The caller below uses its actual bound
// Model owner, preserving the original lock/publish/clear order.
//
// ⚠ mfBlackBarSize is deliberately NOT initialised here: the X360 Construct writes
// +0x54C8, +0x54D0 and +0x54D4 and skips +0x54CC. Reproduced as written.
// ===================================================================================

#include "GameSource/Gui/BrnGuiHudMessageDirector.h"
#include "GameSource/Gui/BrnGuiCache.h"                        // GuiCache (friend access: mbGameplayHudActive)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                // GuiHudMessage (mCachedMessage)
#include "GameShared/GameClasses/Gui/Model/CgsModelModule.h"
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
    // ARTIST82508FA0: retain the original owner diagnostic and continuation.
    if (!lpModelModule)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpModelModule",
            CGS_ASSERT_LOCATION("..\\..\\..\\GameSource\\Gui/BrnGuiHudMessageDirector.cpp", 80));
        CgsDev::Assert::EndAssert();
    }
    CGS_ASSERT_AT(lpGuiCache != 0, "lpGuiCache", CGS_ARTIST_FILE, 81);         // cpp:81

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
        mpModelModule->AddGuiEvents(&mHudMessageQueue, lpModelInputBuffer);
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
            static s32        siPrinted    = 0;
            const s32         KI_DIAG_MAX  = 32;
        }

        mHudMessageQueue.Clear();
    }
}

} // namespace BrnGui
