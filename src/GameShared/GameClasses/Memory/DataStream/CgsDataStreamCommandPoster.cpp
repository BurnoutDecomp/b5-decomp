#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandPoster.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

#include <cstring>  // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)
#include <cstdint>  // uintptr_t (Construct's two 128-byte buffer-alignment tripwires)
#include <atomic>   // std::atomic_thread_fence (End's console lwsync)

namespace CgsMemory
{
    // ========================================================================
    // X360 0x82869E08 (178 instructions) -- ⭐ RETIRES THE TRAP STUB THAT HAD KEPT THIS
    // WHOLE SUBSYSTEM OFF THE LINK SINCE 2026-08-06 (CgsDataStreamCommandPoster_LinkStub.cpp,
    // now deleted).
    //
    // The address is an EXPORT-SET HOLE -- there is no JSON at 0x82869E08 and no name index
    // hit -- so the body was LIFTED FROM THE IMAGE, not guessed:
    //   * identity: SimpleDataStreamProducer::Construct @0x8286A3B0's own `xrefs_from` names
    //     `CgsMemory::DataStreamCommandPoster::Construct` at exactly 0x82869E08;
    //   * signature: the PS3 DecFIGS mangle
    //     `_ZN9CgsMemory23DataStreamCommandPoster9ConstructEPviiiS1_ii` (7 params) matches the
    //     X360 prologue's r3..r10 (`this` + 7);
    //   * body: decoded 0x82869E08..0x8286A0CC with the scratchpad PPC decoder, which was
    //     proved 82/82 (mnemonic-for-mnemonic) against the exported neighbour
    //     BaseCollisionGenerator::RunFillTriangleCacheStream @0x82810D38 in the same run, on
    //     an image reader that self-tested 10/10.
    //
    // Five alignment tripwires, in the console's own order; every message string was READ OUT
    // OF THE IMAGE at the address the lifted `addi r4, r11, <lo>` names, not reconstructed
    // from memory:
    //   :48 lpCommandBuffer     % 128   "Command buffer MUST be on a 128 byte boundary\n"
    //   :49 lpDataBuffer        % 128   "Data buffer MUST be on a 128 byte boundary\n"
    //   :50 liCommandBufferSize % 128   "Command buffer MUST be a multiple of 128 bytes\n"
    //   :51 liDataBufferSize    % 128   "Data buffer MUST be a multiple of 128 bytes\n"
    //   :52 liCommandSize       % 16    "Command size MUST be a multiple of 16 bytes\n"
    //
    // ⚠️ mEncodedStatus is zeroed through an `ldarx`/`stdcx.` pair fenced by
    // `mfmsr`/`mtmsree` (0x8286A078..0x8286A08C) -- an ATOMIC store, not a plain one. The
    // idiom is not a decoder artefact: the exported DataStreamCommandReader::ReadCom
    // @0x82867964 carries the identical five-instruction sequence with IDA's own mnemonics.
    // Reproduced as the atomic's SetValue, exactly as Begin() below already does.
    //
    // ⚠️ miMaxCommands is a DIVISION, `liCommandBufferSize / liCommandSize`
    // (`0x8286A098 divw r10, r21, r20`), not a stored parameter -- the two `twi` words either
    // side of it are the compiler's divide-by-zero / overflow traps and carry no semantics.
    void DataStreamCommandPoster::Construct(void* lpCommandBuffer, s32 liCommandBufferSize,
                                            s32 liCommandSize, s32 liInitialCommandCount,
                                            void* lpDataBuffer, s32 liDataBufferSize,
                                            s32 liInitialDataBufferUsed)
    {
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpCommandBuffer) % 128u) == 0,
                   "Command buffer MUST be on a 128 byte boundary\n");
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpDataBuffer) % 128u) == 0,
                   "Data buffer MUST be on a 128 byte boundary\n");
        CGS_ASSERT((liCommandBufferSize % 128) == 0,
                   "Command buffer MUST be a multiple of 128 bytes\n");
        CGS_ASSERT((liDataBufferSize % 128) == 0,
                   "Data buffer MUST be a multiple of 128 bytes\n");
        CGS_ASSERT((liCommandSize % 16) == 0,
                   "Command size MUST be a multiple of 16 bytes\n");

        mbStreaming = false;                 // 0x8286A074 stb r30, 40(this)
        mEncodedStatus.SetValue(0);          // 0x8286A078..0x8286A08C ldarx/stdcx. pair

        mpcCommandBuffer    = static_cast<char*>(lpCommandBuffer);       // +0x08
        miCommandBufferSize = liCommandBufferSize;                       // +0x0C
        miCommandSize       = liCommandSize;                             // +0x10
        miNumCommands       = liInitialCommandCount;                     // +0x14
        miMaxCommands       = liCommandBufferSize / liCommandSize;       // +0x18 (divw)
        mpcDataBuffer       = static_cast<char*>(lpDataBuffer);          // +0x1C
        miDataBufferSize    = liDataBufferSize;                          // +0x20
        miDataBufferUsed    = liInitialDataBufferUsed;                   // +0x24
    }

    // X360 0x82867BE8 (79 instructions, exported).
    // Close a stream: acquire-fence, assert the stream was open (:130 "Not streaming\n"),
    // assert no reader still holds a reservation (:137 "Attempted to end stream while "
    // <n> " users connected\n" -- the NUM_USERS nibble of the packed status), then drop
    // mbStreaming. Nothing else is touched: the command cursor stays where the posting side
    // left it so the result walk can still size itself.
    void DataStreamCommandPoster::End()
    {
        // 0x82867BF8 `lwsync` -- the acquire half of the stream handshake. Mapped to the
        // host fence the tree already uses for a console lwsync (SDKs/XAudio precedent).
        std::atomic_thread_fence(std::memory_order_acquire);  // lwsync

        CGS_ASSERT(mbStreaming, "Not streaming\n");                 // :130

        // The console streams the count into the message ("Attempted to end stream while "
        // << n << " users connected\n", 0x82867CC8/0x82867CF0); CGS_ASSERT takes a literal,
        // so the two halves are joined and the count is dropped from the text only.
        const u8 lucNumUsers = static_cast<u8>(
            (GetEncodedStatus() >> KU_NUM_USERS_BIT) & KU_NUM_USERS_MAX);
        CGS_ASSERT(lucNumUsers == 0,
                   "Attempted to end stream while users connected\n"); // :137

        mbStreaming = false;                                        // 0x82867D18
    }

    // X360 0x82867D28.
    // Synchronously appends one command record. Asserts that streaming has not yet
    // begun and that there is still room (miNumCommands < miMaxCommands), then block-
    // copies miCommandSize bytes from lpCommand into slot miNumCommands of the command
    // buffer, bumps and returns the new command count.
    //
    // Asm member accesses: *(this+40)=mbStreaming, *(this+20)=miNumCommands,
    // *(this+24)=miMaxCommands, *(this+16)=miCommandSize, *(this+8)=mpcCommandBuffer.
    // XMemCpy(dest = miNumCommands*miCommandSize + mpcCommandBuffer, src = lpCommand,
    // count = miCommandSize); result = ++miNumCommands.
    s32 DataStreamCommandPoster::AddCommand(void* lpCommand)
    {
        // Two X360 assert tripwires (fire-on-failure), in store order.
        CGS_ASSERT(!mbStreaming,
                   "Streaming has already begun so can't syncronously add commands\n");
        CGS_ASSERT(miNumCommands < miMaxCommands,
                   "No space for more commands\n");

        std::memcpy(mpcCommandBuffer + miNumCommands * miCommandSize,
                    lpCommand,
                    static_cast<size_t>(miCommandSize));

        return ++miNumCommands;
    }

    // X360 0x82867E60.
    // Reserves one command slot without copying any payload: asserts not-streaming,
    // room remaining, and a non-NULL out pointer, then writes the slot address
    // (miCommandSize*miNumCommands + mpcCommandBuffer) through lppOutBuffer, bumps and
    // returns the new command count.
    //
    // Asm member accesses match AddCommand; the slot address is computed
    // miCommandSize*miNumCommands + mpcCommandBuffer (multiply operand order swapped
    // vs AddCommand, same product) and stored to *lppOutBuffer; result = ++miNumCommands.
    s32 DataStreamCommandPoster::AllocateCommand(void** lppOutBuffer)
    {
        // Three X360 assert tripwires (fire-on-failure), in store order.
        CGS_ASSERT(!mbStreaming,
                   "Streaming has already begun so can't synchronously add commands\n");
        CGS_ASSERT(miNumCommands < miMaxCommands,
                   "No space for more commands\n");
        CGS_ASSERT(lppOutBuffer != 0,
                   "lppOutBuffer is NULL\n");

        *lppOutBuffer = mpcCommandBuffer + miCommandSize * miNumCommands;

        return ++miNumCommands;
    }

    // X360 0x82867AE8 (⚠ .ida-exports HOLE -- reconstructed from the PS3 DecFIGS out-of-line
    // body @0xBD1930, whose baked asserts pin the file/lines).
    // Start a stream: assert not already streaming (:94), raise mbStreaming, then reset the
    // packed status word to 0. The console resets it through an ldarx/stdcx CAS loop (the
    // inlined GetEncodedStatus + SetValueConditional pair -- GetEncodedStatus's own
    // "Encoded status is only valid during stream" tripwire is satisfied by the mbStreaming
    // store above it); the PC build is single-threaded, so the atomic's own SetValue is the
    // same observable store.
    void DataStreamCommandPoster::Begin()
    {
        CGS_ASSERT(!mbStreaming, "Already streaming\n");   // CgsDataStreamCommandPoster.cpp:94
        mbStreaming = true;
        mEncodedStatus.SetValue(0);
    }

    // X360 0x82867790.
    // Returns the packed 64-bit status word, but only while streaming is in progress:
    // asserts mbStreaming (offset 40) is set, then loads and returns the raw 64-bit
    // value at offset 0 (mEncodedStatus).
    //
    // Asm: lbz r11,0x28(this)=mbStreaming; on zero, fire the assert; then
    // ld r3,0(this)=mEncodedStatus and return it.
    u64 DataStreamCommandPoster::GetEncodedStatus() const
    {
        CGS_ASSERT(mbStreaming,
                   "Encoded status is only valid during stream\n");

        return mEncodedStatus.GetValue();
    }
}
