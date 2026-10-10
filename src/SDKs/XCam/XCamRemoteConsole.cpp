#include "SDKs/XCam/XCamRemoteConsole.h"
#include "SDKs/XCam/XCamStreamEngine.h"   // CStreamEngine (mpEngine's subobjects)
#include "SDKs/XCam/XCamOverlappedIO.h"   // COverlappedIO, KU_XCAM_IO_PENDING

#include <cstring>  // memset, memcpy

// ===========================================================================
// XCAM per-peer pipeline, reconstructed store-for-store from the
// console executable. This TU homes every console body -- bring-up
// (Initialize), session re-arm (ReInitialize), the per-frame receive pump
// (DoWork), decoded-chunk hand-off (GetNextDataChunk), frame release
// (ReleaseLastFrameData), outbound packet assembly (GetEncodedPacket) and
// inbound packet routing (SubmitEncodedPacket). `XCAM` is a console SDK boundary,
// so its identifiers are preserved verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

namespace
{
    // Packet header (8 bytes, then payload), as the depacketizer reads it:
    //   +0x00 u8  version (always 1)
    //   +0x01 u8  flags: bits 0..1 packet kind (0 sequence header, 1 data,
    //             2 sync, 3 bare QOS feedback); 0x04 recovery / data loss,
    //             0x08 sequence change, 0x10 key-frame request; 0x20 / 0x40 are
    //             set only by CDepacketizer::AddQOSDataToPacket
    //   +0x02 u16 wrapped sequence number
    //   +0x04 u32 packed metadata, bits 22..31 = payload length
    const u8  KU_PACKET_VERSION          = 1;
    const u8  KU_PACKET_KIND_MASK        = 0x03;
    const u8  KU_PACKET_KIND_SEQHEADER   = 0;
    const u8  KU_PACKET_KIND_DATA        = 1;
    const u8  KU_PACKET_KIND_SYNC        = 2;
    const u8  KU_PACKET_KIND_FEEDBACK    = 3;
    const u8  KU_PACKET_FLAG_RECOVERY    = 0x04;
    const u8  KU_PACKET_FLAG_NEWSEQUENCE = 0x08;
    const u8  KU_PACKET_FLAG_DECODEERROR = 0x10;
    const u8  KU_PACKET_FLAGS_QOS_KEEP   = 0x9F;  // clears the two depacketizer bits
    const int KI_PACKET_HEADER_SIZE      = 8;
    const u32 KU_PACKET_LENGTH_SHIFT     = 22;
    const u32 KU_PACKET_META_LOW_MASK    = 0x3FFFFF;

    // Sequence-header scratch capacity offered to CEncoder::GetSequenceHeader.
    const int KI_SEQUENCE_HEADER_CAPACITY = 64;

    // With nothing new packetized, stay quiet until the encoder has been idle
    // this long (ms); after that fall through to the feedback-only path.
    const u32 KU_ENCODER_IDLE_MS = 1000;

    // The outbound sequence space (the depacketizer's modulus).
    const u32 KU_SEQUENCE_MODULUS = 0xFF78;

    // Raw packet-header access (external wire data, native word order as the
    // depacketizer reads it).
    inline u32 ReadU32(const u8* p) { return *reinterpret_cast<const u32*>(p); }
    inline void WriteU32(u8* p, u32 uValue) { *reinterpret_cast<u32*>(p) = uValue; }
    inline void WriteU16(u8* p, u16 uValue) { *reinterpret_cast<u16*>(p) = uValue; }

    // Reduce a sequence counter into the sequence space, reproducing the asm's
    // branch shape (non-negative -> unsigned modulo; negative -> add the modulus).
    inline u16 WrapSeq(int iValue)
    {
        if (iValue >= 0)
        {
            u32 uValue = static_cast<u32>(iValue);
            if (uValue >= KU_SEQUENCE_MODULUS)
                uValue %= KU_SEQUENCE_MODULUS;
            return static_cast<u16>(uValue);
        }
        return static_cast<u16>(static_cast<u32>(iValue) + KU_SEQUENCE_MODULUS);
    }

