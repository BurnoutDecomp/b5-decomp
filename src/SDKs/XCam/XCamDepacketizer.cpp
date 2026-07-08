#include "SDKs/XCam/XCamDepacketizer.h"

#include <cstring>  // memset
#include <cstdint>  // uintptr_t

// ===========================================================================
// XCAM depacketizer, reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
// This TU homes the receiver-side FEC video depacketizer: it reassembles wire
// packets (data chunks + XOR recovery rows) into frames keyed by sequence
// number, recovers dropped chunks from their parity rows, and paces frames to
// the decoder. See XCamDepacketizer.h for the CDepacketizer layout and the
// serialised wire-packet / recovery-packet formats.
//
// The wire packets and XOR recovery packets are external byte streams, so their
// fields are accessed by documented raw offset (serialised-data exception);
// `this`, the frame slots and the reorder nodes use named members throughout.
//
// The X360 integer-divide guards (`tdllei r,0`) trap divide-by-zero on the
// performance-frequency divisor, which is provably non-zero after
// QueryPerformanceFrequency, so they carry no source-level meaning and are
// omitted. `XCAM` is an X360 SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

// --- serialised little-endian field accessors (external byte streams) --------
static inline u16 ReadU16(const u8* p) { return *reinterpret_cast<const u16*>(p); }
static inline u32 ReadU32(const u8* p) { return *reinterpret_cast<const u32*>(p); }

// The X360 depacketizer stashes the source packet pointer in the async request's
// overlapped +0x08 (InternalContext) so ReleaseOldPacketFromPacketBuffer can
// recover it on completion. This is the platform XDK overlapped's context slot
// (pointer-width on X360); modelled through the named field.
static inline void SetStashedPacket(XOVERLAPPED* pOverlapped, const u8* pPacket)
{
    pOverlapped->InternalContext = static_cast<u32>(reinterpret_cast<uintptr_t>(pPacket));
}
static inline const u8* GetStashedPacket(const XOVERLAPPED* pOverlapped)
{
    return reinterpret_cast<const u8*>(static_cast<uintptr_t>(pOverlapped->InternalContext));
}

// Reduce a sequence counter into the 0xFF78 sequence space, reproducing the exact
// asm branch (non-negative -> unsigned modulo; negative -> add the modulus once,
// which the asm renders as `addis 1 ; addi -0x88` == +0xFF78).
static inline u16 WrapSeq(int iValue)
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

// Reduce an index into the 300-entry frame-slot space (asm: mod 0x12C, then a
// 16-bit truncation).
static inline u32 WrapSlot(int iValue)
{
    if (iValue >= 0)
    {
        u32 uValue = static_cast<u32>(iValue);
        if (uValue >= 0x12Cu)
            uValue %= 0x12Cu;
        return uValue & 0xFFFFu;
    }
    return static_cast<u32>(static_cast<u16>(iValue + 300));
}

// @ 0x829863F0 -- circular/wraparound comparison of two counters (uA, uB) within
// a modulus. Returns 0 if equal, else classifies the difference against
// +/-(modulus/2) so wraparound orders correctly. Used to order frame/packet
// sequence numbers.
int RoundCompare(u32 uA, u32 uB, u32 uModulus)
{
    if (uA == uB)                                 // cmplw eq -> return 0
        return 0;

    if (uA < uB)                                  // unsigned less-than (fall-through of bge)
    {
        // subf r11,r3,r4 = uB-uA ; srwi r10,r5,1 = uModulus>>1 ; cmpw (SIGNED) ; bltlr
        if (static_cast<s32>(uB - uA) < static_cast<s32>(uModulus >> 1))
            return -1;
        return 1;
    }

    // uA > uB: neg(uModulus>>1) ; cmpw (SIGNED) ; blelr
    if (static_cast<s32>(uB - uA) <= -static_cast<s32>(uModulus >> 1))
        return -1;
    return 1;
}

