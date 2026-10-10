#include "SDKs/XCam/XCamEncoder.h"
#include "SDKs/XCam/XCamVideo.h"   // KI_XCAM_ERROR_INSUFFICIENT_BUFFER

#include <cstring> // memcpy

// ===========================================================================
// XCAM::CEncoder::GetSequenceHeader -- partfile of XCamEncoder.cpp, reconstructed
// from the console executable (the PowerPC asm is authoritative). Called by
// CRemoteConsole::GetEncodedPacket to put the encoder's sequence header (the EMB
// extended-format blob captured at Initialize) into an outbound packet.
// ===========================================================================

namespace XCAM
{

// Copies the extended-format blob into pOut when the caller's buffer
// (*piInOutSize bytes, compared signed) can hold it; either way *piInOutSize
// receives the blob size. Returns 0, or 122 (insufficient buffer) without
// copying.
int CEncoder::GetSequenceHeader(u8* pOut, int* piInOutSize)
{
    const int iSize = static_cast<int>(muExtendedFormatSize);
    if (*piInOutSize < iSize)
    {
        *piInOutSize = iSize;
        return KI_XCAM_ERROR_INSUFFICIENT_BUFFER;
    }

    std::memcpy(pOut, mExtendedFormat, muExtendedFormatSize);
    *piInOutSize = static_cast<int>(muExtendedFormatSize);
    return 0;
}

} // namespace XCAM
