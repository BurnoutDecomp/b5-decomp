#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
// CgsSceneManager::CgsCollision::PrimitivePairList::Itterator — a forward cursor over the
// packed primitive-pair records in a PrimitivePairList blob:
//   Prepare          @ 0x82812128 — point the cursor at the first record and cache it.
//   GetPrimativeA    @ 0x828121D8 — address of primitive A's data (past the header).
//   GetPrimativeB    @ 0x828121E8 — address of primitive B's data (past A's volume data).
//   MoveToNextHeader @ 0x82812210 — advance to and cache the next record.
// plus CollisionHeader::CheckCheckSum (the header-checksum assert shared by the above).
//
// Each record in the blob is { CollisionHeader (16 bytes), primitive-A data, primitive-B
// data }. The cursor (mpau8CurrentStreamPosition) points at the current record's header;
// mCurrentHeader is a cached copy of it. Behaviour-faithful to the X360 asm; the asm
// BeginAssert/FireAssert/EndAssert triples are collapsed into single CGS_ASSERTs.

namespace CgsSceneManager
{
namespace CgsCollision
{
    // -------------------------------------------------------------------------
    // CollisionHeader::CheckCheckSum @ CgsPrimitivePairList.h:85
    //
    // The asm loads the three type bytes + the checksum byte and compares
    // `mu8DataPaddingAndCheckSum == mu8PrimTypeA + mu8PrimTypeB + mu8HeaderType`
    // (a 32-bit cmpw of the zero-extended bytes), firing the assert on mismatch.
    // The byte members promote to int in the comparison, matching the cmpw.
    // -------------------------------------------------------------------------
    void PrimitivePairList::CollisionHeader::CheckCheckSum() const
    {
        CGS_ASSERT(
            mu8DataPaddingAndCheckSum == (mu8PrimTypeA + mu8PrimTypeB + mu8HeaderType),
            "mu8DataPaddingAndCheckSum == mu8PrimTypeA + mu8PrimTypeB + mu8HeaderType");
    }

    // -------------------------------------------------------------------------
    // Itterator::Prepare @ 0x82812128
    //
    // X360 (r3 = this, r4 = lpPairList):
    //   mpau8CurrentStreamPosition = lpPairList->mpaDataStream   (stw r11,0x10)
    //   mu16CurrentTest        = 0                               (sth 0,0x14)
    //   mu16CurrentDataPosition= 0                               (sth 0,0x18)
    //   mu16DataSize           = (mu16UsedData + 15) & ~15       (lhz 4 / addi 0xF / rlwinm)
    //   mu16NumHeaders         = lpPairList->mu16NumTests        (lhz 6 / sth 0x1A)
    //   mCurrentHeader = *(CollisionHeader*)mpaDataStream        (4-dword copy 0..0xC)
    //   mCurrentHeader.CheckCheckSum()                           (checksum assert)
    // -------------------------------------------------------------------------
    void PrimitivePairList::Itterator::Prepare(const PrimitivePairList* lpPairList)
    {
        mpau8CurrentStreamPosition = lpPairList->mpaDataStream;
        mu16CurrentTest            = 0;
        mu16CurrentDataPosition    = 0;
        // Round the in-use byte count up to a multiple of 16 (the asm's
        // `(size + 0xF) & 0xFFF0`); the result stays a u16.
        mu16DataSize  = static_cast<u16>((lpPairList->mu16UsedData + 15u) & 0xFFF0u);
        mu16NumHeaders = lpPairList->mu16NumTests;

        // Cache the first record's header (the asm copies the 16-byte header dword by
        // dword from the stream base) and validate its checksum.
        mCurrentHeader =
            *reinterpret_cast<const CollisionHeader*>(mpau8CurrentStreamPosition);
        mCurrentHeader.CheckCheckSum();
    }