// @ 0x82987E28
int CDepacketizer::Initialize(int iReserved, int iPayloadSize)
{
    (void)iReserved;
    miPayloadSize = iPayloadSize;

    mpFrameSlots = static_cast<FrameSlot*>(
        XMemAlloc(6000, KU_DEPACKETIZER_XMEM_ATTRIBUTES));
    if (!mpFrameSlots)
        return 8;

    muRecoveryCount = 40;
    mpRecoveryTable = static_cast<RecoveryPacket**>(
        XMemAlloc(static_cast<u32>((iPayloadSize + 12) * 40), KU_DEPACKETIZER_XMEM_ATTRIBUTES));
    if (!mpRecoveryTable)
    {
        XMemFree(mpFrameSlots, KU_DEPACKETIZER_XMEM_ATTRIBUTES);
        return 8;
    }

    mpCurrentHeader = nullptr;
    std::memset(mpFrameSlots, 0, 6000);

    // Carve the recovery allocation into a pointer table (muRecoveryCount 4-byte
    // X360 slots) followed by the packet bodies (miPayloadSize+8 each).
    u8* pBody = reinterpret_cast<u8*>(mpRecoveryTable) + muRecoveryCount * 4u;
    for (u32 i = 0; i < muRecoveryCount; ++i)
    {
        mpRecoveryTable[i] = reinterpret_cast<RecoveryPacket*>(pBody);
        pBody += (iPayloadSize + 8);
    }

    Reset();
    QueryPerformanceFrequency(&mPerfFrequency);
    mLastDecodeTime = 0;
    return 0;
}

// @ 0x829877B0
int CDepacketizer::Reset()
{
    ReleasePacketsFromPacketBuffer(0, 299);
    mpCurrentHeader = nullptr;

    // X360 literal sizes: the 15 embedded reorder nodes (600 bytes), the 300-slot
    // frame table (6000 bytes) and every recovery packet body (contiguous).
    std::memset(static_cast<void*>(maHeaders), 0, 600);
    std::memset(mpFrameSlots, 0, 6000);
    std::memset(mpRecoveryTable[0], 0,
                static_cast<size_t>((miPayloadSize + 8) * static_cast<int>(muRecoveryCount)));
    return 0;
}

// @ 0x82987F18
int CDepacketizer::Shutdown()
{
    Reset();
    if (mpFrameSlots)
        XMemFree(mpFrameSlots, KU_DEPACKETIZER_XMEM_ATTRIBUTES);
    if (mpRecoveryTable)
        XMemFree(mpRecoveryTable, KU_DEPACKETIZER_XMEM_ATTRIBUTES);
    return 0;
}

// @ 0x82986D00
FrameSlot* CDepacketizer::ReleaseOldPacketFromPacketBuffer(int iSeq)
{
    FrameSlot* pSlot = &mpFrameSlots[WrapSlot(iSeq)];
    if (pSlot->muType == 2)
    {
        // Async data read still pending -> complete it as "recovered", tagging the
        // code with the source chunk's length field (packet meta bits 22..31 + 9).
        const u8* pPacket = GetStashedPacket(pSlot->mAsync.GetOverlapped());
        pSlot->mAsync.SignalComplete(0, 0, (ReadU32(pPacket + 4) >> 22) + 9u);
        pSlot->muType = 0;
    }
    else if (pSlot->muType == 3)
    {
        pSlot->muType = 0;
    }
    return pSlot;
}

// @ 0x82987118
FrameSlot* CDepacketizer::ReleasePacketsFromPacketBuffer(int iFirst, int iLast)
{
    const u32 uStart = WrapSlot(iFirst);
    u32 uEnd = WrapSlot(iLast + 1);
    if (WrapSeq(iLast - iFirst) >= 0x12Bu)   // span covers (almost) the whole ring
        uEnd = WrapSlot(iFirst - 1);

    FrameSlot* pSlot = &mpFrameSlots[uStart];
    for (u32 i = uStart; i != uEnd; )
    {
        pSlot = ReleaseOldPacketFromPacketBuffer(static_cast<int>(i));
        pSlot->muType = 0;
        if (++i >= 0x12Cu)
            i = 0;
    }
    return pSlot;
}

