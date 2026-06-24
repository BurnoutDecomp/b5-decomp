#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamConsumer.h"

namespace CgsMemory
{
    // X360 0x82916FD8.
    // Thin forwarder onto the embedded command reader. The asm forms the reader
    // subobject address (addi r3, this, 0x80), passes lpDest (r4) and lpuOutIndex
    // (r5) through unchanged, and tail-delegates to
    // CgsMemory::DataStreamCommandReader::ReadCom, returning its result verbatim.
    s32 SimpleDataStreamConsumer::ReadCo(void* lpDest, u32* lpuOutIndex)
    {
        return mReader.ReadCom(lpDest, lpuOutIndex);
    }
}
