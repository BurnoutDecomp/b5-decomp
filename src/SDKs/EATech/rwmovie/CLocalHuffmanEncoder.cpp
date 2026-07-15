// =====================================================================================
// CLocalHuffmanEncoder.cpp -- per-table variable-length-code *encoder* for the RTCMV/WMV
// video path. CAltTablesEncoder owns eleven of these; each measures a frame, buffers its
// motion-vector / literal symbol records, and finally emits them through the code table its
// selector names. Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC asm authoritative).
//
//   CLocalHuffmanEncoder::encodeHeader   @0x82A7DEE8
//   CLocalHuffmanEncoder::encodeSymbol   @0x82A7E030
//   CLocalHuffmanEncoder::WriteSymbol    @0x82A7E128
//   CLocalHuffmanEncoder::checkInMV      @0x82A7E410
//   CLocalHuffmanEncoder::checkInRTCMV   @0x82A7E580
//   CLocalHuffmanEncoder::checkFrame     @0x82A7E6D0
//
// Byte-addressed in the asm (this[N] == byte offset N*4); pointer members widen on this
// PC/x64 target, so the documented offsets are documentary and all access is by name. The
// symbol record, the owning-encoder dirty/config words and the walk cursor are reached
// through CLocalHuffmanEncoder::mpState (the owning CAltTablesEncoder, modelled by the opaque
// SLocalHuffmanEncoderState in the header to keep the include cycle open).
// =====================================================================================

#include "SDKs/EATech/rwmovie/CLocalHuffmanEncoder.h"

#include "SDKs/EATech/rwmovie/COutBitStream.h"

// Foreign codec global (X360 dword_8322C3F0): 1 when the active table mode is "short" (<= 5),
// else 0. Owned/written by CAltTablesEncoder::clear; declared extern here (same symbol) so the
// encode path reads the same value. It biases the emitted code length by one extra bit.
extern int g_iAltTablesShortCodeFlag;

// Length-limited canonical-Huffman builder shared by the rwmovie codec: given the per-symbol
// code-length array (lpauLengths, luNumSymbols entries) it writes the canonical code words into
// lpauCodesOut and returns a pointer into the rebuilt table (r3, discarded by encodeHeader).
// A separate, not-yet-reconstructed X360 TU (generateHuffman_balanced @todo).
extern u32* generateHuffman_balanced(u32* lpauLengths, int liA2, u32 luNumSymbols,
                                     u32* lpauCodesOut, int liA5);

// -------------------------------------------------------------------------------------
// Per-motion-vector-component variable-length-code lookups (X360 sub_82A7E1A0 /
// sub_82A7E258). Each takes one signed MV component and writes three ints -- a symbol-index
// contribution, the packed code bits, and the shift/length -- that the check-in packers below
// combine into the 32-bit symbol record. Their bodies live between WriteSymbol and checkInMV
// in the XEX but are NOT in this reconstruction packet (no asm recovered), so they are
// declared here and defined by a separate rwmovie TU. FLAG: un-recovered helper bodies.
// The RTCMV variant additionally takes the MV search-range bit count.
// -------------------------------------------------------------------------------------
extern int EncodeMvComponent(int liComponent, int* lpiIndexOut, int* lpiCodeOut,
                             int* lpiShiftOut);
extern int EncodeRtcmvComponent(int liComponent, int* lpiIndexOut, int* lpiCodeOut,
                                int* lpiShiftOut, int liBits);

