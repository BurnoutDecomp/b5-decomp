#include "GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

#include <cstddef>  // offsetof

// CgsSceneManager::FineIntersectionTestIO::OutputBuffer member function, reconstructed
// from BURNOUT_X360_ARTIST.XEX:
//   GetResults() @ 0x828B0A88 — write-locked handle to the fine-intersection result store.
//
// X360 body (0x828B0A88):
//   lbz   r11,0(r28)        ; status flags
//   extrwi r11,r11,1,28     ; (status >> 3) & 1   -> eStatusLockedForWrite
//   cmplwi cr6,r11,0; bne ...; on failure stream "Not locked for writing\n" and
//                              FireAssert(... CgsFineIntersectionTestModuleIO.h, 192)
//   addi  r3,r28,0x4020     ; return this + 0x4020 (&mResults)
// The Begin/Clear/off_82000D00 churn is the StrStream that formats the assert message;
// the recovered intent is a single write-lock assertion (collapsed via CGS_ASSERT, matching
// the CgsGuiModuleIO::OutputBuffer accessors).

namespace CgsSceneManager
{
namespace FineIntersectionTestIO
{
    // Byte-offset pin: the result store must sit at the asm-attested +0x4020.
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, maResults) == 0x4020,
                      "OutputBuffer::maResults must be at +0x4020 (GetResults return offset)");
    }

    // GetResults() @ 0x828B0A88
    u8* OutputBuffer::GetResults()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(&maResults);
    }
}
}
