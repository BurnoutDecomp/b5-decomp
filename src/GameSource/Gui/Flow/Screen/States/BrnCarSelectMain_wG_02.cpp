// ===================================================================================
// BrnGui::CarSelectMain -- wave-G partfile 02: the two online-overlay handlers.
//   HandleLaunchedEvent  @0x824C8EF0  (in-queue event 58)
//   HandleLeftGameEvent  @0x824C91C8  (in-queue event 273)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm). Both
// bodies build the same out-queue overlay records the rest of the SCREEN flow posts:
// the 304-byte GuiOverlayRequest wire (id 184) and the 24-byte GuiOverlayWaitFinish
// wire (id 188), published on channel 40 through
// mpStateInterface->GetOutputEventQueue()->AddEvent(). The X360 reaches those records
// either by inlining CgsGui::StateInterface::OutputGuiEvent<T> (HandleLaunchedEvent's
// construct-then-memcpy at 0x824C8FC0) or by calling the out-of-line instantiation
// (HandleLeftGameEvent's kick overlay at 0x824C93F4); both produce the identical
// { sizeof(payload), <event id>, 16, <pad>, payload } record on channel 40, so the
// reconstruction posts the wire record directly at every site (the committed
// CgsGui::StateInterface::OutputGuiEvent template in CgsGuiStateInterface.h queues the
// bare payload under channel == GetEventType() instead, which is NOT what the X360
// instantiation does -- it is not called here, and its header is frozen this wave).
//
// The kick-reason overlay table @0x82F26774 was read out of the image (see
// KAPC_KICK_REASON_OVERLAYS below); KPC_LAUNCH_FAILED_STRINGIDS @0x82F26C94 is the
// class static declared in the header and defined in BrnCarSelectMain_wG_01.cpp.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectMain.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase (debug print)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayRequest / GuiFlow
#include "GameSource/Gui/BrnGuiOverlaysDirector.h"                        // GuiOverlayWaitFinishRequest (188 payload)

namespace BrnGui
{
    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT = 40;   // GuiEventOut (the AddEvent channel both sites use)

        // ---- in-queue payload views (the queue delivers the header-stripped payload;
        //      same idiom as BrnInGame.cpp's) -------------------------------------------

        // Event 58 (online "launched") payload: the X360 reads one word at +0x00 and uses
        // it both as the failure test and as the KPC_LAUNCH_FAILED_STRINGIDS index.
        struct GuiEventLaunchedPayload : public CgsModule::Event
        {
            s32 miResult;        // +0x00  (0 == launched OK; 1..6 select the failure string)
        };

        // Event 273 ("left game") payload: the reason word at +0x00 and, for the kicked
        // reason, the kick sub-reason at +0x04 (compared SIGNED against 0 and 5 by the
        // X360 assert -- Hex-Rays renders it as an unsigned `> 4u`, the asm is blt/blt).
        struct GuiEventLeftGamePayload : public CgsModule::Event
        {
            s32 miReason;        // +0x00
            s32 miKickReason;    // +0x04  (index into KAPC_KICK_REASON_OVERLAYS)
        };

        // ---- out-queue wire records (same shape as BrnInGame.cpp's / BrnPauseScreen's) --

