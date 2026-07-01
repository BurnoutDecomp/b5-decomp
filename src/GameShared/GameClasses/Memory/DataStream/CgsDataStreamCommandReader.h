#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandPoster.h"

// CgsMemory::DataStreamCommandReader
// ---------------------------------------------------------------------------
// The reading side of a lock-free command stream: it plays back the fixed-stride
// command records a DataStreamCommandPoster appended. The reader is a thin
// wrapper that holds a pointer to the poster it drains (mpPoster @ +4); the
// command buffer and the packed 64-bit lock-free status word live on the poster.
// Each ReadCom() atomically advances the poster's packed nextCommand field (low
// 24 bits of mEncodedStatus) via a compare-and-swap retry loop, then block-copies
// the reserved record out.
//
// X360 homes (reconstructed in this pass):
//   ReadCom (ReadCommand) @ 0x82867920
//   Construct             @ 0x82867830
//   Destruct              @ 0x828678B0
//
// Layout: the X360 ReadCom reads the poster pointer from *(this+4); the leading
// 4-byte word at +0 is NOT touched by ReadCom, but Construct writes -1 through it
// (asm `li r11,-1` / `stw r11,0(r3)`) before entering its CAS loop. The DecFIGS
// DWARF names this field miLastCommandIndex (CgsDataStreamCommandReader.h:98),
// which matches the observed -1 ("no command read yet") sentinel init exactly;
// no X360-attested function reads it back in this pass, so it is adopted by name
// per DWARF but not yet load-bearing beyond Construct's init. The reader is
// embedded at offset +128 inside a SimpleDataStreamConsumer (proven by ReadCo @
// 0x82916FD8, which calls ReadCom(this+128, ...)). Member access is by name
// throughout.
namespace CgsMemory
{
    struct DataStreamCommandReader
    {
        // Reconstructed in this pass (X360 0x82867830). Binds this reader to
        // lpPoster and registers it as a user of the poster's stream: atomically
        // increments the poster's packed numUsers nibble (bits 24-27 of
        // mEncodedStatus) via a compare-and-swap retry loop, preserving the
        // nextCommand field. Also sets miLastCommandIndex = -1 (X360 sentinel init).
        void Construct(DataStreamCommandPoster* lpPoster);

        // Reconstructed in this pass (X360 0x828678B0). Atomically decrements
        // the bound poster's packed numUsers nibble via the same compare-and-
        // swap retry loop as Construct (unregisters this reader as a user).
        void Destruct();

        // Reconstructed in this pass (X360 0x82867920). Atomically reserves the
        // next command slot and copies it into lpDest; writes the slot index
        // through lpuOutIndex when non-NULL. Returns 0 on success, 1 when the
        // stream is drained (nextCommand has reached the poster's posted count).
        s32 ReadCom(void* lpDest, u32* lpuOutIndex);

    private:
        // --- members ---------------------------------------------------------
        // +0: DWARF name (CgsDataStreamCommandReader.h:98); not referenced by
        //     ReadCom in this pass, Construct initialises it to -1 (matches the
        //     name: no command read yet). Sized/placed so mpPoster lands at +4
        //     (matches the *(this+4) poster load in ReadCom/Destruct).
        s32                      miLastCommandIndex; // +0  read cursor sentinel (-1 = none read); DWARF name
        DataStreamCommandPoster* mpPoster;           // +4  the poster this reader drains
    };
}
