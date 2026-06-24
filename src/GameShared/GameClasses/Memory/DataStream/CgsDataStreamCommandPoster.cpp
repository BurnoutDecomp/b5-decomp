#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandPoster.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

#include <cstring>  // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

namespace CgsMemory
{
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