        // The OutputGuiEvent<BrnGui::GuiOverlayRequest> record: { 288, 184, 16, <pad>, the
        // 288-byte request }, channel 40, 304 bytes. The header words are host expressions
        // (sizeof / alignment), NOT the console's baked 0x120 / 0x130 immediates.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C (payload is 16-aligned past the header)
            GuiOverlayRequest mRequest;   // +0x10
            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0) {}
        };

        // The OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> record:
        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes.
        struct GuiOverlayWaitFinishWire : public CgsGui::GuiEvent<188>
        {
            u32                         muPad0C;    // +0x0C (8-aligned payload)
            GuiOverlayWaitFinishRequest mRequest;   // +0x10
            GuiOverlayWaitFinishWire()
                : CgsGui::GuiEvent<188>(static_cast<u32>(sizeof(GuiOverlayWaitFinishRequest)), 16)
                , muPad0C(0) {}
        };

        // ---- the left-game reason / kick-reason vocabularies ------------------------

        // *a2 (the reason word) values this handler acts on. FLAG: the producer-side enum
        // name is not in the recovered DWARF slice; the roles come from the overlay ids.
        const s32 KI_LEFT_GAME_REASON_KICKED       = 2;   // -> the leave questions + kick overlay
        const s32 KI_LEFT_GAME_REASON_LOBBY_DELETED = 3;  // -> "CNLobbyDlted"

        // Kick-reason -> overlay id. Read from the image: off_82F26774 is a POINTER table of
        // 5 entries (0x82F26774..0x82F26784, the next qwords are zero), chased to their
        // strings -- { "OnKicked", "OnKickedGame", "OnKicked", "OnKickedComm", "OnKickedLost" }
        // (entries 0 and 2 share the same "OnKicked" literal @0x8205CBE0). FLAG: the table's
        // ORIGINAL name is not in the recovered DWARF slice; the assert bound (5) is the
        // X360's own `kick >= 5` test.
        const s32 KI_KICK_REASON_COUNT = 5;
        const char* const KAPC_KICK_REASON_OVERLAYS[KI_KICK_REASON_COUNT] =
        {
            "OnKicked",       // 0  @0x8205CBE0
            "OnKickedGame",   // 1  @0x8205CBD0
            "OnKicked",       // 2  @0x8205CBE0 (same literal as 0)
            "OnKickedComm",   // 3  @0x8205CBC0
            "OnKickedLost",   // 4  @0x8205CBB0
        };
    }

    // ---- HandleLaunchedEvent @ 0x824C8EF0 -----------------------------------------
    // In-queue event 58. On a non-zero launch result, raise the "CNOnlLchFail" overlay
    // carrying the result's ONLINE_LAUNCHED_FAILED_* string id as message param 2.
    void CarSelectMain::HandleLaunchedEvent(const CgsModule::Event* lpLaunchedEvent)
    {
        // cpp:686. The X360 streams this text into the assert buffer; the message is the
        // ORIGINAL copy-paste from OnlineGameRoomPlayerInfo (kept verbatim).
        CGS_ASSERT(lpLaunchedEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleLaunchedEvent");

        const GuiEventLaunchedPayload* lpPayload =
            reinterpret_cast<const GuiEventLaunchedPayload*>(lpLaunchedEvent);

        if (lpPayload->miResult != 0)
        {
            // The X360 builds the request in a scratch stack slot and memcpy's the 288
            // bytes into the record (the inlined OutputGuiEvent<GuiOverlayRequest> copy);
            // constructed in place here.
            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct("CNOnlLchFail");
            lWire.mRequest.AddMessageParam(2, KPC_LAUNCH_FAILED_STRINGIDS[lpPayload->miResult]);

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(GuiOverlayRequestWire)));
        }
    }

    // ---- HandleLeftGameEvent @ 0x824C91C8 -----------------------------------------
    // In-queue event 273. Raise the reason-specific overlay(s), then hand the flow the
    // "DISCONNECT" state event and drop the expected-apt-component list if the screen is
    // still loading.
    void CarSelectMain::HandleLeftGameEvent(const CgsModule::Event* lpLeftGameEvent)
    {
        // The X360 fires BOTH asserts back-to-back on the null path: the bare expression
        // text (cpp:755) then the streamed message (cpp:756, again the original
        // OnlineGameRoomPlayerInfo copy-paste).
        CGS_ASSERT(lpLeftGameEvent != 0, "lpLeftGameEvent");                            // cpp:755
        CGS_ASSERT(lpLeftGameEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleLeftGameEvent"); // cpp:756

        const GuiEventLeftGamePayload* lpPayload =
            reinterpret_cast<const GuiEventLeftGamePayload*>(lpLeftGameEvent);

        if (lpPayload->miReason == KI_LEFT_GAME_REASON_LOBBY_DELETED)
        {
            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct("CNLobbyDlted");

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(GuiOverlayRequestWire)));
        }
        else if (lpPayload->miReason == KI_LEFT_GAME_REASON_KICKED)
        {
            // Tear down the two outstanding "are you sure you want to leave" questions
            // before the kick overlay goes up.
            {
                GuiOverlayWaitFinishWire lWaitGame;
                lWaitGame.mRequest.Construct("CNOnlLvGmQn");
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lWaitGame), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(GuiOverlayWaitFinishWire)));
            }
            {
                GuiOverlayWaitFinishWire lWaitChat;
                lWaitChat.mRequest.Construct("CNOnlLvChaQn");
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lWaitChat), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(GuiOverlayWaitFinishWire)));
            }

            // cpp:780 -- SIGNED bound check (asm: blt on 0 then blt on 5). The X360 streams
            // "Invalid kick reason of " << miKickReason << "\n" into the assert buffer;
            // collapsed to the static text per project policy (the streamed value is the
            // kick reason below).
            CGS_ASSERT(lpPayload->miKickReason >= 0 && lpPayload->miKickReason < KI_KICK_REASON_COUNT,
                       "Invalid kick reason of ");

            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct(KAPC_KICK_REASON_OVERLAYS[lpPayload->miKickReason]);

            // The X360 calls the out-of-line OutputGuiEvent<GuiOverlayRequest>
            // instantiation here; it queues exactly this record on channel 40.
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(GuiOverlayRequestWire)));
        }

        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "RG :: CSM : SendStateEvent( \"DISCONNECT\" ) 2\n";

        SendStateEvent("DISCONNECT");

        if (mpGuiCache != 0 &&
            (meCurrentState == E_CARSELECT_UNLOADED ||
             meCurrentState == E_CARSELECT_LOADING_COMPONENTS))
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        }
    }
}