// -------------------------------------------------------------------------------------
// CLocalHuffmanEncoder::encodeHeader  @0x82A7DEE8
//
// Emit the per-table frame header ahead of the first symbol, then rebuild the code words for
// the selected code set. When the table has more than one candidate code set and the owning
// encoder is not "dirty", a one-bit "code-set changed" flag is written; if it did change (or
// the encoder is dirty) the selected code index follows in muSecondaryBits bits. Finally the
// selected candidate's packed code-length pairs are unpacked into mpauCodeData (+1 per nibble)
// and handed to generateHuffman_balanced, whose result (r3) the caller discards.
// -------------------------------------------------------------------------------------
u32* CLocalHuffmanEncoder::encodeHeader(void* lpStream)
{
    COutBitStream* lpBitStream = static_cast<COutBitStream*>(lpStream);

    if (muSecondaryTableSize > 1)
    {
        bool lbEmitCode = true;
        if (mpState->muDirty == 0)
        {
            if (muCurrentCode == muReferenceCode)
            {
                lpBitStream->putBits(0, 1);   // "unchanged" -> no code index follows
                lbEmitCode = false;
            }
            else
            {
                lpBitStream->putBits(1, 1);   // "changed" -> the code index follows
            }
        }
        if (lbEmitCode)
            lpBitStream->putBits(muCurrentCode, muSecondaryBits);
    }

    // Unpack the selected candidate's code-length nibbles (two symbols per packed byte) into
    // mpauCodeData, each biased by +1, then build the canonical code words.
    const int liNumSymbols = static_cast<int>(muNumSymbols);
    const int liStride      = (liNumSymbols + 1) >> 1;
    const u8* lpPack        = mpNextByte + liStride * muCurrentCode;

    muEmittedCode = muCurrentCode;

    int liSym = 0;
    while (liSym < liNumSymbols - 1)
    {
        u8 luPack = *lpPack++;
        mpauCodeData[liSym]     = (luPack & 0xF) + 1;
        mpauCodeData[liSym + 1] = (luPack >> 4) + 1;
        liSym += 2;
    }
    if (liSym < liNumSymbols)
        mpauCodeData[liSym] = (*lpPack & 0xF) + 1;

    return generateHuffman_balanced(mpauCodeData, 1, muNumSymbols, mpauCodes, 1);
}

// -------------------------------------------------------------------------------------
// CLocalHuffmanEncoder::encodeSymbol  @0x82A7E030
//
// Emit the current buffered symbol through the VLC table (writing the header first the once).
// The record's leading byte is the symbol index; its bits 1..17 carry an extra literal field.
// muBits selects the packing: for < 4 the code word is shifted up by (mpaCodeLengths[sym] +
// short-code-flag + 1) and OR'd with the extra field; for > 4 the same without the +1; for
// exactly 4 the raw code word is emitted with no extra field.
// -------------------------------------------------------------------------------------
int CLocalHuffmanEncoder::encodeSymbol(void* lpStream)
{
    COutBitStream* lpBitStream = static_cast<COutBitStream*>(lpStream);

    if (muNeedHeader)
    {
        encodeHeader(lpStream);
        muNeedHeader = 0;
    }

    const int liBits    = static_cast<int>(muBits);
    u32*      lpRecord  = mpState->mpuSymbolRecord;
    const u32 luSymbol  = static_cast<u8>(*lpRecord >> 24);   // leading byte = symbol index
    const u32 luExtra   = (*lpRecord >> 1) & 0x1FFFF;
    const u32 luCode    = mpauCodes[luSymbol];

    u32 luValue;
    u32 luNumBits;
    if (liBits < 4)
    {
        const int liExtraLen = mpaCodeLengths[luSymbol] + g_iAltTablesShortCodeFlag + 1;
        luNumBits = mpauCodeData[luSymbol] + liExtraLen;
        luValue   = (luCode << liExtraLen) | luExtra;
    }
    else if (liBits != 4)
    {
        const int liExtraLen = mpaCodeLengths[luSymbol] + g_iAltTablesShortCodeFlag;
        luNumBits = mpauCodeData[luSymbol] + liExtraLen;
        luValue   = (luCode << liExtraLen) | luExtra;
    }
    else
    {
        luNumBits = mpauCodeData[luSymbol];
        luValue   = luCode;
    }

    lpBitStream->putBits(luValue, luNumBits);
    return 0;
}

// -------------------------------------------------------------------------------------
// CLocalHuffmanEncoder::WriteSymbol  @0x82A7E128
//
// Emit a literal/escape symbol record. Unless muBits == 10 (which emits nothing), the record's
// code word / length pair is written; when muBits == 8 the record's bits 1..17 are additionally
// emitted in mpaCodeLengths[sym] bits. Returns the last COutBitStream* (r3), or `this` on the
// muBits == 10 path; the caller discards it.
// -------------------------------------------------------------------------------------
u32* CLocalHuffmanEncoder::WriteSymbol(void* lpStream)
{
    COutBitStream* lpBitStream = static_cast<COutBitStream*>(lpStream);

    u32*      lpRecord = mpState->mpuSymbolRecord;
    const u32 luSymbol = static_cast<u8>(*lpRecord >> 24);   // leading byte = symbol index

    void* lpResult = this;   // r3 is left as `this` when muBits == 10 emits nothing
    if (muBits != 10)
    {
        lpResult = lpBitStream->putBits(mpauCodes[luSymbol], mpauCodeData[luSymbol]);
        if (muBits == 8)
            lpResult = lpBitStream->putBits((*lpRecord >> 1) & 0x1FFFF, mpaCodeLengths[luSymbol]);
    }
    return reinterpret_cast<u32*>(lpResult);
}

