// =====================================================================================
// rw::audio::core::PackedColumnReader -- run-length + delta unpacker for one packed-table
// column. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   ReadRunInfo   @0x82B6B928
//   ReadSymbol    @0x82B6B9A0
//   GetNextValue  @0x82B6B9F8
// The PowerPC asm is authoritative; no Feb-2007 leak source and no DecFIGS DWARF exist.
// Bodies are store-for-store equivalents of the IDA pseudocode, which mirrors the asm
// exactly. The sole dependency is the committed rw::audio::core::Unpack0::UnpackInt32
// (the zig-zag SIGNED varint decoder, @0x82B6B818). See PackedColumnReader.h for layout.
// =====================================================================================

#include "rw/audio/core/PackedColumnReader.h"
#include "rw/audio/core/Unpack0.h"

namespace rw
{
namespace audio
{
namespace core
{

// ---------------------------------------------------------------------------
// PackedColumnReader::ReadRunInfo @ 0x82B6B928
//
// Decode a SIGNED run header off the cursor. lwz r3,0(r11)=**mppCursor feeds
// UnpackInt32; the decoded value (v3) is signed-compared against 0 (cmpwi cr6).
// Non-negative -> literal run of v3+1 symbols (mbLiteral=1). Negative -> repeat
// run of 1-v3 copies (mbLiteral=0). The cursor advances by the bytes consumed.
// ---------------------------------------------------------------------------
void PackedColumnReader::ReadRunInfo()
{
    unsigned int uDecoded; // [sp+50h] var_20 -- UnpackInt32 out (signed payload)
    int bytesUsed = Unpack0::UnpackInt32(*mppCursor, &uDecoded);

    s32 v3 = static_cast<s32>(uDecoded); // r10 -- signed run header
    bool bNegative = (v3 < 0);           // cr56 -- signed compare against 0

    *mppCursor += bytesUsed; // **a1 += result -- advance the byte cursor
    mCount = v3 + 1;         // *(a1+8)  = v3 + 1
    mbLiteral = 1;           // *(a1+12) = 1

    if (bNegative)
    {
        mbLiteral = 0;   // *(a1+12) = 0
        mCount = 1 - v3; // *(a1+8)  = 1 - v3
    }
}

// ---------------------------------------------------------------------------
// PackedColumnReader::ReadSymbol @ 0x82B6B9A0
//
// Decode a SIGNED delta off the cursor, advance the cursor by the bytes consumed,
// and accumulate the delta into mValue.
// ---------------------------------------------------------------------------
void PackedColumnReader::ReadSymbol()
{
    unsigned int uDelta; // [sp+50h] var_20 -- UnpackInt32 out (signed payload)
    int bytesUsed = Unpack0::UnpackInt32(*mppCursor, &uDelta);

    *mppCursor += bytesUsed;              // **a1 += result -- advance the byte cursor
    mValue += static_cast<s32>(uDelta);   // *(a1+4) += v3[0] -- accumulate the delta
}

// ---------------------------------------------------------------------------
// PackedColumnReader::GetNextValue @ 0x82B6B9F8
//
// Return the next column value. When the run is exhausted (mCount <= 0) refresh it
// via ReadRunInfo, and if that produced a literal run read its first symbol. For a
// repeat run (mbLiteral still 0) read the one shared symbol. Then snapshot mValue,
// consume one run step (--mCount), and return the snapshot.
// ---------------------------------------------------------------------------
s32 PackedColumnReader::GetNextValue()
{
    if (mCount <= 0)
    {
        ReadRunInfo();
        if (mbLiteral)
            ReadSymbol();
    }
    if (!mbLiteral)
        ReadSymbol();

    s32 result = mValue; // r3 -- value to return
    --mCount;            // consume one run step
    return result;
}

} // namespace core
} // namespace audio
} // namespace rw