    // Set (bSet) or clear one bit of the header flag byte.
    inline u8 WithFlag(u8 uFlags, u8 uBit, bool bSet)
    {
        return static_cast<u8>((uFlags & ~uBit) | (bSet ? uBit : 0));
    }
}

int CRemoteConsole::Initialize(RemoteConsoleConfig* pConfig)
{
    // Empty (self-referential) intrusive list node.
    mLink.mpNext = &mLink;
    mLink.mpPrev = &mLink;

    // The descriptor handed in is the leading block of the owning engine.
    mpEngine = static_cast<CStreamEngine*>(pConfig);

    std::memset(maAddress, 0, sizeof(maAddress));
    maPrivileges[0] = 0;
    maPrivileges[1] = 0;
    maPrivileges[2] = 0;
    maPrivileges[3] = 0;
    miPrivilegeCount = 0;

    mbReceiving = 0;
    mbSending = 0;
    mbDisabled = 0;
    mbSendSequenceHeader = 0;
    muSequenceNumber = 0;

    mbSignalDecodeError = false;
    mbSignalNewSequence = false;
    mbSignalRecovery = false;

    RtlInitializeCriticalSection(&mCriticalSection);

    // The console is itself the decoder's data source (its vtable slot 1 is the
    // decoder's per-frame notify callback).
    const int iResult = mDecoder.Initialize(pConfig->muDimensions, pConfig->miFramerate,
                                            pConfig->muBitrate, this, pConfig->mpDevice);
    if (iResult != 0)
        return iResult;

    return mDepacketizer.Initialize(pConfig->muBitrate, pConfig->muPayloadSize);
}

int CRemoteConsole::ReInitialize(const u64* pPrivileges, u32 uPrivilegeCount, int bSending, int bReceiving)
{
    RtlEnterCriticalSection(&mCriticalSection);

    maPrivileges[0] = 0;
    maPrivileges[1] = 0;
    maPrivileges[2] = 0;
    maPrivileges[3] = 0;
    miPrivilegeCount = static_cast<s32>(uPrivilegeCount);
    if (uPrivilegeCount != 0)
        std::memcpy(maPrivileges, pPrivileges, uPrivilegeCount * sizeof(u64));

    CRemoteConsoleList& lConsoleList = mpEngine->mRemoteConsoleList;
    if (!mbSending && bSending)
    {
        // Starting to send: lead with a sequence header and a key frame from the
        // newest packet on, and count this console into the sending set.
        mbSendSequenceHeader = 1;
        mpEngine->mQOSController.ForceKeyFrame();
        muSequenceNumber = static_cast<u32>(mpEngine->mPacketizer.GetNewestPacketSequenceNumber());
        CStreamEngine* lpListEngine = static_cast<CStreamEngine*>(lConsoleList.mpInitData);
        ++lConsoleList.miActiveCount;
        lpListEngine->mQOSController.ThrottleBitrateDown();
    }
    else if (mbSending && !bSending)
    {
        // Stopping: count it out; the last sender restores the target bit rate.
        if (lConsoleList.miActiveCount != 0 && --lConsoleList.miActiveCount == 0)
        {
            CQOSController& lQOSController =
                static_cast<CStreamEngine*>(lConsoleList.mpInitData)->mQOSController;
            lQOSController.SetActualBitrate(lQOSController.miTargetBitrate);
        }
    }

    const s32 bWasReceiving = mbReceiving;
    mbSending = bSending;
    if (bWasReceiving && !bReceiving)
    {
        mDecoder.Reset(0);
        mDepacketizer.Reset();
        mbSignalDecodeError = false;
        mbSignalNewSequence = false;
    }
    mbReceiving = bReceiving;

    RtlLeaveCriticalSection(&mCriticalSection);
    return mpEngine->UpdatePrivilegeBits();
}

