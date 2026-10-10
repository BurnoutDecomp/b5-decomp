// =====================================================================================
// CWMVideoObjectEncoder -- the WMV video-object encoder (picture context) that owns a
// CWMVideoPerceptionModel and hands it to the model's constructor.
//
// Reconstructed from the console executable; the PowerPC asm is authoritative. No
// reference source and no debug-info declaration exists for this class.
//
// This is the minimal owning header: only the members the perception model reads are
// named; the rest of the (very large) encoder record is opaque and kept as padding so the
// named members sit at their console byte offsets. The encoder's own methods and
// construction are not homed yet; the encoder TU extends this header additively. The
// object is only ever reached through a pointer (CWMVideoPerceptionModel::mpVideoInfo),
// never constructed from this declaration.
//
// Console layout (byte offsets from this):
//   +0x2D0  muMBWidth     picture width in macro-blocks (map stride / column count)
//   +0x2D4  muMBHeight    picture height in macro-blocks (row count)
//   +0x588  miPQuant      picture quantiser; the dquant decision runs for 3..21 only
//   +0x6D54 miField6D54   with miField7B38, selects the two-row (field pair) MB analysis
//   +0x7B34 miField7B34   >= 4 widens the pixel shift of ShiftPixels from 2 to 4
//   +0x7B38 miField7B38   field-pair flag: doubles the MB rows of the IDquant map
// =====================================================================================
#pragma once

#include <cstddef>

#include "types.hpp"

class CWMVideoObjectEncoder
{
public:
    u8  mOpaque000[0x2D0];
    u32 muMBWidth;                      // +0x2D0
    u32 muMBHeight;                     // +0x2D4
    u8  mOpaque2D8[0x588 - 0x2D8];
    s32 miPQuant;                       // +0x588
    u8  mOpaque58C[0x6D54 - 0x58C];
    s32 miField6D54;                    // +0x6D54
    u8  mOpaque6D58[0x7B34 - 0x6D58];
    s32 miField7B34;                    // +0x7B34
    s32 miField7B38;                    // +0x7B38

    // Pin the named members to their console offsets.
    static void _AssertLayout()
    {
        static_assert(offsetof(CWMVideoObjectEncoder, muMBWidth) == 0x2D0, "muMBWidth @ +0x2D0");
        static_assert(offsetof(CWMVideoObjectEncoder, muMBHeight) == 0x2D4, "muMBHeight @ +0x2D4");
        static_assert(offsetof(CWMVideoObjectEncoder, miPQuant) == 0x588, "miPQuant @ +0x588");
        static_assert(offsetof(CWMVideoObjectEncoder, miField6D54) == 0x6D54, "miField6D54 @ +0x6D54");
        static_assert(offsetof(CWMVideoObjectEncoder, miField7B34) == 0x7B34, "miField7B34 @ +0x7B34");
        static_assert(offsetof(CWMVideoObjectEncoder, miField7B38) == 0x7B38, "miField7B38 @ +0x7B38");
    }
};
