#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. CgsSceneManager::CgsCollision::TriangleList:
//   CheckAlignment    @ 0x825B3858 — assert the triangle base pointer is 16-byte aligned.
//   ValidateTriangles @ 0x825C0C28 — debug-validate every Triangle4 record in the list.
//
// Behaviour-faithful to the X360 pseudocode. Both are debug-only validation helpers
// (assert-on-failure, otherwise no observable effect). The asm `result` register is the
// `this` pointer (the leading int arg = `this`, IDA-style).

namespace CgsSceneManager
{
namespace CgsCollision
{
    // CheckAlignment @ 0x825B3858
    // X360: loads mpTriangles (offset 0), computes `ptr - ((ptr>>4)<<4)` (i.e. ptr % 16)
    // via srawi/addze/slwi/subf., and if the remainder is non-zero streams the assert
    // message "Triangles not aligned to 16 bytes: <ptr>\n" into the assert buffer and
    // fires it at CgsTriangleList.h:164. The original built the message with a StrStream
    // (the BasePriorityQueue::Clear / off_82000D00 vtable churn in the asm is the
    // StrStream construct/format machinery); the recovered intent is a single assertion
    // that the base pointer is 16-byte aligned, reconstructed via CGS_ASSERT with the
    // stringized condition.
    void TriangleList::CheckAlignment() const
    {
        // `*result % 16` in the asm: the low 4 bits of the triangle base pointer.
        const bool lbAligned = (reinterpret_cast<uintptr_t>(mpTriangles) & 0xF) == 0;
        CGS_ASSERT(lbAligned, "Triangles not aligned to 16 bytes");
    }

    // ValidateTriangles @ 0x825C0C28
    // X360: `if (*(this+4) > 0) { v3 = 0; do { AssertIsValid(v3 + *this); ++count;
    // v3 += 224; } while (count < this[1]); }`. Walks the contiguous Triangle4 array by
    // 224-byte stride and validates each record. The loop guard re-reads miNumTriangles
    // from memory every iteration (lwz 4(r29) inside the loop), matching `this[1]`.
    void TriangleList::ValidateTriangles() const
    {
        if (miNumTriangles > 0)
        {
            // ⭐ 2026-08-10 (fill-worker wave 2): this called the ODR-forked
            // NAMESPACE `Triangle4::AssertIsValid(void*)`, which was defined
            // `{ return 0; }` -- so this whole loop validated NOTHING. It now calls
            // the real member. The console's 224-byte cursor advance IS the element
            // stride (static_assert-gated in the header), so the walk is typed.
            s32 liCount = 0;
            do
            {
                mpTriangles[liCount].AssertIsValid();
                ++liCount;
            }
            while (liCount < miNumTriangles);
        }
    }
}
}
