#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + PlayerName
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"         // GuiEventStuntInfo / GuiEventStuntAllComplete / GuiHUDMessageSignSmashed

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsID

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint ([DIAG] BRN_PROP_DIAG)
#include <stdlib.h>                                          // getenv    ([DIAG] BRN_PROP_DIAG)

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (gateui wave partfile 01). The three collectible/sign HUD-popup handlers the wave-B
// fan-out dispatches but never bodied:
//
//   HandleStuntInfo                          @0x8251F650  (37 insns)
//   HandleStuntsComplete(GuiEventStuntAllComplete*) @0x8251F7E8  (50 insns, cpp:5164/5168)
//   HandleSignSmashMessage                   @0x8251BF40  (25 insns)
//
// WHICH ONE IS THE BILLBOARD/SMASH-GATE POPUP -- the wave brief had this backwards.
// HandleSignSmashMessage is the CRASH-MODE OVERHEAD SIGN: its sole producer is
// BrnGame::BrnGameModule::TranslateShowtimeActionToGuiEvent @0x823E1988 case 128
// (E_ACTION_OVERHEAD_SIGN_HIT), whose whole translate call is gated on game mode 2/16, and
// its feeder is PropEntityIO::OutputBuffer_PostPhysics::mHitOverheadSignQueue (+0x7B0) ->
// CrashModeScoring::DealWithHitOverheadSign @0x82312928. A different subsystem.
// The REAL billboard / smash-gate popup is HandleStuntInfo: game action 58
// (E_ACTION_ON_STUNT_ELEMENT_COMPLETE, posted by StuntManager::ProcessStuntElement
// @0x8239CDB0) -> TranslateGameActionsToGuiEvents @0x823E9CE0 case 58 -> GuiEventStuntInfo
// (217) or GuiEventBoostBarStuntInfo (218) -> Update case 217 -> here. That is literally
// the "Billboards Smashed 12/45" line.
//
// The message-name table is KA_STUNT_INFO_MESSAGES (X360 .data qword_82FB5898), defined in
// BrnGuiHudMessageAnalyzer_wB_res.cpp alongside the other recovered rodata; the
// collectible-completion string ids are the already-committed KAPC_COLLECTABLE_COMPLETION_
// STRINGID (@0x8206F684) that the sibling area-complete handler in _wB_11 indexes.

namespace BrnGui
{

// @ 0x8251F650
// The running collectible tally popup -- "<family> <current>/<total>". The X360 body
// inlines the CgsID Construct overload (the message-name hash is stored straight into the
// stack record at +0x00 and the three parameter counts are zeroed at +0x08/+0x0C/+0x10,
// asm 0x8251F694..0x8251F6A4), then appends BOTH counts as INT parameters of display
// string 1 -- both calls pass `li r5,1` (0x8251F680 / 0x8251F6AC), i.e. two parameters on
// the same display string, not one each on strings 0 and 1.
//
// NOTE: no assert guards the type here (unlike the two sibling *Complete handlers) -- the
// X360 body indexes the 3-entry table unchecked (`lwz r8,8(r31); slwi r8,r8,3; ldx`).
// Reproduced as-is; the producer's own MapStuntEnumsFromGameplayToGui @0x823AA4A8 is what
// asserts >= 3.
void HudMessageAnalyzer::HandleStuntInfo(const GuiEventStuntInfo* lpEvent)
{
    GuiHudMessage lMessage;
    lMessage.Construct(KA_STUNT_INFO_MESSAGES[lpEvent->meStuntType]);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miCurrentCount);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1, lpEvent->miTotalCount);

    // [DIAG] NOT IN THE X360 BINARY. The last rung of the gateui ladder (set BRN_PROP_DIAG
    // -- the same knob the wave-Q prop probe uses, so the whole chain prints under one
    // switch). Emitted BEFORE TriggerMessage so a line appears even if the director drops
    // the message (IsMessageAllowed can veto it). The latch is evaluated ONCE.
    {
        static const bool sbUiGateDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        if ( sbUiGateDiag && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] hud stunt-info type=" << static_cast<s32>(lpEvent->meStuntType)
                << " count=" << lpEvent->miCurrentCount
                << "/" << lpEvent->miTotalCount
                << "\n";
        }
    }

    TriggerMessage(&lMessage);
}

// @ 0x8251F7E8
// "Every collectible of this family is done, city-wide" -- the sibling of the
// per-county HandleStuntsComplete(GuiEventStuntAreaComplete*) in _wB_11. Same
// "StntAllDone" message, same family-completion string id in slot 0, but the county slot
// carries the literal "PARADISE_CITY" instead of a county string id.
void HudMessageAnalyzer::HandleStuntsComplete(const GuiEventStuntAllComplete* lpEvent)
{
    // Non-gating tripwires (cpp:5164/5168; both streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpStuntAllCompleteEvent");
    CGS_ASSERT(lpEvent->meStuntElementType < E_STUNTTYPE_COUNT,
               "lpStuntAllCompleteEvent->meStuntElementType < E_STUNTTYPE_COUNT");

    GuiHudMessage lMessage;
    lMessage.Construct("StntAllDone");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 0,
                      KAPC_COLLECTABLE_COMPLETION_STRINGID[lpEvent->meStuntElementType]);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, "PARADISE_CITY");

    // [DIAG] NOT IN THE X360 BINARY -- see HandleStuntInfo.
    {
        static const bool sbUiGateDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        if ( sbUiGateDiag && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] hud stunt-all-complete type="
                << static_cast<s32>(lpEvent->meStuntElementType) << "\n";
        }
    }

    TriggerMessage(&lMessage);
}

// @ 0x8251BF40
// The crash-mode overhead-sign score popup ("ShowSignHit" + the points awarded as an INT
// parameter of display string 2 -- `li r4,2; li r5,2` @0x8251BF6C/0x8251BF70). Landed as a
// freebie alongside the two stunt handlers because it is 25 instructions and the fan-out
// already dispatches it (Update case 400); it is NOT the wave's proof rung.
void HudMessageAnalyzer::HandleSignSmashMessage(const GuiHUDMessageSignSmashed* lpEvent)
{
    GuiHudMessage lMessage;
    lMessage.Construct("ShowSignHit");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 2, lpEvent->miPointsAwarded);

    // [DIAG] NOT IN THE X360 BINARY -- see HandleStuntInfo.
    {
        static const bool sbUiGateDiag = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        if ( sbUiGateDiag && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] hud SignSmashed handled points=" << lpEvent->miPointsAwarded
                << "\n";
        }
    }

    TriggerMessage(&lMessage);
}

} // namespace BrnGui
