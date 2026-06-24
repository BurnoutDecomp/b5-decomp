#include "GameShared/Jobs/DXTDecode/DXTDecodeJob.h"

#include <cstddef>  // offsetof

namespace
{
    static_assert(offsetof(DXTDecodeJob, mpDesc) == 0x00, "mpDesc @ +0x00");
    static_assert(offsetof(DXTDecodeJobDesc, mu32Field00) == 0x00, "desc +0x00");
    static_assert(offsetof(DXTDecodeJobDesc, mu32Field04) == 0x04, "desc +0x04");
    static_assert(offsetof(DXTDecodeJobDesc, mu32Field10) == 0x10, "desc +0x10");
    static_assert(offsetof(DXTDecodeJobDesc, mu32Field14) == 0x14, "desc +0x14");
}

// DXTDecodeJob::Execute @ 0x82C07590
//
//   mr  r11, r4            ; r11 = apDesc
//   stw r11, 0(r3)         ; *this = apDesc      (latch the job-desc pointer)
//   lwz r6, 0x14(r11)      ; arg4 = apDesc[+0x14]
//   lwz r5, 0x10(r11)      ; arg3 = apDesc[+0x10]
//   lwz r4, 0(r11)         ; arg2 = apDesc[+0x00]
//   lwz r3, 4(r11)         ; arg1 = apDesc[+0x04]
//   b   dxt_decode         ; tail-call the codec; its result is Execute's result
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity). The X360 lowers Execute as a
// store of the desc pointer into the job object, then a tail-call into the dxt_decode codec
// forwarding four desc fields (arg order from the r3..r6 setup). The codec itself lives in
// its own TU; this dispatch shim only stores and forwards.
int DXTDecodeJob::Execute(const DXTDecodeJobDesc* apDesc)
{
    mpDesc = apDesc;
    return dxt_decode(apDesc->mu32Field04, apDesc->mu32Field00,
                      apDesc->mu32Field10, apDesc->mu32Field14);
}
