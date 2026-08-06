#include "GameSource/Network/X360/BrnServerInterfaceX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnServerInterfaceX360::Release @ 0x8258B2E0
//
// Wave-L partfile: the eventual home of this body is the sibling
// GameSource/Network/X360/BrnServerInterfaceX360.cpp (the X360 assert path in the
// binary is "...\gamesource\unity\../Network/X360/BrnServerInterfaceX360.cpp");
// it is landed here as a per-implementer partfile purely to avoid write
// collisions inside this wave, and is meant to be folded into that .cpp.
//
// NOT IN THIS FILE -- BrnServerInterfaceX360::Prepare @ 0x8258B200 is the mirror
// of this machine and belongs beside it, but its very first (unconditional) store
// parks &mGames in the prepare-params block and the committed
// CgsNetwork::ServerInterfacePrepareParams still models that word as part of an
// opaque `u8 maHead[48]`, so the body cannot name the field. It is reconstructed
// in full and parked at
// scratchpad/waveL/parked/BrnServerInterfaceX360_03_Prepare.cpp with the exact
// header lines that would unblock it.
//
// ---------------------------------------------------------------------------
// Release() -- the four-stage resumable teardown machine (measured, 0x8258B2E0)
//
// It is the exact MIRROR of Prepare's machine: Prepare runs Base THEN Games and
// clears meReleaseStage when it reaches DONE; Release runs Games THEN Base
// (reverse-order teardown) and clears mePrepareStage when it reaches DONE. Both
// cross-resets are measured, not inferred -- keep both.
//
// The X360 codegen is a 4-entry jump table on meReleaseStage (console +0x14C0)
// with the classic resume-machine FALL-THROUGH shape: each case first re-stores
// its own stage value (so a re-entry after a `false` return resumes at exactly
// that step), then does its one piece of work, then falls into the next case.
// A stage that returns false leaves meReleaseStage parked on itself.
//
//   0x8258B2F8  lwz   r11, 0x14C0(r31)          ; switch (meReleaseStage)
//   0x8258B2FC  cmplwi cr6, r11, 3              ; 4 cases; >3 -> default
//   0x8258B300  bgt   cr6, def_8258B31C
//   0x8258B304  li    r30, 0                    ; the constant 0 reused below
//   -- case 0 (E_RELEASESTAGE_START) --
//   0x8258B330  stw   r30, 0x14C0(r31)          ; meReleaseStage = START (re-store)
//   -- case 1 (E_RELEASESTAGE_GAMES_COMPONENT) --  falls in from case 0
//   0x8258B334  lwz   r11, 0xC2C(r31)           ; mGames' vptr
//   0x8258B338  li    r10, 1
//   0x8258B33C  addi  r3,  r31, 0xC2C           ; this-> &mGames
//   0x8258B340  lwz   r11, 0x1C(r11)            ; vtable slot +0x1C == Games::Release
//   0x8258B344  stw   r10, 0x14C0(r31)          ; meReleaseStage = GAMES_COMPONENT
//   0x8258B348  mtctr r11                       ;   (stored BEFORE the call)
//   0x8258B34C  bctrl                           ; mGames.Release()
//   0x8258B350  clrlwi r11, r3, 24              ; bool return (byte-truncated)
//   0x8258B358  beq   cr6, loc_8258B3AC         ; !ok -> return false
//   -- case 2 (E_RELEASESTAGE_BASECLASS) --      falls in from case 1
//   0x8258B35C  li    r11, 2
//   0x8258B360  mr    r3,  r31                  ; `this`
//   0x8258B364  stw   r11, 0x14C0(r31)          ; meReleaseStage = BASECLASS
//   0x8258B368  bl    BrnServerInterfaceBase::Release
//   0x8258B36C  clrlwi r11, r3, 24              ; bool return
//   0x8258B374  beq   cr6, loc_8258B3AC         ; !ok -> return false
//   -- case 3 (E_RELEASESTAGE_DONE) --           falls in from case 2
//   0x8258B378  li    r11, 3
//   0x8258B37C  stw   r30, 0x14BC(r31)          ; mePrepareStage = E_PREPARESTAGE_START
//   0x8258B380  li    r3,  1                    ; return true
//   0x8258B384  stw   r11, 0x14C0(r31)          ; meReleaseStage = DONE
//   -- default --
//   0x8258B38C  bl    CgsDev::Assert::BeginAssert
//   0x8258B394  li    r5,  0xB5                 ; __LINE__ == 181
//   0x8258B398  addi  r4,  ..., "d:\p4\b5_main\burnout\main\code\gamesource\..."
//   0x8258B3A0  addi  r3,  ..., "0"             ; the assert EXPRESSION string
//   0x8258B3A4  bl    CgsDev::Assert::FireAssert
//   0x8258B3A8  bl    CgsDev::Assert::EndAssert
//   0x8258B3AC  li    r3, 0                     ; ...and fall out returning false
//
// The default arm falls into the shared `r3 = 0` epilogue, so the assert does NOT
// return early -- it fires and then returns false, which is what CGS_ASSERT +
// `return false` reproduces.
//
// Console-vs-host note (measured offsets are COMMENTS only): 0xC2C / 0x14BC /
// 0x14C0 are 32-bit-pointer X360 layout facts. Nothing here reproduces them --
// every access is a by-name member access, so the host's (wider) layout is used.
//
// Dispatch note: the asm reaches Games::Release through mGames' own vtable slot
// +0x1C. That slot holds CgsNetwork::ServerInterfaceGamesX360::Release
// (0x8288C0C8), which is a single-instruction `b ServerInterfaceGames::Release`
// thunk -- so the direct by-name call on the by-value member is semantically
// identical (see scratchpad/waveL/ServerInterfaceX360.spec.md for the full
// 18-slot vtable dump).
// ---------------------------------------------------------------------------

namespace BrnNetwork
{
    bool BrnServerInterfaceX360::Release()
    {
        switch ( meReleaseStage )
        {
        case E_RELEASESTAGE_START:
            // Re-store of the stage the switch just matched (asm case 0). It is
            // redundant on the value but it IS emitted, and it is what makes each
            // arm a self-contained resume point.
            meReleaseStage = E_RELEASESTAGE_START;
            // fall through

        case E_RELEASESTAGE_GAMES_COMPONENT:
            meReleaseStage = E_RELEASESTAGE_GAMES_COMPONENT;
            if ( !mGames.Release() )
            {
                return false;
            }
            // fall through

        case E_RELEASESTAGE_BASECLASS:
            meReleaseStage = E_RELEASESTAGE_BASECLASS;
            if ( !BrnServerInterfaceBase::Release() )
            {
                return false;
            }
            // fall through

        case E_RELEASESTAGE_DONE:
            // Arm the MIRROR machine for the next Prepare (measured: stw of the
            // same zero register to +0x14BC, before the meReleaseStage store).
            mePrepareStage = E_PREPARESTAGE_START;
            meReleaseStage = E_RELEASESTAGE_DONE;
            return true;

        default:
            // BrnServerInterfaceX360.cpp:181 -- expression string "0" verbatim.
            CGS_ASSERT( false, "0" );
            return false;
        }
    }
}
