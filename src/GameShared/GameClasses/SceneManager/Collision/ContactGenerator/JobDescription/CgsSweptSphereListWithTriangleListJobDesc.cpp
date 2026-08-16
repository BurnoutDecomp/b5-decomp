#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSweptSphereListWithTriangleListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsSceneManager::CgsCollision::SweptSphereListWithTriangleListJobDesc::Prepare @0x828103E0.
//
// ⚠️ THIS BODY IS A HOLE IN THE X360 EXPORT SET, NOT A MISSING FUNCTION. It was recovered by
// reading the image bytes directly (scratchpad x360rd + ppcdis) and is reproduced below from
// that disassembly, not from the sphere sibling by analogy — although the two turn out to be
// the same 37 instructions with one word changed:
//
//   0x828103EC  li    r11, 13          ; muJobType = 13   (the sphere sibling has `li r11, 5`)
//   0x828103F0  stfs  f1, 0xF4(r3)     ; mfRadius  = lfPadding
//   0x828103F4  li    r10, 0
//   0x828103F8  stw   r6, 0xF0(r3)     ; mpResultsList = lpResultsList
//   0x828103FC  addi  r9, r3, 8        ; &mTriangleList
//   0x82810400  stb   r11, 0xFF(r3)
//   0x82810404  stw   r10, 0xF8(r3)    ; mpDebugStream = 0
//   0x82810408..0x82810424              ; two dwords from r4 -> +0x00, two from r5 -> +0x08
//   0x82810428..0x8281043C  srawi/addze/slwi/subf. ; `*(u32*)this % 16` == 0 ?
//   0x82810458  FireAssert("Spheres not aligned to 16 bytes",
//                          "...ContactGenerator/JobDescription/CgsSweptSphereListWithTriangle...",
//                          84)
//   0x82810460  li    r3, 1            ; returns true
//
// Both strings were read out of .rdata (0x820DAA04 and 0x820DACA8) rather than inferred from
// the sibling, because "identical apart from the type byte" is exactly the kind of claim that
// deserves a direct check.

namespace CgsSceneManager
{
namespace CgsCollision
{
    bool SweptSphereListWithTriangleListJobDesc::Prepare(
        const SweptSphereList* lpSweptSphereList,
        const TriangleList*    lpTriangleList,
        CollisionResultList*   lpResultsList,
        f32                    lfPadding)
    {
        muJobType     = E_COLLISIONJOB_SWEPT_SPHERE_LIST_WITH_TRIANGLE_LIST;
        mfRadius      = lfPadding;
        mpResultsList = lpResultsList;
        mpDebugStream = 0;

        mSweptSphereList = *lpSweptSphereList;
        mTriangleList    = *lpTriangleList;

        // `subf. r11, r10, r11` on the FIRST word of the object, i.e. the swept-sphere base
        // pointer: it must be 16-byte aligned. Kept at the console's line number (:84).
        const bool lbAligned =
            (reinterpret_cast<uintptr_t>(mSweptSphereList.mpaSweptSpheres) & 0xF) == 0;
        CGS_ASSERT(lbAligned, "Spheres not aligned to 16 bytes");

        return true;
    }
}
}