// @ 0x82987730
int CDepacketizer::ReleaseLastReadFrame()
{
    ReleasePacketsFromPacketBuffer(muCurrentSeq, muCurrentSeq + miReadOffset - 1);
    muCurrentSeq = WrapSeq(muCurrentSeq + miReadOffset);
    miReadOffset = -1;
    // (The X360 tail passes back the release cursor, which no caller consumes.)
    return 0;
}

// @ 0x82986DA8
u8 CDepacketizer::SubmitPacket_Init(const u8* pPacket)
{
    const int iSeq = static_cast<int>(ReadU16(pPacket + 2))
                   - static_cast<int>(ReadU32(pPacket + 4) & 0xFu) - 10;
    muCurrentSeq = WrapSeq(iSeq);
    miReadOffset = -1;

    // Seed the reorder-list nodes and the recovery packets with the base seq.
    for (int k = 0; k < 15; ++k)
        maHeaders[k].muSeq = muCurrentSeq;
    for (u32 k = 0; k < muRecoveryCount; ++k)
        mpRecoveryTable[k]->muSeq = muCurrentSeq;

    mpCurrentHeader = &maHeaders[0];

    u32 uNext = muCurrentSeq + 1u;
    if (uNext >= 0xFF78u)
        uNext = 0;
    muCurrentSeq = static_cast<u16>(uNext);

    mpCurrentHeader->muSeq = muCurrentSeq;
    mpCurrentHeader->mReceiveTime = 0;
    mpFrameSlots[WrapSlot(muCurrentSeq)].muType = 1;

    miMaxLatencyMs = 200;
    const u8 bResult = QueryPerformanceCounter(&mLastDecodeTime);
    miRequestKeyFrame    = 0;
    miLostPacketCount    = 0;
    miResendRequestCount = 0;
    miAvgDecodeMs        = 100;
    return bResult;
}

// @ 0x82987830
int CDepacketizer::SubmitPacket_DataPacket(const u8* pPacket, int iLength, XOVERLAPPED* pOverlapped)
{
    FrameSlot* pSlot = ReleaseOldPacketFromPacketBuffer(ReadU16(pPacket + 2));

    int iResult;
    if ((pPacket[1] & 3) != 1)
    {
        // Synchronously known chunk -> record its sequence in the slot.
        pSlot->muDataSeq = ReadU16(pPacket + 2);
        pSlot->muType = 1;
        iResult = 0;
    }
    else
    {
        // Async overlapped read -> stash the source packet and arm the overlapped.
        SetStashedPacket(pOverlapped, pPacket);
        iResult = static_cast<int>(pSlot->mAsync.Setup(pOverlapped));
        if (iResult == 0)
        {
            pSlot->muType = 2;
            UpdateFrameBuffer(pPacket, iLength - 8);
            iResult = 0;
        }
    }
    return iResult;
}

