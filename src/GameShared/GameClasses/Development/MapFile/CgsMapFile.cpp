#include "GameShared/GameClasses/Development/MapFile/CgsMapFile.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (CheckVersion)

namespace CgsDev
{
namespace MapFile
{
    // CgsMapFile.cpp:47 - binary-search the address-sorted record array for the record whose
    // [mAddress, mAddress + muSize) range contains lAddress. (Used by the full-memory reader path; the
    // minimal-memory reader the assert uses streams + matches inline instead.)
    const Record* MapFileHeader::FindRecord(TargetPtr lAddress) const
    {
        s32 liLow  = 0;
        s32 liHigh = static_cast<s32>(muRecordCount) - 1;
        while (liLow <= liHigh)
        {
            const s32     liMid    = liLow + (liHigh - liLow) / 2;
            const Record& lrRecord = mpRecordArray[liMid];
            if (lAddress < lrRecord.mAddress)
                liHigh = liMid - 1;
            else if (lAddress >= lrRecord.mAddress + lrRecord.muSize)
                liLow = liMid + 1;
            else
                return &lrRecord;
        }
        return nullptr;
    }

    // CgsMapFile.h:81/82 - the X360 byte-swaps the file (big-endian target) and turns the on-disk record
    // offset into a real pointer here. The PC build writes/reads the file native little-endian, so the
    // endian conversion is a no-op; the offset->pointer fix-up belongs to the full-memory load path.
    void MapFileHeader::FixUp()   {}
    void MapFileHeader::FixDown() {}

    // CgsMapFile.h:83 - reject a map that is not the current format version.
    void MapFileHeader::CheckVersion() const
    {
        CGS_ASSERT(meVersion == E_VERSION_CURRENT,
                   "map file is not the correct version. make sure you have the latest tools / code");
    }
}
}
