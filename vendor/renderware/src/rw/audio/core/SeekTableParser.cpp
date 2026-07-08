// =====================================================================================
// rw::audio::core::SeekTableParser::ParseChunkSection0 -- the inner 16-byte chunk-section
// copy step. Reconstructed from BURNOUT_X360_ARTIST.XEX @0x82B6E880; the PowerPC asm is
// authoritative. No reference source and no DecFIGS DWARF exist for this TU.
//
// The asm is 16 paired lbz r11,N(r4) / stb r11,N(r3) for N = 0..0xF, then blr returning
// r3. It is a flat 16-byte byte-for-byte copy of the source chunk-section block into the
// destination, returning the destination pointer.
// =====================================================================================

#include "rw/audio/core/SeekTableParser.h"
#include "rw/audio/core/PackedTableReader.h"

namespace rw
{
namespace audio
{
namespace core
{

// ---------------------------------------------------------------------------
// SeekTableParser::ParseChunkSection0 (copy helper) @ 0x82B6E880
//
// The private per-row read step: copy a 16-byte chunk-section record from apSource
// into apDest byte-for-byte (16 lbz/stb pairs) and return apDest.
// ---------------------------------------------------------------------------
ChunkSection0* SeekTableParser::ParseChunkSection0(ChunkSection0* apDest,
                                                   const ChunkSection0* apSource)
{
    // 16 byte-for-byte stores (lbz/stb at displacements 0..0xF), return r3 (apDest).
    for (int li = 0; li < 16; ++li)
        apDest->mau8Bytes[li] = apSource->mau8Bytes[li];
    return apDest;
}

// ---------------------------------------------------------------------------
// SeekTableParser::ParseChunkSection0 (table walk) @ 0x82B6F328
//
// Walk the raw 16-byte chunk-section records at apRecords, resolving the chunk that
// contains aTargetPos into the state members. Each row is copied out via the copy
// helper above. Returns false the moment aTargetPos is covered by the accumulated
// length (the chunk was found), or true if the length-terminator (negative length)
// is hit first (target beyond the table).
// ---------------------------------------------------------------------------
bool SeekTableParser::ParseChunkSection0(u8* apRecords, s32 aTargetPos)
{
    const s32 lTargetInChunk = (aTargetPos - mLatency > 0) ? (aTargetPos - mLatency) : 0;

    ChunkSection0 lRow;
    ParseChunkSection0(&lRow, reinterpret_cast<const ChunkSection0*>(apRecords));

    u8* const lpSeekBase = mpSeekData; // captured before the loop rewrites mpSeekData
    s32 lAccChunkOffset = 0;
    s32 lAccSeekOffset  = 0;
    s32 lPos            = 0;
    bool lNotFound      = true;

    while (lRow.miLength >= 0)
    {
        const s32 lFlag = lRow.miFlag;
        if ((lPos <= lTargetInChunk && lTargetInChunk < lRow.miLength + lPos) || lFlag == 1)
        {
            s32 lPlayerSkip = mLatency;
            mStreamSkip = lPos;
            mpSeekData  = lpSeekBase + lAccSeekOffset;
            if (aTargetPos - lPos < lPlayerSkip)
                lPlayerSkip = aTargetPos - lPos;
            mPlayerSkip     = lPlayerSkip;
            mChunkOffset    = lAccChunkOffset;
            mDecoderSkip    = aTargetPos - lPlayerSkip - lPos;
            mIsNewFeedChunk = (lFlag == 1) ? 1 : 0;
        }

        lPos += lRow.miLength;
        if (aTargetPos < lPos)
        {
            lNotFound = false;
            break;
        }

        lAccChunkOffset += lRow.miChunkOffsetDelta;
        lAccSeekOffset  += lRow.miSeekOffsetDelta;

        apRecords += 16;
        ParseChunkSection0(&lRow, reinterpret_cast<const ChunkSection0*>(apRecords));
    }
    return lNotFound;
}

// ---------------------------------------------------------------------------
// SeekTableParser::ParseChunkSection1 @ 0x82B6E908
//
// Identical walk to ParseChunkSection0, except the four columns of each row are
// pulled from a PackedTableReader over the packed column stream at apTableData
// (the reader's constructor is inlined here in the X360 asm).
// ---------------------------------------------------------------------------
bool SeekTableParser::ParseChunkSection1(u8* apTableData, s32 aTargetPos)
{
    const s32 lTargetInChunk = (aTargetPos - mLatency > 0) ? (aTargetPos - mLatency) : 0;

    PackedTableReader lReader(apTableData);
    s32 lRow[4];
    lReader.UnpackNextRow(lRow); // lRow[0]=chunk-offset delta, [1]=seek delta, [2]=length, [3]=flag

    u8* const lpSeekBase = mpSeekData;
    s32 lAccChunkOffset = 0;
    s32 lAccSeekOffset  = 0;
    s32 lPos            = 0;
    bool lNotFound      = true;

    while (lRow[2] >= 0)
    {
        const s32 lFlag = lRow[3];
        if ((lPos <= lTargetInChunk && lTargetInChunk < lRow[2] + lPos) || lFlag == 1)
        {
            s32 lPlayerSkip = mLatency;
            mStreamSkip = lPos;
            mpSeekData  = lpSeekBase + lAccSeekOffset;
            if (aTargetPos - lPos < lPlayerSkip)
                lPlayerSkip = aTargetPos - lPos;
            mPlayerSkip     = lPlayerSkip;
            mChunkOffset    = lAccChunkOffset;
            mDecoderSkip    = aTargetPos - lPlayerSkip - lPos;
            mIsNewFeedChunk = (lFlag == 1) ? 1 : 0;
        }

        lPos += lRow[2];
        if (aTargetPos < lPos)
        {
            lNotFound = false;
            break;
        }

        lAccChunkOffset += lRow[0];
        lAccSeekOffset  += lRow[1];
        lReader.UnpackNextRow(lRow);
    }
    return lNotFound;
}

// ---------------------------------------------------------------------------
// SeekTableParser::ParseHeader0 @ 0x82B6F8E0
//
// Read the 8-byte header out of apSeekData:
//   [1]     version byte      -> mSeekDataVersion; (version >> 4) selects the section kind
//   [2..3]  latency halfword  -> mLatency        (big-endian)
//   [4..7]  seek-data offset  -> mpSeekData = apSeekData + offset (or null)  (big-endian)
// then dispatch to the matching chunk-section walk over the table at apSeekData + 8.
// ---------------------------------------------------------------------------
bool SeekTableParser::ParseHeader0(u8* apSeekData, s32 aTargetPos)
{
    const u8 lVersionByte = apSeekData[1];
    mSeekDataVersion = lVersionByte;
    // (s8)version >> 4, then byte-masked -- matches extsb/srawi/clrlwi in the asm.
    const s32 lSectionKind = (static_cast<s8>(lVersionByte) >> 4) & 0xFF;

    // 16-bit big-endian latency at [2..3] (lhz of the assembled bytes).
    mLatency = (static_cast<s32>(apSeekData[2]) << 8) | static_cast<s32>(apSeekData[3]);

    // 32-bit big-endian seek-data offset at [4..7] (lwz of the assembled bytes).
    const u32 lSeekOffset = (static_cast<u32>(apSeekData[4]) << 24)
                          | (static_cast<u32>(apSeekData[5]) << 16)
                          | (static_cast<u32>(apSeekData[6]) << 8)
                          |  static_cast<u32>(apSeekData[7]);
    mpSeekData = lSeekOffset ? (apSeekData + lSeekOffset) : nullptr;

    u8* const lpSection = apSeekData + 8;
    if (lSectionKind == 0)
        return ParseChunkSection0(lpSection, aTargetPos);
    if (lSectionKind == 1)
        return ParseChunkSection1(lpSection, aTargetPos);
    return false;
}

// ---------------------------------------------------------------------------
// SeekTableParser::Parse @ 0x82B6FBD8
//
// Entry point. A non-zero first blob byte means "no usable seek table" -> not
// resolved (true); otherwise parse the header. When the result is true (target not
// resolved within the table) reset the resolved output members (mpSeekData,
// mStreamSkip, mDecoderSkip, mChunkOffset, mSeekDataVersion); mPlayerSkip, mLatency
// and mIsNewFeedChunk are deliberately left untouched (as in the asm).
// ---------------------------------------------------------------------------
bool SeekTableParser::Parse(u8* apSeekData, s32 aTargetPos)
{
    bool lNotResolved;
    if (apSeekData[0])
        lNotResolved = true;
    else
        lNotResolved = ParseHeader0(apSeekData, aTargetPos);

    if (lNotResolved)
    {
        mpSeekData       = nullptr;
        mStreamSkip      = 0;
        mDecoderSkip     = 0;
        mChunkOffset     = 0;
        mSeekDataVersion = 0;
    }
    return lNotResolved;
}

} // namespace core
} // namespace audio
} // namespace rw
