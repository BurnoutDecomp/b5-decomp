#include "GameShared/Jobs/DXTDecode/DXTDecodeJob.h"

DXTDecodeJob gaDXTDecodeJobs[KI_NUM_DXT_DECODE_JOBS];

// Latch the data block into the job object, then tail-call the codec with the decoded buffer,
// the compressed buffer and the image size.
s32 DXTDecodeJob::Execute(const DXTDecodeData* lpData)
{
    mpData = lpData;
    return dxt_decode(lpData->mpDecodedPixels, lpData->mpCompressedPixels,
                      lpData->miDecodedWidth, lpData->miDecodedHeight);
}
