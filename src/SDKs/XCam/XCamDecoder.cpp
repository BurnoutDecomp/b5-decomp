#include "SDKs/XCam/XCamDecoder.h"

#include <intrin.h>   // _InterlockedIncrement (X360 interrupt-safe interlocked inc)

// ===========================================================================
// XCAM::CDecoder -- reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
// See XCamDecoder.h for the member layout and the per-method X360 addresses.
// `XCAM`/`WMV` are X360 SDK boundaries, so their identifiers are preserved
// verbatim per the naming convention.
// ===========================================================================

namespace XCAM
{

// @ 0x82988078
int CDecoder::InitializeCodec()
{
    // width = high 16 bits, height = low 16 bits (lhz this+0x12C / this+0x12E).
    const s32 iWidth  = static_cast<s32>(muDimensions >> 16);
    const s32 iHeight = static_cast<s32>(muDimensions & 0xFFFF);

    // Both rate params are converted through single precision (fcfid + frsp).
    const double dFramerate = static_cast<double>(static_cast<float>(miFramerate));
    const double dBitrate   = static_cast<double>(static_cast<float>(miBitrate));

    if (WMVideoDecInit(&mpDecoder, &mDataCallback, KU_XCAM_DEC_CODEC_WMVR,
                       dFramerate, dBitrate, iWidth, iHeight, KI_XCAM_DEC_DECODE_MODE))
    {
        return KI_XCAM_DECODER_INIT_FAILED;
    }

    mbSequenceHeaderDecoded = false;
    return 0;
}

// @ 0x82988118
int CDecoder::DecodeFrame(int bSkipOutput, int iErrorRecoveryFrameType)
{
    WMVideoDecSetErrorRecoveryFrameType(mpDecoder, iErrorRecoveryFrameType);

    u8 auDecodeData[8];
    const int iResult = WMVideoDecDecodeData(mpDecoder, auDecodeData, KI_XCAM_DEC_DECODE_MODE);

    // Ping the data-source/notifier object through its second vtable slot. It is a
    // COM-style foreign interface (same object WMVDecCBGetData_HANDLER receives as
    // its "this"); only the one slot the decoder touches is expressed here.
    void** ppVtable = *static_cast<void***>(mpDataSource);
    reinterpret_cast<void (*)(void*)>(ppVtable[1])(mpDataSource);

    // Atomically bump the decoded-frame counter (X360 interrupt-safe interlocked
    // increment: mfmsr/mtmsree around lwarx/stwcx on this+0).
    _InterlockedIncrement(reinterpret_cast<volatile long*>(&muDecodeCount));

    switch (iResult)
    {
    case KI_XCAM_DEC_ASYNC_PENDING:   // -100
    case KI_XCAM_DEC_NEED_MORE_DATA:  // 1
    case 4:
    case 11:
        // No frame produced this call; nothing to do.
        break;

    case KI_XCAM_DEC_FRAME_READY:     // 0
        if (!bSkipOutput)
        {
            SVideoBuffer bufY, bufU, bufV;
            mDecoderOutput.GetBuffersForWriting(&bufY, &bufU, &bufV);
            if (!WMVideoDecPlanarYUVGetOutput(mpDecoder, KI_XCAM_DEC_OUTPUT_MODE,
                                              bufY.mpData, bufU.mpData, bufV.mpData,
                                              bufY.muPitch, bufU.muPitch, bufV.muPitch))
            {
                mDecoderOutput.BufferWriteSucceeded();
            }
        }
        break;

    case KI_XCAM_DEC_RESET_REQUIRED:  // 3
        WMVideoDecReset(mpDecoder);
        break;

    default:                          // hard error -> tear down and re-initialise
        WMVideoDecClose(mpDecoder);
        InitializeCodec();
        WMVideoDecReset(mpDecoder);
        mbSequenceHeaderDecoded = false;
        break;
    }

    return iResult;
}

// @ 0x82988248
int CDecoder::Initialize(u32 uDimensions, s32 iFramerate, s32 iBitrate,
                         void* pDataSource, D3DDevice* pDevice)
{
    muDimensions = uDimensions;
    miFramerate  = iFramerate;
    miBitrate    = iBitrate;
    mpDataSource = pDataSource;

    const int iResult = mDecoderOutput.Initialize(pDevice,
                                                  static_cast<s32>(uDimensions >> 16),
                                                  static_cast<s32>(uDimensions & 0xFFFF));
    if (iResult != 0)
        return iResult;

    mDataCallback.mpContext   = pDataSource;
    muDecodeCount             = 0;
    mDataCallback.mpfnGetData = &WMVDecCBGetData_HANDLER;
    return InitializeCodec();
}

// @ 0x82988038
int CDecoder::ProcessSequenceHeader()
{
    const int iResult = WMVideoDecDecodeSequenceHeader(&mpDecoder);
    mbSequenceHeaderDecoded = (iResult == 0);
    return iResult;
}

// @ 0x82987FE0
int CDecoder::Reset(int bPreserveOutput)
{
    const int iResult = WMVideoDecReset(mpDecoder);
    mbSequenceHeaderDecoded = false;

    // The X360 tail-returns CDecoderOutput::Reset() here; that method is void, so
    // its result is not a meaningful value -- return the codec-reset result.
    if (!bPreserveOutput)
        mDecoderOutput.Reset();

    return iResult;
}

} // namespace XCAM
