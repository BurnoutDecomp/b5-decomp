#include "SDKs/XCam/XCamPacketizer.h"

#include <cstring>  // memset / memcpy

// ===========================================================================
// XCAM::CPacketizer -- FEC video packetizer, reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. See XCamPacketizer.h for the layout and the
// serialised packet/parity formats. The wire buffers and parity rows are
// external byte streams, so their fields are accessed by documented raw offset
// (serialised-data exception); `this` uses named members throughout.
//
// The X360 integer-divide guards (`twllei r,0`) are the compiler's divide-by-
// zero traps; every divisor here is a member that is provably non-zero
// (muChunkSize >= 8), so they carry no source-level meaning and are omitted.
// ===========================================================================

namespace XCAM
{

// Reduce a sequence counter modulo 0xFF78, reproducing the exact asm branch:
// non-negative values take an unsigned modulo; the (never-taken in practice)
// negative branch adds the modulus once (asm `add r,r,0xFF78`, equivalent to the
// pseudocode's LOWORD(x) - 0x88 since 0xFF78 == -0x88 mod 0x10000).
static inline u16 WrapSequence(int iValue)
{
    if (iValue >= 0)
    {
        u32 uValue = static_cast<u32>(iValue);
        if (uValue >= 0xFF78u)
            uValue %= 0xFF78u;
        return static_cast<u16>(uValue);
    }
    return static_cast<u16>(static_cast<u32>(iValue) + 0xFF78u);
}

// @ 0x82986440
int CPacketizer::Initialize(int iPayloadSize, u32 uMaxPacketSize)
{
    if (uMaxPacketSize < 0x10u)
        return 8;

    miBitrate = 0;
    if (uMaxPacketSize > 0x408u)
        uMaxPacketSize = 0x408u;

    const u32 uChunkSize = (uMaxPacketSize - 8u) & 0xFFFFFFF8u;
    muChunkSize = uChunkSize;

    const u32 uNumChunks =
        (uChunkSize + static_cast<u32>(iPayloadSize) - 1u) / uChunkSize;
    muPacketSize = uChunkSize + 8u;
    if (uNumChunks > 0x40u)
        return 87;

    const int iBufferSize = static_cast<int>(uChunkSize * uNumChunks) + 0x11;
    mpBuffers[0] = static_cast<u8*>(
        XMemAlloc(3u * static_cast<u32>(iBufferSize), KU_PACKETIZER_XMEM_ATTRIBUTES));
    if (!mpBuffers[0])
        return 8;

    muRowCount = 3u * ((uNumChunks + 3u) >> 2);
    if (muRowCount < 2u)
        muRowCount = 2u;

    const u32 uRowStride = muChunkSize + 12u;
    mpRowTable = static_cast<u8*>(
        XMemAlloc((muChunkSize + 16u) * muRowCount, KU_PACKETIZER_XMEM_ATTRIBUTES));
    if (!mpRowTable)
    {
        XMemFree(mpBuffers[0], KU_PACKETIZER_XMEM_ATTRIBUTES);
        return 8;
    }

    miWriteBuffer = 0;
    miCurrentRow  = 0;
    muWriteCount  = 0;
    miSeqBase     = 0;
    miNewestSeq   = 0;

    // Carve the single allocation into the 3 assembly buffers and reset headers.
    u8* const pBase = mpBuffers[0];
    int iOffset = 0;
    for (int i = 0; i < 3; ++i)
    {
        u8* const pBuf = pBase + iOffset;
        iOffset += iBufferSize;
        mpBuffers[i] = pBuf;
        reinterpret_cast<u32*>(pBuf)[0] = static_cast<u32>(miSeqBase);
        reinterpret_cast<u32*>(pBuf)[1] = static_cast<u32>(miCurrentRow);
        reinterpret_cast<u32*>(pBuf)[2] = 0;
        reinterpret_cast<u32*>(pBuf)[3] = 0;
    }

    // Lay out the parity-row pointer table and point each slot at its row body.
    u8** const ppRows = reinterpret_cast<u8**>(mpRowTable);
    u8* const pFirstRow = mpRowTable + 4u * muRowCount;
    if (muRowCount != 0)
    {
        u8* pRow = pFirstRow;
        for (u32 i = 0; i < muRowCount; ++i)
        {
            ppRows[i] = pRow;
            pRow += uRowStride;
            reinterpret_cast<u32*>(ppRows[i])[0] = 0;  // clear the row length
        }
    }

    miChunkInRow  = 0;
    miChunkTarget = 8;

    // Prime the first parity row's packet header (fresh, seq flag = 2).
    std::memset(pFirstRow + 4, 0, muPacketSize);
    pFirstRow[4] = 1;
    pFirstRow[5] = static_cast<u8>((pFirstRow[5] & 0xFC) | 2);

    muStartTick   = GetTickCount();
    muReserved40  = 0;
    return 0;
}

// @ 0x82986628
void CPacketizer::Shutdown()
{
    if (mpBuffers[0])
        XMemFree(mpBuffers[0], KU_PACKETIZER_XMEM_ATTRIBUTES);
    if (mpRowTable)
        XMemFree(mpRowTable, KU_PACKETIZER_XMEM_ATTRIBUTES);
}

// @ 0x82986680
u8* CPacketizer::GetBufferForWriting()
{
    const int iIndex = miWriteBuffer;
    int iNext = iIndex + 1;
    if (iNext >= 3)
        iNext = 0;

    // Latch the sequence base of the buffer about to be recycled.
    miSeqBase = static_cast<int>(reinterpret_cast<u32*>(mpBuffers[iNext])[0]);
    return mpBuffers[iIndex] + 0x11;
}

// @ 0x829866B8
int CPacketizer::GetNewestPacketSequenceNumber()
{
    return miNewestSeq;
}

// @ 0x829866C0
void CPacketizer::SetBitrate(int iBitrate)
{
    miBitrate = iBitrate;
}

// @ 0x82986748
void CPacketizer::BufferWriteCompleted(u32 uLength, const u8* pMarker)
{
    if (uLength == 0xFFFFFFFFu || pMarker == nullptr)
        return;

    // Stamp the just-written packet buffer's header with the current stream state.
    u8* const pBuf = mpBuffers[miWriteBuffer];
    const u8 uMarker = *pMarker;
    ++muWriteCount;
    reinterpret_cast<u32*>(pBuf)[0] = static_cast<u32>(miNewestSeq);
    reinterpret_cast<u32*>(pBuf)[1] = static_cast<u32>(miCurrentRow);
    reinterpret_cast<u32*>(pBuf)[2] = static_cast<u32>(miChunkInRow);
    reinterpret_cast<u32*>(pBuf)[3] = uLength;
    pBuf[0x10] = uMarker;

    if (uLength != 0 && miBitrate > 32)
    {
        u8** const ppRows = reinterpret_cast<u8**>(mpRowTable);
        const u32 uBufSeq = reinterpret_cast<u32*>(pBuf)[0];  // seq stamped above
        const u32 uLenMinus1 = uLength - 1u;

        u8* pRow = ppRows[miCurrentRow];
        u32 uOffset = 0;
        do
        {
            u32 uChunk = uLength - uOffset;
            const u32 uChunkSize = muChunkSize;
            ++miChunkInRow;
            ++miNewestSeq;
            if (uChunk >= uChunkSize)
                uChunk = uChunkSize;
            if (reinterpret_cast<u32*>(pRow)[0] < uChunk)
                reinterpret_cast<u32*>(pRow)[0] = uChunk;

            // Sequence tag for this parity row.
            *reinterpret_cast<u16*>(pRow + 6) = WrapSequence(miNewestSeq);

            // Rebuild the packed +0x08 metadata bitfield for this chunk.
            const u32 uOffChunks = uOffset / uChunkSize;
            const u32 uMeta =
                  (static_cast<u32>(miChunkInRow) & 0xFu)
                | ((static_cast<u32>(uMarker) << 4) & 0x30u)
                | ((uOffChunks << 6) & 0xFC0u)
                | (((uLenMinus1 / uChunkSize) << 12) & 0x3F000u)
                | ((static_cast<u32>(
                      static_cast<u32>(miNewestSeq) - uOffChunks - uBufSeq - 1u) << 18)
                     & 0x3C0000u)
                | (((uChunk - 1u) << 22) & 0xFFC00000u);
            // The +0x08 word folds up to miChunkTarget chunks: bits 0-3 (chunkInRow)
            // are a plain insert, but bits 4-31 XOR-ACCUMULATE into the running row
            // value (asm xor+rlwimi chain @0x82986874..0x82986908, e.g. rlwimi
            // r11,r10,0,0,27 keeps the new nibble + old[4:31], then each field xor'd).
            u32* const pMeta = reinterpret_cast<u32*>(pRow + 8);
            *pMeta = (uMeta & 0xFu) | ((*pMeta ^ uMeta) & 0xFFFFFFF0u);

            // Fold this chunk's payload into the parity row (XOR accumulate).
            u8* const pDst = pRow + 12;
            const u8* const pSrc = pBuf + 0x11 + uOffset;
            for (u32 i = 0; i < uChunk; ++i)
                pDst[i] ^= pSrc[i];

            // Flush and open a fresh parity row when this one is full, or when the
            // bitrate budget says we have emitted enough recovery data.
            if (miChunkInRow == miChunkTarget ||
                (muWriteCount > 3u && muChunkSize + uOffset >= uLength))
            {
                muWriteCount = 0;
                int iNextRow = miCurrentRow + 1;
                if (static_cast<u32>(iNextRow) >= muRowCount)
                    iNextRow = 0;
                miCurrentRow = iNextRow;
                pRow = ppRows[iNextRow];
                std::memset(pRow + 4, 0, muPacketSize);
                pRow[4] = 1;
                pRow[5] = static_cast<u8>((pRow[5] & 0xFC) | 2);
                miChunkInRow = 0;
                ++miNewestSeq;
            }
            uOffset += muChunkSize;
        }
        while (uOffset < uLength);
    }
    else
    {
        if (uLength != 0)
            miNewestSeq += static_cast<int>((uLength - 1u) / muChunkSize);
        miChunkInRow = 0;
        ++miNewestSeq;
    }

    // Advance to the next round-robin write buffer.
    ++miWriteBuffer;
    if (miWriteBuffer >= 3)
        miWriteBuffer = 0;
}

// @ 0x82986A88
int CPacketizer::GetEncodedPacket(int iSeq, u8* pOut, int* piOutLen)
{
    const int iSeqBase = miSeqBase;
    int iWindow = miNewestSeq - iSeqBase;               // v8
    if ((iSeq - iSeqBase) < 0 || (iSeq - iSeqBase) >= iWindow)
        return 87;

    // Pick the retained buffer whose sequence base is closest at/below iSeq.
    u8* pBuf = nullptr;
    for (int i = 0; i < 3; ++i)
    {
        const int iDist = iSeq - static_cast<int>(reinterpret_cast<u32*>(mpBuffers[i])[0]);
        if (iDist >= 0 && iDist < iWindow)
        {
            pBuf = mpBuffers[i];
            iWindow = iDist;
        }
    }

    u8** const ppRows = reinterpret_cast<u8**>(mpRowTable);
    u16 uSeq16 = WrapSequence(static_cast<int>(reinterpret_cast<u32*>(pBuf)[0]));
    int iRowIndex   = static_cast<int>(reinterpret_cast<u32*>(pBuf)[1]);   // v13
    int iRemaining  = static_cast<int>(reinterpret_cast<u32*>(pBuf)[3]);   // v16 (frame length)
    int iChunkInRow = static_cast<int>(reinterpret_cast<u32*>(pBuf)[2]);   // v17
    u32 uDataOffset = 0;   // v18 byte offset into the buffer payload
    int iDataChunks = 0;   // v19
    int iRowSpan    = 0;   // v20
    bool bOnData    = true;  // v21
    u8* pRow = ppRows[iRowIndex];

    // Walk forward through the sequence space to the requested unit, tracking
    // whether it lands on a data chunk (bOnData) or a completed parity row.
    for (int iSteps = iWindow; iSteps > 0; --iSteps)
    {
        if (bOnData)
        {
            const u32 uChunkSize = muChunkSize;
            ++iDataChunks;
            iChunkInRow = (iChunkInRow & ~0xFF) | ((iChunkInRow + 1) & 0xFF);
            uDataOffset += uChunkSize;
            iRemaining  -= static_cast<int>(uChunkSize);
        }
        else
        {
            if (static_cast<u32>(++iRowIndex) >= muRowCount)
                iRowIndex = 0;
            iChunkInRow &= ~0xFF;
            ++iRowSpan;
            pRow = ppRows[iRowIndex];
        }

        u16 uNext = static_cast<u16>(uSeq16 + 1);
        if (uNext >= 0xFF78u)
            uNext = 0;
        uSeq16 = uNext;

        // bOnData is set while the current parity row's stored seq differs from
        // the running seq (asm: (cntlzw(rowSeq - seq) & 0x20) == 0).
        const u16 uRowSeq = *reinterpret_cast<u16*>(pRow + 6);
        bOnData = (static_cast<u16>(uRowSeq - uSeq16) != 0);
    }

    if (bOnData)
    {
        // Emit the reconstructed data packet: 8-byte header + payload.
        if (iRemaining >= static_cast<int>(muChunkSize))
            iRemaining = static_cast<int>(muChunkSize);

        pOut[0] = 1;
        pOut[1] = static_cast<u8>((pOut[1] & 0x80) | 1);

        const u8 uMarker = pBuf[0x10];
        u32 uField = reinterpret_cast<u32*>(pOut + 4)[0];
        uField = (uField & 0xFFFFFFCFu) | ((static_cast<u32>(uMarker) << 4) & 0x30u);
        uField = (uField & 0xFFC3FFFFu) | ((static_cast<u32>(iRowSpan) << 18) & 0x3C0000u);

        *reinterpret_cast<u16*>(pOut + 2) = WrapSequence(iSeq);

        uField = (uField & 0xFFFFFFF0u) | (static_cast<u32>(iChunkInRow) & 0xFu);
        if (iRemaining != 0)
        {
            uField = (uField & 0xFFFFF03Fu) | ((static_cast<u32>(iDataChunks) << 6) & 0xFC0u);
            const u32 uChunkSize = muChunkSize;
            const u32 uLenMinus1 = reinterpret_cast<u32*>(pBuf)[3] - 1u;
            const u32 uLow = uField & 0x3C0FFFu;
            uField = ((((uLenMinus1 / uChunkSize) & 0x3Fu)
                        | (static_cast<u32>(iRemaining - 1) << 10)) << 12)
                     | uLow;
            reinterpret_cast<u32*>(pOut + 4)[0] = uField;
            std::memcpy(pOut + 8, pBuf + uDataOffset + 0x11, static_cast<size_t>(iRemaining));
        }
        else
        {
            reinterpret_cast<u32*>(pOut + 4)[0] = uField & 0x3C003Fu;
        }
        *piOutLen = iRemaining + 8;
    }
    else
    {
        // The requested unit IS a completed parity row -- copy it verbatim.
        const int iLen = static_cast<int>(reinterpret_cast<u32*>(pRow)[0]) + 8;
        std::memcpy(pOut, pRow + 4, static_cast<size_t>(iLen));
        *piOutLen = iLen;
    }
    return 0;
}

} // namespace XCAM
