#include "SDKs/Xam/CSchemaInMemory.h"

#include <cstring>

// ===========================================================================
// CSchemaInMemory -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm.
// Bind @ 0x8297F2B8. Sibling of the Xam marshalling classes
// (CSchemaData::LookupConstantFromTable @ 0x8297F280 is the function
// immediately preceding Bind in this TU). See CSchemaInMemory.h for the layout.
// ===========================================================================

// E_FAIL HRESULT the guard path returns (lis r29,-0x8000 ; ori r29,r29,0x4005).
static const s32 KI_E_FAIL = (s32)0x80004005;

// ---------------------------------------------------------------------------
// CSchemaInMemory::Bind @ 0x8297F2B8
//
// Copy the source blob's 0x2C-byte header into this descriptor, then carve the
// source's trailing payload (source + 0x2C) into a run of sub-sections, storing
// a cursor (or a size) for each back into this object at +0x2C..+0x48.
//
//   memcpy(this, source, 0x2C)                    ; li r5,0x2C ; bl memcpy
//   if (*(u32*)(this+8) & 1) return 0x80004005    ; lwz r11,8 ; clrlwi. ,31
//   p = source + 0x2C                             ; addi r9,r30,0x2C
//   if (muExtraSize(@0x26)) { mpExtra=p; p+=extra }; lhz r6,0x26 ; stw r9,0x3C
//   r7 = @0x1A * @0x18   (rows*cols)              ; mullw r7,r10,r8
//   r8 = @0x20 * 2       (halfwords*2)            ; rotlwi r8,r10,1
//   mpSection0 = p                                ; stw r9,0x2C
//   r10 = @0x28 * 4      (count*4)                ; rotlwi r10,r11,2
//   r11 = @0x2A * @0x28  (stride*count)           ; mullw r11,r5,r11
//   p += count*4 ; mpSection1 = p                 ; add r9 ; stw r9,0x30
//   remainder = totalSize - count*4 - extra
//                       - stride*count - 0x2C     ; subf x3 ; addi -0x2C
//   mpSection2 = section1 + stride*count          ; add r11 ; stw r11,0x34
//   muRemainderSize = remainder                   ; stw r10,0x38
//   mpSection3 = section2 + remainder             ; add r11 ; stw r11,0x40
//   mpSection4 = section3 + rows*cols             ; add r11 ; stw r11,0x44
//   mpSection5 = section4 + halfwords*2           ; add r10 ; stw r10,0x48
//   return 0 (S_OK)
//
// Widths: every store is stw (full word); header reads are lhz (u16) except
// muTotalSize @0x14 (lwz, u32). The two rotlwi are strength-reduced multiplies
// (by 2 and by 4). All computed pointers land inside the caller-owned source
// span, so this descriptor holds cursors into `source`, not owned storage.
// ---------------------------------------------------------------------------
s32 CSchemaInMemory::Bind(CSchemaInMemory* lpSource)
{
    memcpy(this, lpSource, 0x2C);

    // Reject a blob that has bit0 of its flags set.
    if ((mFlags & 1) != 0)
    {
        return KI_E_FAIL;
    }

    u8* lpPayload = reinterpret_cast<u8*>(lpSource) + 0x2C;

    // Optional leading extra block.
    if (muExtraSize != 0)
    {
        mpExtra = lpPayload;
        lpPayload += muExtraSize;
    }

    const u32 luRowByRow     = static_cast<u32>(muRows) * static_cast<u32>(muCols);
    const u32 luHalfWordsX2   = static_cast<u32>(muHalfWords) * 2u;
    const u32 luBlockIndexX4  = static_cast<u32>(muBlockCount) * 4u;
    const u32 luBlockSpan     = static_cast<u32>(muBlockStride) * static_cast<u32>(muBlockCount);

    // Section 0: at the current payload cursor.
    mpSection0 = lpPayload;

    // Section 1: one block-index table (4 bytes per block) later.
    lpPayload += luBlockIndexX4;
    mpSection1 = lpPayload;

    // Section 2: one block span (stride * count) later.
    mpSection2 = lpPayload + luBlockSpan;

    // Remaining bytes after the fixed header, extra block, index table and block
    // span have been subtracted from the declared total size.
    muRemainderSize = muTotalSize - luBlockIndexX4 - muExtraSize - luBlockSpan - 0x2C;

    // Section 3: the remainder placed after section 2.
    mpSection3 = mpSection2 + muRemainderSize;

    // Section 4: the row-major body (rows * cols) after section 3.
    mpSection4 = mpSection3 + luRowByRow;

    // Section 5: the trailing half-word block (2 * count) after section 4.
    mpSection5 = mpSection4 + luHalfWordsX2;

    return 0;
}