int CRemoteConsole::DoWork()
{
    RtlEnterCriticalSection(&mCriticalSection);

    int iCodecType;
    if (mbReceiving && mDepacketizer.FrameReadyToDecode(&iCodecType))
    {
        mbHasDirectChunk = false;
        const int iResult = mDecoder.DecodeFrame(mbDisabled, iCodecType);
        switch (iResult)
        {
        case 0:
            // Full frame decoded: clear the sequence-change / error feedback.
            mbSignalNewSequence = false;
            mbSignalDecodeError = false;
            break;
        case 3:
            // Sequence change: ask the sender for a fresh sequence.
            mbSignalNewSequence = true;
            break;
        case -100:
        case 1:
        case 4:
        case 11:
            // Async pending / need-more-data / recoverable errors: request a key frame.
            mbSignalDecodeError = true;
            break;
        default:
            // Hard error: drop all feedback and resync the depacketizer.
            mbSignalDecodeError = false;
            mbSignalNewSequence = false;
            mbSignalRecovery = false;
            mDepacketizer.Reset();
            break;
        }
    }

    return RtlLeaveCriticalSection(&mCriticalSection);
}

int CRemoteConsole::GetNextDataChunk(void** ppData, int* piLength, int* pbLast)
{
    if (mbHasDirectChunk)
    {
        // Single directly-submitted (non-FEC) chunk: hand it out whole.
        *ppData = mpDirectChunkData;
        *piLength = static_cast<int>(muDirectChunkLength);
        *pbLast = 0;
        return 10;
    }

    mDepacketizer.GetNextDataChunk(ppData, piLength, pbLast);
    return (*pbLast == 0) ? 10 : 0;
}

int CRemoteConsole::ReleaseLastFrameData()
{
    return mDepacketizer.ReleaseLastReadFrame();
}