// -------------------------------------------------------------------------------------
// CLocalHuffmanEncoder::checkInMV  @0x82A7E410
//
// Buffer one non-RTC motion-vector symbol record. If the context marks an escape (bit 2) the
// symbol is 36; if either MV component is out of the +/-((128>>searchBits)+30) range it is 35;
// otherwise the two components are VLC-coded and packed into the record (index/code/shift). For
// muBits == 8 the symbol index is biased (+37 when bit 3 is set) and decremented. The record's
// leading byte is finally set to the symbol index, that symbol's occurrence count is bumped,
// and the table's checked-in count advances.
// -------------------------------------------------------------------------------------
int CLocalHuffmanEncoder::checkInMV(void* lpContext)
{
    const int liMotion = *static_cast<int*>(lpContext);
    u32*      lpRecord = mpState->mpuSymbolRecord;

    int liSymbol;
    if ((liMotion & 4) != 0)
    {
        liSymbol   = 36;
        *lpRecord &= 0xFFFC0001u;
    }
    else
    {
        const int liRange = (128 >> mpState->muMvSearchBits) + 30;
        const int liCompX = liMotion >> 16;
        const int liCompY = (liMotion << 16) >> 20;
        if (liCompX > liRange || liCompX < -liRange || liCompY > liRange || liCompY < -liRange)
        {
            liSymbol   = 35;
            *lpRecord &= 0xFFFC0001u;
        }
        else
        {
            int liIdx1, liCode1, liShift1;
            int liIdx2, liCode2, liShift2;
            EncodeMvComponent(liCompX, &liIdx1, &liCode1, &liShift1);
            EncodeMvComponent(liCompY, &liIdx2, &liCode2, &liShift2);

            liSymbol = 6 * liIdx2 + liIdx1;

            int liShift = liShift2;
            if (mpState->muMvSearchBits != 0)
                liShift = liShift2 - (liIdx2 == 5 ? 1 : 0);

            *lpRecord = ((static_cast<u32>(liShift + liShift1) & 0x3Fu) << 18)
                      | ((static_cast<u32>((liCode1 << liShift) | liCode2) & 0x1FFFFu) << 1)
                      | (*lpRecord & 0xFF000001u);
        }
    }

    if (muBits == 8)
    {
        if ((liMotion & 8) != 0)
            liSymbol += 37;
        --liSymbol;
    }

    *lpRecord = (*lpRecord & 0x00FFFFFFu) | ((static_cast<u32>(liSymbol) & 0xFFu) << 24);
    ++mpauCodeBuffer[liSymbol];
    ++muDirtyFlag;
    return 0;
}

// -------------------------------------------------------------------------------------
// CLocalHuffmanEncoder::checkInRTCMV  @0x82A7E580
//
// Buffer one RTCMV motion-vector symbol record. Escape (bit 2) -> symbol 37; out-of-range
// (valid is -range <= comp < range) -> symbol 36; otherwise both components are VLC-coded with
// the search-range bit count and packed. Bit 3 biases the symbol by +38. The record's leading
// byte is set to (symbol - 1), that symbol's occurrence count is bumped, and the checked-in
// count advances.
// -------------------------------------------------------------------------------------
int CLocalHuffmanEncoder::checkInRTCMV(void* lpContext)
{
    const int liMotion = *static_cast<int*>(lpContext);
    u32*      lpRecord = mpState->mpuSymbolRecord;

    int liSymbol;
    if ((liMotion & 4) != 0)
    {
        liSymbol   = 37;
        *lpRecord &= 0xFFFC0001u;
    }
    else
    {
        const int liBits  = static_cast<int>(mpState->muMvSearchBits);
        const int liRange = (128 >> liBits) + 30;
        const int liCompX = liMotion >> 16;
        const int liCompY = (liMotion << 16) >> 20;
        if (liCompX < liRange && liCompX >= -liRange &&
            liCompY < liRange && liCompY >= -liRange)
        {
            int liIdx1, liCode1, liShift1;
            int liIdx2, liCode2, liShift2;
            EncodeRtcmvComponent(liCompX, &liIdx1, &liCode1, &liShift1, liBits);
            EncodeRtcmvComponent(liCompY, &liIdx2, &liCode2, &liShift2, liBits);

            liSymbol = 6 * liIdx2 + liIdx1;

            *lpRecord = ((static_cast<u32>(liShift2 + liShift1) & 0x3Fu) << 18)
                      | ((static_cast<u32>((liCode1 << liShift2) | liCode2) & 0x1FFFFu) << 1)
                      | (*lpRecord & 0xFF000001u);
        }
        else
        {
            liSymbol   = 36;
            *lpRecord &= 0xFFFC0001u;
        }
    }

    if ((liMotion & 8) != 0)
        liSymbol += 38;

    *lpRecord = (*lpRecord & 0x00FFFFFFu) | ((static_cast<u32>(liSymbol - 1) & 0xFFu) << 24);
    ++mpauCodeBuffer[liSymbol - 1];
    ++muDirtyFlag;
    return 0;
}

