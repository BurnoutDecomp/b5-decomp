#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

// Layout self-check for the CgsModule::VariableEventQueue<N, 16> family (VEQ group).
// Confirms the X360-authoritative inline layout the generic header documents:
//   +0           bool mbIsConstructed        (BaseVariableEventQueue)
//   +1           char macData[BUFSIZE]       (byte-aligned -- NO alignas)
//   +BUFSIZE+4   s32  miBufferWritePos
//   +BUFSIZE+8   s32  miLength
//   +BUFSIZE+12  s32  miFirstEventOffset
// For every N in this group BUFSIZE % 4 == 0, so the 1-byte lead bool + macData ends at
// offset BUFSIZE+1, the s32 trio aligns at BUFSIZE+4, and the whole struct rounds to
// sizeof == BUFSIZE + 16. The per-record header CBufferEntry is exactly ALIGN (16) bytes.
namespace
{
    template <s32 BUFSIZE>
    struct VeqLayoutCheck
    {
        static_assert(sizeof(CgsModule::VariableEventQueue<BUFSIZE, 16>) == (size_t)(BUFSIZE + 16),
                      "VariableEventQueue<N,16> sizeof must be N + 16 (bool + char[N] + 3*s32, 4-aligned)");
    };

    // Force instantiation of a representative spread of this group's N values.
    template struct VeqLayoutCheck<2048>;
    template struct VeqLayoutCheck<4096>;
    template struct VeqLayoutCheck<8192>;
    template struct VeqLayoutCheck<14000>;
    template struct VeqLayoutCheck<16384>;
    template struct VeqLayoutCheck<32768>;
    template struct VeqLayoutCheck<65536>;
}
