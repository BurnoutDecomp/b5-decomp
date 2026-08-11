#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamConsumer.h"

#include <cstring>   // std::memcpy (the XMemCpy tail-calls)

namespace CgsMemory
{
    // X360 0x82868508 (15).
    //   0x82868524  bl  XMemCpy(this, lpProducer, 0x20)
    //   0x82868530  stw r30, 0x20(this)
    //   0x82868534  stw r29, 0x24(this)
    //   0x8286852C  lwz r4,  0x18(this)    <- mShared.mpPoster, read back from the copy
    //   0x82868538  bl  DataStreamCommandReader::Construct(this + 0x80, r4)
    //
    // ⚠️ The 0x20-byte XMemCpy is a CONSOLE SIZE LITERAL over a runtime-carved
    // struct whose two pointers widen on x64. Reproduced as the struct assignment
    // it is, not as a byte count (standing rule: "a count is not a size";
    // "carved at runtime => widen 4->8"). The console then re-loads mpPoster out
    // of the freshly written copy; that ordering is kept because it is what makes
    // the reader bind to the producer's poster rather than to a caller argument.
    void SimpleDataStreamConsumer::Construct(SimpleDataStreamProducer* lpProducer,
                                             void*                     lpvResultDestination,
                                             s32                       liResultDestinationSize)
    {
        mShared = lpProducer->mShared;

        mpvResultDestination   = lpvResultDestination;
        miResultDestinationSize = liResultDestinationSize;

        mReader.Construct(mShared.mpPoster);
    }

    // X360 0x82868548 (2): `addi r3, r3, 0x80 ; b DataStreamCommandReader::Destruct`.
    void SimpleDataStreamConsumer::Destruct()
    {
        mReader.Destruct();
    }

    // X360 0x82916FD8.
    // Thin forwarder onto the embedded command reader. The asm forms the reader
    // subobject address (addi r3, this, 0x80), passes lpDest (r4) and lpuOutIndex
    // (r5) through unchanged, and tail-delegates to
    // CgsMemory::DataStreamCommandReader::ReadCom, returning its result verbatim.
    s32 SimpleDataStreamConsumer::ReadCo(void* lpDest, u32* lpuOutIndex)
    {
        return mReader.ReadCom(lpDest, lpuOutIndex);
    }

    // X360 0x82868550 -- EXPORT HOLE, six instructions lifted from the image:
    //   lwz r11,16(r3) / lwz r9,20(r3) / mullw r10,r11,r5 / add r3,r10,r9 /
    //   mr r5,r11 / b XMemCpy
    // i.e. memcpy(resultBuffer + alignedResultSize*index, lpvResult, alignedResultSize).
    // The stride AND the copy length are both miAlignedResultSize -- the console
    // uses one register for both, so a whole aligned slot is written every time.
    void SimpleDataStreamConsumer::AddResult(const void* lpvResult, s32 liResultIndex)
    {
        const s32 liStride = mShared.miAlignedResultSize;

        u8* lpDestination = static_cast<u8*>(mShared.mpResultBuffer) + (liStride * liResultIndex);

        std::memcpy(lpDestination, lpvResult, static_cast<size_t>(liStride));
    }
}
