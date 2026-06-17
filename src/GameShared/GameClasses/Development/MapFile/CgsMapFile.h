#pragma once

#include "types.hpp"

// CgsDev::MapFile - the function map the assert/exception callstack resolver reads (DWARF
// GameShared/GameClasses/Development/MapFile/CgsMapFile.h). A map is a binary file: a 16-byte header
// followed by an array of fixed-size Records, each naming one function + its address range. The build
// pipeline generates it from the linker's .map (tools/_make_cgsmap.py); the reader streams it to turn
// a captured return address into "the function it lies inside".
//
// X360 note: TargetPtr is the target's 32-bit (big-endian) pointer. On the PC x64 build the records
// store RVAs (address - image base, which fits in 32 bits for any one module and is ASLR-independent),
// and the file is written little-endian, so FixUp/FixDown (the X360 endian/pointer fix) are no-ops.

namespace CgsDev
{
namespace MapFile
{
    // CgsMapFile.h:41 - max function-name length stored per record.
    const s32 KI_NAME_LENGTH = 120;

    // The on-disk address word (X360: 32-bit big-endian target pointer; PC: 32-bit RVA).
    typedef u32 TargetPtr;

    // CgsMapFile.h:33 - file format version. The reader rejects anything but E_VERSION_CURRENT.
    enum EVersion
    {
        E_VERSION_1       = 0,
        E_VERSION_CURRENT = 1,
    };

    // CgsMapFile.h:58 - one function: its name + start address + byte size. Exactly 128 bytes on disk
    // (120 + 4 + 4), which is the record stride the reader walks.
    struct Record
    {
        char      macName[KI_NAME_LENGTH];
        TargetPtr mAddress;
        u32       muSize;
    };

    // CgsMapFile.h:74 - the file header (16 bytes on disk). In memory mpRecordArray is a real pointer
    // (the full-memory reader FixUp's the on-disk offset into one); the minimal-memory reader streams
    // the records instead and only reads muRecordCount / meVersion / the record offset.
    struct MapFileHeader
    {
        u32       muRecordCount;
        Record*   mpRecordArray;
        TargetPtr mBaseAddress;
        EVersion  meVersion;

        // CgsMapFile.h:78 - binary-search the record array for the one whose [mAddress, mAddress+muSize)
        // range contains lAddress (records are sorted by address). Null if none.
        const Record* FindRecord(TargetPtr lAddress) const;

        // CgsMapFile.h:81-83 - X360 endian / offset->pointer fix-ups + the version gate. No-ops on the
        // same-endian PC build except CheckVersion.
        void FixUp();
        void FixDown();
        void CheckVersion() const;
    };
}
}
