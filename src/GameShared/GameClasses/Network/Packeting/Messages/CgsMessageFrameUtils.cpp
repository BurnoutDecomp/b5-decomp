#include "CgsMessage.h"

// CgsNetwork frame-wrapping free functions (declared in CgsMessage.h in the
// leak; part of the class:CgsNetwork translation unit).
//
//   GetFrameDiffWrapped16        @ 0x82580760
//   UInt16IsLargerOrEqualWrapped @ 0x82540F28
//   UInt16IsLargerWrapped        @ 0x82540FB8
//
// 16-bit frame numbers wrap modulo 2^16; KU16_INVALID_FRAME (0xFFFF) is reserved
// and asserted against on entry. Store-for-store faithful to the X360 asm.

namespace CgsNetwork
{
    const u16 KU16_HALF_OF_FRAMES = 32768;

    // ---- GetFrameDiffWrapped16 @ 0x82580760 -----------------------------------
    // Smallest wrapped distance between two frames, treating them as a ring of
    // 65535 (the 0xFFFF sentinel is excluded). Returns min(|forward|-1, |back|)
    // exactly as the asm computes it.
    u16 GetFrameDiffWrapped16(u16 lu16FrameA, u16 lu16FrameB)
    {
        CGS_ASSERT(lu16FrameA != KU16_INVALID_FRAME, "lu16FrameA != KU16_INVALID_FRAME");
        CGS_ASSERT(lu16FrameB != KU16_INVALID_FRAME, "lu16FrameB != KU16_INVALID_FRAME");

        s16 li16Signed;
        u16 lu16Unsigned;
        if (lu16FrameA >= lu16FrameB)
        {
            li16Signed   = static_cast<s16>(lu16FrameB - lu16FrameA);
            lu16Unsigned = static_cast<u16>(lu16FrameA - lu16FrameB);
        }
        else
        {
            li16Signed   = static_cast<s16>(lu16FrameA - lu16FrameB);
            lu16Unsigned = static_cast<u16>(lu16FrameB - lu16FrameA);
        }

        u16 lu16Result = static_cast<u16>(li16Signed - 1);
        if (lu16Unsigned < lu16Result)
        {
            lu16Result = lu16Unsigned;
        }
        return lu16Result;
    }

    // ---- UInt16IsLargerOrEqualWrapped @ 0x82540F28 ----------------------------
    // True when lu16A is "ahead of or equal to" lu16B on the wrapped ring, i.e.
    // the unsigned forward distance (lu16A - lu16B) is in the near half.
    bool UInt16IsLargerOrEqualWrapped(u16 lu16A, u16 lu16B)
    {
        CGS_ASSERT(lu16A != KU16_INVALID_FRAME, "lu16A != KU16_INVALID_FRAME");
        CGS_ASSERT(lu16B != KU16_INVALID_FRAME, "lu16B != KU16_INVALID_FRAME");
        return static_cast<u16>(lu16A - lu16B) < KU16_HALF_OF_FRAMES;
    }

    // ---- UInt16IsLargerWrapped @ 0x82540FB8 -----------------------------------
    // Strict version: equal frames are not "larger".
    bool UInt16IsLargerWrapped(u16 lu16A, u16 lu16B)
    {
        CGS_ASSERT(lu16A != KU16_INVALID_FRAME, "lu16A != KU16_INVALID_FRAME");
        CGS_ASSERT(lu16B != KU16_INVALID_FRAME, "lu16B != KU16_INVALID_FRAME");
        if (lu16A == lu16B)
        {
            return false;
        }
        return UInt16IsLargerOrEqualWrapped(lu16A, lu16B);
    }
}
