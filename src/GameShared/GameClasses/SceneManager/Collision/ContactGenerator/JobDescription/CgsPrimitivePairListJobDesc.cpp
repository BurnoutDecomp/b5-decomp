#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsPrimitivePairListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
// CgsSceneManager::CgsCollision::PrimitivePairListJobDesc::Prepare @0x82810478.
//
// Behaviour-faithful to the X360 asm. `this` is r3. The descriptor is filled with the
// primitive-pair list and the bookkeeping fields, then the pair-list base pointer is
// asserted 16-byte aligned. Returns true (`li r3,1; blr`).

namespace CgsSceneManager
{
namespace CgsCollision
{
    bool PrimitivePairListJobDesc::Prepare(const PrimitivePairList* lpPairList,
                                           CollisionResultList*     lpResultsList)
    {
        // Bookkeeping (base) fields:
        //   stw  r5,0xF0(r3)   ; mpResultsList = lpResultsList
        //   li   r10,0xA; stb r10,0xFF(r3)  ; muJobType = 10
        //   lfs  f0,flt_82001CC0(=0.0f); stfs f0,0xF4(r3)  ; mfRadius = 0.0f
        //   li   r11,0; stw r11,0xF8(r3)    ; miStatus = 0
        mpResultsList = lpResultsList;
        muJobType     = E_COLLISIONJOB_PRIMITIVE_PAIR_LIST;
        mfRadius      = 0.0f;
        miStatus      = 0;

        // Copy the three dwords of the pair list into the leading payload (0,4,8):
        //   lwz r11,0(r4); stw r11,0(r3)
        //   lwz r10,4(r4); stw r10,4(r3)
        //   lwz r10,8(r4); stw r10,8(r3)
        mPrimitivePairList = *lpPairList;

        // `if ( *a1 % 16 )` — the pair-list base pointer must be 16-byte aligned.
        // (srawi/addze/slwi/subf. of mpaDataStream, branch on the remainder.)
        const bool lbAligned =
            (reinterpret_cast<uintptr_t>(mPrimitivePairList.mpaDataStream) & 0xF) == 0;
        CGS_ASSERT(lbAligned, "Primitive list not aligned to 16 bytes");

        return true;
    }
}
}