    // -------------------------------------------------------------------------
    // Itterator::GetPrimativeA @ 0x828121D8
    //
    //   lwz  r11,0x10(r3)   ; mpau8CurrentStreamPosition
    //   addi r3,r11,0x10    ; + sizeof(CollisionHeader)
    //   blr
    //
    // Primitive A's packed data begins immediately after the 16-byte header.
    // -------------------------------------------------------------------------
    const void* PrimitivePairList::Itterator::GetPrimativeA() const
    {
        return mpau8CurrentStreamPosition + sizeof(CollisionHeader);
    }

    // -------------------------------------------------------------------------
    // Itterator::GetPrimativeB @ 0x828121E8
    //
    //   lbz   r9,0(r3)              ; mCurrentHeader.mu8PrimTypeA
    //   lwz   r10,0x10(r3)          ; mpau8CurrentStreamPosition
    //   rotlwi r9,r9,1              ; type * 2  (halfword index)
    //   lhzx  r11,r9,KAU16_VOLUME_SIZES ; KAU16_VOLUME_SIZES[type]
    //   add   r11,r11,r10           ; + cursor
    //   addi  r3,r11,0x10           ; + sizeof(CollisionHeader)
    //   blr
    //
    // Primitive B's packed data begins after the header and after primitive A's data,
    // whose size is the volume-type size of primitive A.
    // -------------------------------------------------------------------------
    const void* PrimitivePairList::Itterator::GetPrimativeB() const
    {
        const u16 lu16PrimASize = KAU16_VOLUME_SIZES[mCurrentHeader.mu8PrimTypeA];
        return mpau8CurrentStreamPosition + lu16PrimASize + sizeof(CollisionHeader);
    }

    // -------------------------------------------------------------------------
    // Itterator::MoveToNextHeader @ 0x82812210
    //
    // X360 (r31 = this):
    //   if (mu16CurrentTest + 1 >= mu16NumHeaders) return false;     // last record
    //   CGS_ASSERT(mpau8CurrentStreamPosition != NULL, ...:128);
    //   stride = mCurrentHeader.mu16PrimADataSize
    //          + mCurrentHeader.mu16PrimBDataSize + sizeof(CollisionHeader);
    //   mu16CurrentTest         += 1;
    //   mu16CurrentDataPosition += stride;
    //   CGS_ASSERT(mu16CurrentTest        < mu16NumHeaders, ...:135);
    //   CGS_ASSERT(mu16CurrentDataPosition < mu16DataSize,  ...:136);
    //   mpau8CurrentStreamPosition += stride;
    //   mCurrentHeader = *(CollisionHeader*)mpau8CurrentStreamPosition;  // re-cache
    //   mCurrentHeader.CheckCheckSum();                                  // ...h:85
    //   return true;
    //
    // The stride is one full record: the 16-byte header plus both primitives' data.
    // Note the cursor/data-position updates happen before the bounds asserts (matching
    // the asm store order), so the asserts observe the post-advance values.
    // -------------------------------------------------------------------------
    bool PrimitivePairList::Itterator::MoveToNextHeader()
    {
        if (mu16CurrentTest + 1 >= mu16NumHeaders)
        {
            return false;
        }

        CGS_ASSERT(mpau8CurrentStreamPosition != NULL,
                   "mpau8CurrentStreamPosition != NULL");

        const u16 lu16Stride = static_cast<u16>(
            mCurrentHeader.mu16PrimADataSize +
            mCurrentHeader.mu16PrimBDataSize +
            sizeof(CollisionHeader));

        mu16CurrentTest         = static_cast<u16>(mu16CurrentTest + 1);
        mu16CurrentDataPosition = static_cast<u16>(mu16CurrentDataPosition + lu16Stride);

        CGS_ASSERT(mu16CurrentTest < mu16NumHeaders,
                   "mu16CurrentTest < mu16NumHeaders");
        CGS_ASSERT(mu16CurrentDataPosition < mu16DataSize,
                   "mu16CurrentDataPosition < mu16DataSize");

        mpau8CurrentStreamPosition += lu16Stride;
        mCurrentHeader =
            *reinterpret_cast<const CollisionHeader*>(mpau8CurrentStreamPosition);
        mCurrentHeader.CheckCheckSum();

        return true;
    }
}
}
