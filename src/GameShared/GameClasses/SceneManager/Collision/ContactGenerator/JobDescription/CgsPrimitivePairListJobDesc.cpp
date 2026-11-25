#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsPrimitivePairListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <new>
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\scenemanager\\collision\\ContactGenerator/JobDescription/CgsPrimitivePairListJobDesc.cpp"

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
        new (maData) Data;

        // Bookkeeping (base) fields:
        //   stw  r5,0xF0(r3)   ; mpResultsList = lpResultsList
        //   li   r10,0xA; stb r10,0xFF(r3)  ; muJobType = 10
        //   lfs  f0,flt_82001CC0(=0.0f); stfs f0,0xF4(r3)  ; mfRadius = 0.0f
        //   li   r11,0; stw r11,0xF8(r3)    ; mpDebugStream = 0 (DWARF mpDebugStream)
        CollisionJobDescription::Prepare(E_COLLISIONJOB_PRIMITIVE_PAIR_LIST, lpResultsList,
                                         0.0f, 0);

        // Copy the three dwords of the pair list into the leading payload (0,4,8):
        //   lwz r11,0(r4); stw r11,0(r3)
        //   lwz r10,4(r4); stw r10,4(r3)
        //   lwz r10,8(r4); stw r10,8(r3)
        GetData()->mPrimitivePairList = *lpPairList;

        // `if ( *a1 % 16 )` — the pair-list base pointer must be 16-byte aligned.
        // (srawi/addze/slwi/subf. of mpaDataStream, branch on the remainder.)
        const bool lbAligned =
            (reinterpret_cast<intptr_t>(GetData()->mPrimitivePairList.mpaDataStream) % 16) == 0;
        CGS_ASSERT_AT(lbAligned, "Primitive list not aligned to 16 bytes", CGS_ARTIST_FILE, 81);

        return true;
    }
}
}

#undef CGS_ARTIST_FILE
