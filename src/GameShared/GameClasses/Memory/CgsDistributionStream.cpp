#include "GameShared/GameClasses/Memory/CgsDistributionStream.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsMemory::DistributionStream - see the header. Faithful port of the X360 body.
namespace CgsMemory
{
    DistributionStream* DistributionStream::Construct()
    {
        muBytesToStream  = 0;
        mpList           = 0;
        muListLength     = 0;
        mpBaseAddress    = 0;
        muField4         = 0;
        muCurrentEntry   = 0;
        muEntryOffset    = 0;
        muStreamPosition = 0;
        mu32Flags        = 0;
        return this;
    }

    // @ 0x82867178 - arm the stream; preserves the other flag bits and ORs in "active".
    void DistributionStream::Execute(const DistributionStreamEntry* lpList, u32 luCount, u8* lpBaseAddress, u32 luField4)
    {
        CGS_ASSERT((mu32Flags & 1u) == 0, "Can't start stream while already streaming");
        CGS_ASSERT(lpList != 0 && lpBaseAddress != 0, "NULL list passed in");

        mpList           = lpList;
        muListLength     = luCount;
        mpBaseAddress    = lpBaseAddress;
        muField4         = luField4;
        muCurrentEntry   = 0;
        muEntryOffset    = 0;
        muStreamPosition = 0;
        mu32Flags       |= 1u;
    }
}
