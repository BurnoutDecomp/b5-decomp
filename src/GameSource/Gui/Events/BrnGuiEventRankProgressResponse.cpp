// BrnGuiEventRankProgressResponse.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   BrnGui::GuiEventRankProgressResponse::Construct     @ 0x824F62F8
//   BrnGui::GuiEventRankProgressResponse::GetPlayerRank @ 0x8240EA28
//
// See the header banner for the layout proof (Construct's nine stores + the DecFIGS DWARF
// member list + the producing action's mirror-image field set).

#include "GameSource/Gui/Events/BrnGuiEventRankProgressResponse.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameSource/GameState/BrnGameActions.h"   // GameStateModuleIO::RankInfoResponseAction

namespace BrnGui
{

// @ 0x824F62F8 -- the game action -> GUI event repack.
//
//   lwz r11, 0(r4)  ; cmpwi cr6, r11, -1 ; bne cr6, loc_824F6304   <- the compare's taken and
//   stw r11, 0x20(r3)                                                 fall-through targets are
//   lwz r11, 4(r4)   ; stw r11, 0(r3)                                 the SAME instruction, so
//   lwz r11, 8(r4)   ; stw r11, 4(r3)                                 the test is dead code the
//   lwz r11, 0xC(r4) ; stw r11, 8(r3)                                 compiler left behind (an
//   lwz r11, 0x10(r4); stw r11, 0xC(r3)                               assert whose body folded
//   lwz r11, 0x14(r4); stw r11, 0x10(r3)                              away in this build). It
//   lwz r11, 0x18(r4); stw r11, 0x14(r3)                              changes nothing and is
//   lwz r11, 0x1C(r4); stw r11, 0x18(r3)                              not reproduced.
//   lwz r11, 0x20(r4); stw r11, 0x1C(r3)
//   blr
//
// So the copy is a ROTATION, not a memcpy: the action's rank word moves from the FRONT of the
// record to the BACK of the event, and everything else slides down one word. Written by name.
void GuiEventRankProgressResponse::Construct(
        const BrnGameState::GameStateModuleIO::RankInfoResponseAction* lpAction)
{
    // DWARF (BrnGuiEventTypeDefs.cpp:29) lists the inlined PlayerHasFinishedLastRank; both arms
    // store the same word (-1 == miPlayerRank on the taken path), which is the dead
    // `cmpwi r11,-1 ; bne next` ARTIST keeps at 0x824F62FC.
    if (lpAction->PlayerHasFinishedLastRank())
        miCurrentRank = KI_PLAYER_HAS_FINISHED_LAST_RANK;
    else
        miCurrentRank = lpAction->GetPlayerRank();     // action +0x00 -> event +0x20

    miRaceRank            = lpAction->GetOfflineRaceRank();       // +0x04 -> +0x00
    miRoadRageRank        = lpAction->GetRoadRageRank();          // +0x08 -> +0x04
    miStuntAttackRank     = lpAction->GetStuntAttackRank();       // +0x0C -> +0x08
    miMarkedManRank       = lpAction->GetMarkedManRank();         // +0x10 -> +0x0C
    miOfflineRaceRankWins = lpAction->GetOfflineRaceRankWins();   // +0x14 -> +0x10
    miRoadRageRankWins    = lpAction->GetRoadRageRankWins();      // +0x18 -> +0x14
    miStuntAttackRankWins = lpAction->GetStuntAttackRankWins();   // +0x1C -> +0x18
    miMarkedManRankWins   = lpAction->GetMarkedManRankWins();     // +0x20 -> +0x1C
}

// @ 0x8240EA28
//
//   lwz r11,0x20(this) ; cmpwi r11,-1 ; bne skip
//     <fire "miCurrentRank != KI_PLAYER_HAS_FINISHED_LAST_RANK" assert>
//   skip: lwz r3,0x20(this) ; return r3
//
// Reads miCurrentRank (s32 @ +0x20), runs a non-fatal guard that it is not the
// "finished last rank" sentinel (-1), and returns it (the X360 returns the raw value
// even when the guard fails). The X360-baked assert file/line are discarded per project
// convention; the stringized condition matches the X360 assert message text.
s32 GuiEventRankProgressResponse::GetPlayerRank() const
{
    CGS_ASSERT_AT( miCurrentRank != KI_PLAYER_HAS_FINISHED_LAST_RANK,
        "miCurrentRank != KI_PLAYER_HAS_FINISHED_LAST_RANK",
            "..\\..\\..\\GameSource\\Gui/BrnGuiEventTypeDefs.h", 7841 /* ARTIST 0x8240EA50 */);

    return miCurrentRank;
}

} // namespace BrnGui