int CRemoteConsole::GetEncodedPacket(u8* pPacket, int* piOutLen)
{
    *piOutLen = 0;

    if (mbSending && !mbDisabled)
    {
        if (mbSendSequenceHeader)
        {
            // Sequence header: kind 0, payload straight from the encoder.
            int iHeaderSize = KI_SEQUENCE_HEADER_CAPACITY;
            pPacket[0] = KU_PACKET_VERSION;
            WriteU16(pPacket + 2, WrapSeq(static_cast<u8>(muSequenceNumber)));
            pPacket[1] = static_cast<u8>(pPacket[1] & ~KU_PACKET_KIND_MASK);
            mpEngine->mEncoder.GetSequenceHeader(pPacket + KI_PACKET_HEADER_SIZE, &iHeaderSize);
            WriteU32(pPacket + 4, (ReadU32(pPacket + 4) & KU_PACKET_META_LOW_MASK)
                                      | (static_cast<u32>(iHeaderSize) << KU_PACKET_LENGTH_SHIFT));
            *piOutLen = iHeaderSize + KI_PACKET_HEADER_SIZE;
            mbSendSequenceHeader = 0;
        }
        else
        {
            const u32 uSequence = muSequenceNumber;
            if (uSequence < static_cast<u32>(mpEngine->mPacketizer.GetNewestPacketSequenceNumber()))
            {
                muSequenceNumber = uSequence + 1;
                if (mpEngine->mPacketizer.GetEncodedPacket(static_cast<int>(uSequence), pPacket, piOutLen) != 0)
                {
                    // Fell out of the retained window: ask for a recovery frame,
                    // throttle, and resync to the newest packet.
                    mpEngine->mQOSController.ForceRecoveryFrame();
                    mpEngine->mQOSController.mbDecreaseRequested = 1;
                    muSequenceNumber = static_cast<u32>(mpEngine->mPacketizer.GetNewestPacketSequenceNumber());
                    *piOutLen = 0;
                    return static_cast<int>(KU_XCAM_IO_PENDING);
                }
                mpEngine->mQOSController.SumSendSize(muSequenceNumber, *piOutLen);
            }
            else if (GetTickCount() - mpEngine->mEncoder.muLastEncodeTick < KU_ENCODER_IDLE_MS)
            {
                *piOutLen = 0;
                return static_cast<int>(KU_XCAM_IO_PENDING);
            }
        }
    }

    if (*piOutLen == 0)
    {
        // Nothing to send: on the receive side, emit a bare feedback packet when
        // there is feedback to carry.
        if (!mbReceiving
            || !(mbSignalRecovery || mbSignalNewSequence || mbSignalDecodeError
                 || mDepacketizer.miRequestKeyFrame || mDepacketizer.miResendRequestCount
                 || mDepacketizer.miLostPacketCount))
        {
            *piOutLen = 0;
            return static_cast<int>(KU_XCAM_IO_PENDING);
        }

        muSequenceNumber = static_cast<u32>(mpEngine->mPacketizer.GetNewestPacketSequenceNumber());
        pPacket[0] = KU_PACKET_VERSION;
        WriteU16(pPacket + 2, WrapSeq(static_cast<u8>(muSequenceNumber)));
        pPacket[1] = static_cast<u8>(pPacket[1] | KU_PACKET_KIND_FEEDBACK);
        *piOutLen = KI_PACKET_HEADER_SIZE;
    }

    // Stamp (and consume) the receive-side feedback flags.
    u8 uFlags = pPacket[1];
    uFlags = WithFlag(uFlags, KU_PACKET_FLAG_RECOVERY, mbSignalRecovery);
    uFlags = WithFlag(uFlags, KU_PACKET_FLAG_NEWSEQUENCE, mbSignalNewSequence);
    uFlags = WithFlag(uFlags, KU_PACKET_FLAG_DECODEERROR, mbSignalDecodeError);
    pPacket[1] = static_cast<u8>(uFlags & KU_PACKET_FLAGS_QOS_KEEP);
    mbSignalDecodeError = false;
    mbSignalNewSequence = false;
    mbSignalRecovery = false;
    mDepacketizer.AddQOSDataToPacket(pPacket);
    return 0;
}

int CRemoteConsole::SubmitEncodedPacket(const u8* pPacket, int iLength, XOVERLAPPED* pOverlapped)
{
    if (mbSending)
    {
        mpEngine->mQOSController.ProcessQOSFeedback(pPacket, maAddress);
        if (pPacket[1] & KU_PACKET_FLAG_RECOVERY)
            mbSendSequenceHeader = 1;
    }

    if (mbReceiving)
    {
        const u8 uKind = static_cast<u8>(pPacket[1] & KU_PACKET_KIND_MASK);
        if (uKind != KU_PACKET_KIND_FEEDBACK)
        {
            if (mDecoder.mbSequenceHeaderDecoded)
            {
                if (uKind == KU_PACKET_KIND_DATA || uKind == KU_PACKET_KIND_SYNC)
                    return mDepacketizer.SubmitPacket(pPacket, iLength, pOverlapped);
            }
            else if (uKind != KU_PACKET_KIND_SEQHEADER)
            {
                // Data before any sequence header: signal the loss.
                mbSignalRecovery = true;
            }
            else
            {
                // A sequence header arrives whole: decode it as a direct chunk.
                mpDirectChunkData = const_cast<u8*>(pPacket + KI_PACKET_HEADER_SIZE);
                mbHasDirectChunk = true;
                muDirectChunkLength = ReadU32(pPacket + 4) >> KU_PACKET_LENGTH_SHIFT;
                mbSignalRecovery = (mDecoder.ProcessSequenceHeader() != 0);
            }
        }
    }

    COverlappedIO::SignalSynchronousComplete(pOverlapped, 0, 0, static_cast<u32>(iLength));
    return 0;
}

} // namespace XCAM
