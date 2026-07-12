#include "SDKs/XCam/XCamRemoteConsole.h"

#include <cstring>  // memset

// ===========================================================================
// XCAM per-peer receive pipeline, reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. This TU homes the console-local bodies -- bring-up
// (Initialize), the per-frame receive pump (DoWork), decoded-chunk hand-off
// (GetNextDataChunk) and frame release (ReleaseLastFrameData). The three
// send/QOS methods reach through the still-un-homed CStreamEngine and land with
// that TU (see XCamRemoteConsole.h). `XCAM` is an X360 SDK boundary, so its
// identifiers are preserved verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

// @ 0x82982AC8
int CRemoteConsole::Initialize(RemoteConsoleConfig* pConfig)
{
    // Empty (self-referential) intrusive list node.
    mLink.mpNext = &mLink;
    mLink.mpPrev = &mLink;

    mpConfig = pConfig;

    std::memset(maAddress, 0, sizeof(maAddress));
    maPrivileges[0] = 0;
    maPrivileges[1] = 0;
    maPrivileges[2] = 0;
    maPrivileges[3] = 0;
    miPrivilegeCount = 0;

    mbReceiving = 0;
    mbSending = 0;
    mbDisabled = false;
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

// @ 0x82982980
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

// @ 0x82982A50
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

// @ 0x82981B68
int CRemoteConsole::ReleaseLastFrameData()
{
    return mDepacketizer.ReleaseLastReadFrame();
}

} // namespace XCAM