// -------------------------------------------------------------------------------------
// CLocalHuffmanEncoder::checkFrame  @0x82A7E6D0
//
// Measure the frame this table contributed to and pick the cheapest code set. Nothing was
// checked in (muDirtyFlag == 0) -> cost 0. Otherwise every candidate code set is priced by
// summing, over all symbols, (codeLength + packedExtra + inc) * occurrenceCount, using each
// candidate's slice of the packed length table. The reference (history) candidate is always
// fully priced. The cheapest candidate becomes muCurrentCode, unless the reference beats it by
// the muSecondaryBits (default 4) margin, in which case the reference code is kept. For the
// 8-bit table with the MV flag set, mpaCodeLengths advances one symbol block. Returns the best
// cost.
// -------------------------------------------------------------------------------------
int CLocalHuffmanEncoder::checkFrame()
{
    muReferenceCode = muHistoryCode;
    if (mpState->muDirty)
    {
        muCurrentCode   = 0;
        muReferenceCode = 0;
    }

    if (muDirtyFlag == 0)
        return 0;

    const int liBits         = static_cast<int>(muBits);
    const int liNumCandidates = static_cast<int>(muSecondaryTableSize);
    const int liNumSymbols    = static_cast<int>(muNumSymbols);
    const int liRefIndex      = static_cast<int>(muReferenceCode);
    const int liBaseInc       = (liBits <= 4) ? 2 : 1;
    const int liStride        = (liNumSymbols + 1) >> 1;

    muNeedHeader = 1;

    int liBestCost = 0x7FFFFFFF;
    int liRefCost  = 0;

    for (int liCand = 0; liCand < liNumCandidates; ++liCand)
    {
        int liInc = liBaseInc;
        if (liBits == 8 && (liCand == 35 || liCand == 36 || liCand == 72))
            liInc = 1;

        const u8* lpPacks = mpNextByte + liCand * liStride;

        int liCost = 0;
        int liSym  = 0;
        while ((liCost < liBestCost || liCand == liRefIndex) && liSym < liNumSymbols - 1)
        {
            u8 luPack = lpPacks[liSym >> 1];
            liCost += (mpaCodeLengths[liSym]     + (luPack & 0xF) + liInc)
                      * static_cast<int>(mpauCodeBuffer[liSym]);
            liCost += (mpaCodeLengths[liSym + 1] + (luPack >> 4)  + liInc)
                      * static_cast<int>(mpauCodeBuffer[liSym + 1]);
            liSym += 2;
        }

        if (liCost < liBestCost || liCand == liRefIndex)
        {
            if (liSym < liNumSymbols)
            {
                u8 luPack = lpPacks[liSym >> 1];
                liCost += ((luPack & 0xF) + mpaCodeLengths[liSym] + liInc)
                          * static_cast<int>(mpauCodeBuffer[liSym]);
                ++liSym;
            }
            if (liCand == liRefIndex)
                liRefCost = liCost;
        }

        if (liSym >= liNumSymbols && liCost < liBestCost)
        {
            liBestCost    = liCost;
            muCurrentCode = static_cast<u32>(liCand);
        }
    }

    int liMargin = static_cast<int>(muSecondaryBits);
    if (liMargin == 0)
        liMargin = 4;
    if (liRefCost - liMargin - liBestCost <= 0)
        muCurrentCode = muReferenceCode;

    if (liBits == 8 && mpState->muMvSearchBits != 0)
        mpaCodeLengths += muNumSymbols;

    return liBestCost;
}
