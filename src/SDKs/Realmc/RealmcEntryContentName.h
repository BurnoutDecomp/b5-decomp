#pragma once
#include "types.hpp"
namespace RealmcIface
{
// ARTIST 82B51C10/82B51C70: 128 UTF16 title units, 42 filename bytes,
// natural two-byte alignment, then the signed scaled-size word at 12C.
// The DataBuffer at 130 belongs to SaveInfo, not this object.
class EntryContentName
{
public:
    // @ 0x82B51C10 -- SetTitle(pTitleSource), SetFileName(pFileNameSource), then
    //                 mnSize = (int)((float)nSizeSeed * 2.8f). The IDA signature
    //                 lists (a1,a2,a3,a4); a2 is the title source, a3 the file-name
    //                 source, a4 the size seed.
    EntryContentName(const void* pTitleSource,
                     const char* pFileNameSource,
                     int nSizeSeed);

    // @ 0x82B51AD8 -- copy 256 bytes from pTitleSource into macTitle (or zero the
    //                 256 bytes when pTitleSource is null), then zero the 16-bit
    //                 word at +0xFE. Returns this (the X360 returns r3).
    EntryContentName* SetTitle(const void* pTitleSource);

    // @ 0x82B51B28 -- strncpy at most 42 bytes from pFileNameSource into
    //                 macFileName + zero-store the byte at +0x129 (NUL); when
    //                 pFileNameSource is null, zero-store macFileName[0] only.
    //                 Returns this.
    EntryContentName* SetFileName(const char* pFileNameSource);

    // Copy-construct: mnSize first, then SetTitle(rOther.macTitle) and
    // SetFileName(rOther.macFileName). muField130/muField134 are NOT copied here;
    EntryContentName(const EntryContentName& rOther);

private:
    u16 macTitle[128];
    char macFileName[0x2A];
    int32_t mnSize;
};
CGS_STATIC_ASSERT(sizeof(EntryContentName) == 0x130, "ARTIST EntryContentName size");
}
