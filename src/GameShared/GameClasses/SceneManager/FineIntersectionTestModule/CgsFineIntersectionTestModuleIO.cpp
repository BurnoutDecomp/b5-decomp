#include "GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

// CgsSceneManager::FineIntersectionTestIO::OutputBuffer accessors, reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   GetEntityBuffer() @ 0x828B0A88 -- write-locked handle to the entity-index store.
//
// X360 body (0x828B0A88):
//   lbz   r11,0(r28)        ; status flags
//   extrwi r11,r11,1,28     ; (status >> 3) & 1   -> eStatusLockedForWrite
//   cmplwi cr6,r11,0; bne ...; on failure stream "Not locked for writing\n" and
//                              FireAssert(... CgsFineIntersectionTestModuleIO.h, 192)
//   addi  r3,r28,0x4020     ; return this + 0x4020 (&mEntityBuffer)
// The Begin/Clear/off_82000D00 churn is the StrStream that formats the assert message;
// the recovered intent is a single write-lock assertion (collapsed via CGS_ASSERT, matching
// the CgsGuiModuleIO::OutputBuffer accessors). The assert line 192 is the DWARF's line for
// `EntityBuffer* GetEntityBuffer()` -- which is how the previous "GetResults()" reading of
// this getter was corrected (scene-query wave 1, 2026-09-02).
//
// The three siblings (:188 / :189 / :191) have no out-of-line console emission; they are the
// same IOBuffer-getter macro with the READ bit (bit 4) for the const pair.

namespace CgsSceneManager
{
namespace FineIntersectionTestIO
{
    const OutputBuffer::LineTestIntersectionArray* OutputBuffer::GetLineTestIntersectionArray() const   // :188
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mLineTestIntersectionArray;
    }

    const OutputBuffer::EntityBuffer* OutputBuffer::GetEntityBuffer() const   // :189
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mEntityBuffer;
    }

    OutputBuffer::LineTestIntersectionArray* OutputBuffer::GetLineTestIntersectionArray()   // :191
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mLineTestIntersectionArray;
    }

    // GetEntityBuffer() @ 0x828B0A88
    OutputBuffer::EntityBuffer* OutputBuffer::GetEntityBuffer()   // :192
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mEntityBuffer;
    }
}
}
