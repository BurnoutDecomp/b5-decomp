#pragma once

// Canonical home for BrnReplays::ReplayIO and its nested per-phase output buffers.
// DWARF/assert home path: GameSource/Replays/BrnReplayModuleIO.h (the X360 bodies'
// lock asserts cite this file:
//   ReplayIO::GetOutputBuffer_PreSim  -> :107  "Not locked for reading"
//   ReplayIO::GetOutputBuffer_PostSim -> :175  "Not locked for reading"
//   OutputBuffer_PostSim::AppendGameEventQueue -> :176 "Not locked for writing").
//
// ReplayIO is the per-frame IO payload the replay module exchanges with the rest of
// the game. It derives from CgsModule::IOBuffer (status byte at +0) and embeds two
// phase output buffers: a pre-sim buffer (returned by GetOutputBuffer_PreSim) and a
// post-sim buffer (returned by GetOutputBuffer_PostSim). The phase accessors take the
// ReplayIO's READ lock (bit 4) and hand back the embedded sub-buffer pointer; the
// sub-buffer is then WRITE-locked (bit 3) by its producer while it is filled.
//
// LAYOUT (X360 asm authoritative; getter return-offsets pin the two sub-buffers):
//   base CgsModule::IOBuffer                          (status byte @ +0; +1..+3 pad)
//   +0x004   OutputBuffer_PostSim mPostSimBuffer      (GetOutputBuffer_PostSim @0x823BB478 -> this+4)
//   +0x61C   OutputBuffer_PreSim  mPreSimBuffer       (GetOutputBuffer_PreSim  @0x823BB128 -> this+1564)
//
// OutputBuffer_PostSim itself derives from CgsModule::IOBuffer and embeds a single
// game-event queue: VariableEventQueue<1536,16> at +4 (AppendGameEventQueue @0x8265A798
// write-locks, then Append<1536,16>'s a source <1536,16> queue into this+4). The
// post-sim sub-buffer occupies +4..+1564 of ReplayIO (1560 bytes).

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue

namespace BrnReplays
{
namespace ReplayIO
{
    // ---- post-sim output buffer ---------------------------------------------------
    // Derives from IOBuffer (its own status byte at +0). The game-event queue lives at
    // +4 (AppendGameEventQueue appends into this+4). VariableEventQueue<1536,16> has
    // miBufferWritePos at queue-offset +1540 (X360 *(a1+1540)) -> sizeof 1552; with the
    // 4-byte IOBuffer header that puts the buffer at 1556 bytes. The ReplayIO getter
    // spacing (this+4 .. this+1564) gives the embedded sub-buffer a 1560-byte slot, so
    // a 4-byte trailing reserve rounds it to the X360-observed stride.
    struct OutputBuffer_PostSim : public CgsModule::IOBuffer
    {
        typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;

        // X360 0x8265A798: write-lock (bit 3); asserts the buffer is locked for writing
        // ("Not locked for writing", BrnReplayModuleIO.h:176), then bulk-appends the
        // source game-event queue into mGameEventQueue (Append<1536,16>). Returns the
        // Append result.
        int AppendGameEventQueue(const GameEventQueue* lpSourceQueue);

        static void _AssertLayout();

    private:
        u8             maStatusPad[3];     // +0x001..+0x003 (force the queue to +4)
        GameEventQueue mGameEventQueue;    // +0x004 (queue.miBufferWritePos @ +1540)
        u8             maTailReserve[4];   // round the sub-buffer to the +1560 stride
    };

    // ---- pre-sim output buffer ----------------------------------------------------
    // FLAG (foreign-ish layout): the pre-sim sub-buffer's full member list is not in
    // this TU group (its accessor TU only returns the pointer). It is an IOBuffer-
    // derived buffer; modelled as correctly-sized opaque storage so the ReplayIO
    // member offset (+1564) is exact. When its own TU lands this should adopt the named
    // members additively. Sized to match the X360 getter stride that follows it.
    struct OutputBuffer_PreSim : public CgsModule::IOBuffer
    {
    private:
        u8 maStorage[1560 - 1];   // IOBuffer status(+0) + opaque payload
    };

    // ---- the replay module IO payload ---------------------------------------------
    struct ReplayIO_Buffer : public CgsModule::IOBuffer
    {
        // X360 0x823BB478: read-lock (bit 4); asserts locked for reading
        // ("Not locked for reading", BrnReplayModuleIO.h:175); returns &mPostSimBuffer
        // (this+4).
        OutputBuffer_PostSim* GetOutputBuffer_PostSim();
        // X360 0x823BB128: read-lock (bit 4); asserts locked for reading
        // ("Not locked for reading", BrnReplayModuleIO.h:107); returns &mPreSimBuffer
        // (this+1564).
        OutputBuffer_PreSim* GetOutputBuffer_PreSim();

        static void _AssertLayout();

    private:
        u8                   maStatusPad[3];   // +0x001..+0x003 (force +4 placement)
        OutputBuffer_PostSim mPostSimBuffer;   // +0x004
        OutputBuffer_PreSim  mPreSimBuffer;    // +0x61C (this+1564)
    };
}
}