// @ 0x829878C8
int CDepacketizer::SubmitPacket_Correction(const u8* pPacket, int iLength)
{
    const int iBaseSeq = static_cast<int>(ReadU16(pPacket + 2))
                       - static_cast<int>(ReadU32(pPacket + 4) & 0xFu);
    const u16 uWrappedSeq = WrapSeq(iBaseSeq);
    const u32 uSeq = uWrappedSeq;

    if (RoundCompare(uSeq, mpCurrentHeader->muSeq, 0xFF78) < 0)
        return 0;

    FrameSlot* pSlot = &mpFrameSlots[WrapSlot(static_cast<int>(uSeq))];
    RecoveryPacket* pRec = pSlot->mpRecovery;

    if (!(pRec && pRec->muSeq == uSeq))
    {
        // No live recovery row for this span -> claim a stale one (its seq behind
        // the current reorder head).
        u32 k = 0;
        if (muRecoveryCount != 0)
        {
            do
            {
                pRec = mpRecoveryTable[k];
                if (RoundCompare(pRec->muSeq, mpCurrentHeader->muSeq, 0xFF78) < 0)
                    break;
                ++k;
            }
            while (k < muRecoveryCount);
        }
        if (k == muRecoveryCount)
            return 0;   // nothing reusable

        std::memset(pRec, 0, static_cast<size_t>(miPayloadSize + 8));
        pRec->muMarker = pPacket[0];
        pRec->muFlags  = static_cast<u8>((pRec->muFlags & 0xFC) | 1);
        pRec->muSeq    = uWrappedSeq;
        pSlot->mpRecovery = pRec;
    }

    // Fold this chunk into the recovery row's countdown + received-chunk mask.
    const u32 uChunkBit = ReadU32(pPacket + 4) & 0xFu;
    if ((pPacket[1] & 3) == 1)
    {
        ++pRec->miRemaining;
        pRec->muMask ^= (1u << uChunkBit);
    }
    else
    {
        pRec->miRemaining -= static_cast<s32>(uChunkBit);
        pRec->muMask ^= ((1u << uChunkBit) - 1u);
    }

    if (pRec->miRemaining == 0)
    {
        pSlot->mpRecovery = nullptr;
        return 0;
    }

    // XOR-accumulate the packet metadata (every field but the chunk-in-row nibble)
    // and the payload into the recovery row.
    const u32 uPacketMeta = ReadU32(pPacket + 4);
    pRec->muMeta = (pRec->muMeta & 0xFu) | ((pRec->muMeta ^ uPacketMeta) & 0xFFFFFFF0u);

    const int iPayloadLen = iLength - 8;
    const u8* pSrc = pPacket + 8;
    for (int i = 0; i < iPayloadLen; ++i)
        pRec->mPayload[i] ^= pSrc[i];

    if (pRec->miRemaining == -1)
    {
        // The row's last missing chunk has been recovered -> reconstruct it and
        // post it back into the frame buffer as a recovered slot.
        ++miResendRequestCount;

        u32 uMask = pRec->muMask;
        int iShift = 0;
        while (uMask >>= 1)
            ++iShift;

        pRec->muSeq  = WrapSeq(static_cast<int>(uSeq) + iShift);
        pRec->muMeta = (static_cast<u32>(iShift) & 0xFu) | (pRec->muMeta & 0xFFFFFFF0u);

        FrameSlot* pRecovered = ReleaseOldPacketFromPacketBuffer(static_cast<int>(uSeq) + iShift);
        pRecovered->mpRecovery = pRec;
        pRecovered->muType = 3;
        // The recovered chunk is exposed as a synthetic wire packet at pRec+8.
        UpdateFrameBuffer(reinterpret_cast<const u8*>(pRec) + 8,
                          static_cast<int>((pRec->muMeta >> 22) + 1u));
    }
    return 0;
}

