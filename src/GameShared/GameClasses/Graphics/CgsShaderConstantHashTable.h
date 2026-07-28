#pragma once

#include "types.hpp"

namespace CgsGraphics
{

// CgsShaderConstantHashTable.h:43
// ShaderConstantHashTable - a name->index lookup baked into shader-technique /
// material resources. Holds a sorted array of hashed shader-constant keys
// (muSize entries) and a parallel array of C-string names. After a resource is
// streamed in, FixUp relocates the two arrays and every name pointer by the
// load-time base offset (FixDown is its inverse, run before re-saving). GetName
// resolves a constant hash to its debug name via a hybrid binary/linear search
// over the sorted keys. Reconstructed from the DecFIGS DWARF
// (CgsShaderConstantHashTable.{h,cpp}) and the X360 pseudocode.
//
// The members below are the DWARF (host-width) shape kept for semantic parity by
// NAME. The SERIALISED form -- the only form the engine ever sees, since the
// table is a 12-byte window inside a streamed ShaderTechnique image -- is three
// u32 slots at +0/+4/+8; every method here walks that view instead of these
// members. See the .cpp banner.
struct ShaderConstantHashTable
{
public:
    // CgsShaderConstantHashTable.h:48
    void FixUp(u8* lpBaseData);

    // CgsShaderConstantHashTable.h:52
    // Inverse of FixUp: convert the relocated absolute pointers back to
    // file-relative offsets by subtracting the base. Header-inline helper for
    // compileability (this TU owns only FixUp/GetName); no separate ledger body
    // exists for it in the trace, so this is a reconstructed inverse, not a
    // transcribed body. The per-name fix-down MUST run while the name array is
    // still a valid pointer, so walk the name array first, then de-relocate the
    // two array pointers last (the mirror image of FixUp's order).
    //
    // SERIALISED-BLOCK SEAM: like FixUp/GetName (see the .cpp banner) this walks
    // the CONSOLE word layout -- three u32 slots at +0/+4/+8 -- because the table
    // is a window inside a streamed resource image on every platform, not a
    // host-width C++ object.
    void FixDown(u8* lpBaseData)
    {
        u32* const lpaWords = reinterpret_cast<u32*>(this);
        const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lpBaseData));
        u32* const lpaNames =
            reinterpret_cast<u32*>(static_cast<uintptr_t>(lpaWords[1]));
        for (u32 luI = 0; luI < lpaWords[2]; ++luI)
        {
            lpaNames[luI] -= luDelta;
        }
        lpaWords[1] -= luDelta;
        lpaWords[0] -= luDelta;
    }

    // CgsShaderConstantHashTable.h:56
    const char* GetName(u32 luHash) const;

    // CgsShaderConstantHashTable.h:59
    // Header-inline trivial accessor (not a separate ledger function).
    u32 GetSize() const { return muSize; }

private:
    // CgsShaderConstantHashTable.h:65
    u32* mpuHashKeys;   // offset 0x00 - sorted ascending array of muSize hash keys

    // CgsShaderConstantHashTable.h:66
    char** mppcNames;   // offset 0x04 - parallel array of muSize C-string names

    // CgsShaderConstantHashTable.h:67
    u32 muSize;         // offset 0x08 - number of entries in both arrays
};

} // namespace CgsGraphics