// @ 0x82987C30
int CDepacketizer::SubmitPacket(const u8* pPacket, int iLength, XOVERLAPPED* pOverlapped)
{
    if (mpCurrentHeader == nullptr)
        SubmitPacket_Init(pPacket);

    // Advance the reorder head past filled nodes at/behind the incoming sequence.
    while (mpCurrentHeader->mReceiveTime == 0)
    {
        FrameHeader* pNext = mpCurrentHeader->mpNext;
        if (!pNext || RoundCompare(pNext->muSeq, muCurrentSeq, 0xFF78) > 0)
            break;
        mpCurrentHeader = pNext;
    }

    const u16 uSeq = ReadU16(pPacket + 2);
    if (RoundCompare(uSeq, mpCurrentHeader->muSeq, 0xFF78) < 0)
    {
        // Older than the window -> drop and request a resend.
        ++miResendRequestCount;
        COverlappedIO::SignalSynchronousComplete(pOverlapped, 0, 0, static_cast<u32>(iLength));
        return 0;
    }

    if (RoundCompare(WrapSeq(mpCurrentHeader->muSeq + 300), uSeq, 0xFF78) <= 0)
    {
        // Too far ahead -> park it in the frame buffer, mark unfilled, resend.
        FrameHeader* pUpdated = UpdateFrameBuffer(pPacket, 0);
        if (pUpdated)
            pUpdated->mReceiveTime = 0;
        ++miResendRequestCount;
        COverlappedIO::SignalSynchronousComplete(pOverlapped, 0, 0, static_cast<u32>(iLength));
        return 0;
    }

    // Within the window: reject an exact duplicate, else route to the handlers.
    FrameSlot* pSlot = &mpFrameSlots[WrapSlot(static_cast<int>(uSeq))];
    if ((pSlot->muType == 2 &&
         uSeq == ReadU16(GetStashedPacket(pSlot->mAsync.GetOverlapped()) + 2)) ||
        (pSlot->muType == 1 && uSeq == pSlot->muDataSeq))
    {
        COverlappedIO::SignalSynchronousComplete(pOverlapped, 0, 0, static_cast<u32>(iLength));
        return 13;
    }

    const bool bSyncComplete = (pPacket[1] & 3) == 2;
    const int iResult = SubmitPacket_DataPacket(pPacket, iLength, pOverlapped);
    SubmitPacket_Correction(pPacket, iLength);
    if (bSyncComplete)
        COverlappedIO::SignalSynchronousComplete(pOverlapped, 0, 0, static_cast<u32>(iLength));
    return iResult;
}

// @ 0x82987228
int CDepacketizer::FrameReadyToDecode(int* piCodecType)
{
    s64 iNow;
    QueryPerformanceCounter(&iNow);

    int iFrameReady = 0;
    int iSeq = muCurrentSeq;
    FrameHeader* pNode = mpCurrentHeader;
    const int iMsSinceDecode =
        static_cast<int>((iNow - mLastDecodeTime) * 1000 / mPerfFrequency);
    int iNodeLatencyMs = 0;

    // Walk the reorder list looking for a complete, in-budget frame.
    while (pNode)
    {
        const s64 iReceive = pNode->mReceiveTime;
        iSeq = pNode->muSeq;
        if (iReceive != 0)
        {
            iNodeLatencyMs = static_cast<int>((iNow - iReceive) * 1000 / mPerfFrequency);
            if (iNodeLatencyMs >= miMaxLatencyMs || iMsSinceDecode >= 3 * miAvgDecodeMs)
            {
                if (pNode->mExpected == pNode->mReceived)
                {
                    iFrameReady = 1;
                    break;   // frame complete -> common tail
                }
                ++miLostPacketCount;   // incomplete -> keep scanning
            }
            else
            {
                // Not yet due: nudge the play cursor onto the next still-unfilled
                // node and report "nothing ready this tick".
                if (mpCurrentHeader->muSeq == muCurrentSeq)
                {
                    FrameHeader* pNext = mpCurrentHeader->mpNext;
                    if (pNext && pNext->mReceiveTime == 0)
                        muCurrentSeq = pNext->muSeq;
                }
                return 0;
            }
        }
        FrameHeader* pNext = pNode->mpNext;
        pNode->mReceiveTime = 0;
        pNode = pNext;
    }

    // Common tail: reconcile the play window against the selected sequence.
    if (RoundCompare(WrapSeq(iSeq), muCurrentSeq, 0xFF78) < 0)
    {
        if (pNode)
            pNode->mReceiveTime = 0;
        return 0;
    }

    if (muCurrentSeq != static_cast<u16>(iSeq))
    {
        if (pNode && pNode->mpData)
        {
            if (mpFrameSlots[WrapSlot(muCurrentSeq)].muType != 1)
                ++miLostPacketCount;
        }
        ReleasePacketsFromPacketBuffer(muCurrentSeq, iSeq - 1);
        muCurrentSeq = WrapSeq(iSeq);
    }

    if (!(pNode && iNodeLatencyMs < 500 && iMsSinceDecode < 500))
    {
        if (mpCurrentHeader)
            miRequestKeyFrame = 1;
        if (!pNode)
            return 0;
    }

    // Adopt the node's sequence as the frame to decode.
    muCurrentSeq = pNode->muSeq;
    if (iFrameReady == 0)
        return 0;

    // Frame ready: fold this interval into the decode-time average and re-stamp.
    miAvgDecodeMs = (15 * miAvgDecodeMs + iMsSinceDecode + 8) >> 4;
    mLastDecodeTime = iNow;
    pNode->mReceiveTime = 0;

    FrameSlot* pSlot = &mpFrameSlots[WrapSlot(pNode->muSeq)];
    if (pNode->mpData == nullptr)
    {
        ReleaseOldPacketFromPacketBuffer(pNode->muSeq);
        u32 uNext = muCurrentSeq + 1u;
        if (uNext >= 0xFF78u)
            uNext = 0;
        muCurrentSeq = static_cast<u16>(uNext);
        return 0;
    }

    const u8* pPacket = (pSlot->muType == 2)
                      ? GetStashedPacket(pSlot->mAsync.GetOverlapped())
                      : (reinterpret_cast<const u8*>(pSlot->mpRecovery) + 8);
    *piCodecType = (ReadU32(pPacket + 4) >> 4) & 3;
    miReadOffset = 0;
    return iFrameReady;
}

// @ 0x82987570
CDepacketizer* CDepacketizer::GetNextDataChunk(void** ppData, int* piLength, int* pbLast)
{
    const int iTotal = static_cast<int>(muCurrentSeq) + miReadOffset;
    FrameSlot* pSlot = &mpFrameSlots[WrapSlot(iTotal)];

    if (pSlot->muType >= 2)
    {
        const u8* pPkt = (pSlot->muType == 2)
                       ? GetStashedPacket(pSlot->mAsync.GetOverlapped())
                       : (reinterpret_cast<const u8*>(pSlot->mpRecovery) + 8);
        if (ReadU16(pPkt + 2) != WrapSeq(iTotal))
        {
            // Slot holds the wrong chunk -> release it and step to the next.
            ReleaseOldPacketFromPacketBuffer(iTotal);
            miReadOffset += 1;
            pSlot = &mpFrameSlots[WrapSlot(static_cast<int>(muCurrentSeq) + miReadOffset)];
        }
    }
    else
    {
        miReadOffset += 1;
        pSlot = &mpFrameSlots[WrapSlot(static_cast<int>(muCurrentSeq) + miReadOffset)];
    }

    miReadOffset += 1;
    const u8* pPkt = (pSlot->muType == 2)
                   ? GetStashedPacket(pSlot->mAsync.GetOverlapped())
                   : (reinterpret_cast<const u8*>(pSlot->mpRecovery) + 8);
    *ppData   = const_cast<u8*>(pPkt + 8);
    *piLength = static_cast<int>(ReadU32(pPkt + 4) >> 22) + 1;
    // asm: (cntlzw(((meta>>6) ^ meta) & 0xFC0) & 0x20) == 0  <=>  the field is non-zero.
    *pbLast   = ((((ReadU32(pPkt + 4) >> 6) ^ ReadU32(pPkt + 4)) & 0xFC0u) != 0) ? 1 : 0;
    return this;
}

// @ 0x829866C8
CDepacketizer* CDepacketizer::AddQOSDataToPacket(u8* pPacket)
{
    if (miLostPacketCount != 0)
    {
        if (miLostPacketCount <= 5)
            pPacket[1] |= 0x10;
        else
            pPacket[1] |= 0x08;
        miLostPacketCount = 0;
        miRequestKeyFrame = 1;
    }
    if (miResendRequestCount != 0)
    {
        pPacket[1] |= 0x20;
        --miResendRequestCount;
    }
    if (miRequestKeyFrame != 0)
    {
        pPacket[1] |= 0x40;
        miRequestKeyFrame = 0;
    }
    return this;
}

} // namespace XCAM
