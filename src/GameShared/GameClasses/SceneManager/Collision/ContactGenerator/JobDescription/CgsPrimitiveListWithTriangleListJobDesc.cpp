#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsPrimitiveListWithTriangleListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <new>
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\scenemanager\\collision\\ContactGenerator/JobDescription/CgsPrimitiveListWithTriangleListJobDesc.cpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
// CgsSceneManager::CgsCollision::PrimitiveListWithTriangleListJobDesc::Prepare
// @0x82810278.
//
// Behaviour-faithful to the X360 asm. `this` is r3. The pair list, triangle list,
// results list and the use-optimized-box-tests flag are stored into the descriptor;
// then two debug asserts run: the pair-list base pointer must be 16-byte aligned, and
// the pair-list size (rounded up to a multiple of 16) must be a multiple of 16.
// Returns true (`li r3,1; b __restgprlr_26`).

namespace CgsSceneManager
{
namespace CgsCollision
{
    bool PrimitiveListWithTriangleListJobDesc::Prepare(const PrimitivePairList* lpPairList,
                                                       const TriangleList*      lpTriangleList,
                                                       CollisionResultList*     lpResultsList,
                                                       bool                     lbUseOptimizedBoxTests)
    {
        new (maData) Data;

        // Bookkeeping (base) fields:
        //   stw  r6,0xF0(r3)              ; mpResultsList = lpResultsList
        //   li   r10,0xB; stb r10,0xFF(r3); muJobType = 11
        //   lfs  f0,flt_82001CC0(=0.0f); stfs f0,0xF4(r3) ; mfRadius = 0.0f
        //   li   r26,0; stw r26,0xF8(r3)  ; mpDebugStream = 0 (DWARF mpDebugStream)
        CollisionJobDescription::Prepare(E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST, lpResultsList,
                                         0.0f, 0);

        // Copy the pair list (3 dwords @0,4,8) and the triangle list (2 dwords @0xC,0x10
        // via `addi r11,r3,0xC`), then the flag (stb r7,0x14):
        //   lwz r10,0(r31); stw r10,0(r3)   ; ... 4,8
        //   lwz r10,0(r5);  stw r10,0(r11)  ; lwz r10,4(r5); stw r10,4(r11)
        //   stb r7,0x14(r3)
        GetData()->mPrimitivePairList     = *lpPairList;
        GetData()->mTriangleList          = *lpTriangleList;
        GetData()->mbUseOptimizedBoxTests = lbUseOptimizedBoxTests;

        // `if ( v6 % 16 )` where v6 = pair-list base pointer (lwz 0(r3)) — assert it is
        // 16-byte aligned. The asm string is "Data stream not aligned to 16 bytes".
        const bool lbStreamAligned =
            (reinterpret_cast<intptr_t>(GetData()->mPrimitivePairList.mpaDataStream) % 16) == 0;
        CGS_ASSERT_AT(lbStreamAligned, "Data stream not aligned to 16 bytes", CGS_ARTIST_FILE, 85);

        // `v7 = (size + 15) & 0xFFF0; if ( v7 != 16 * (v7 >> 4) )` — round the pair-list
        // byte size (lhz 4(r31)) up to a multiple of 16 and assert that result is itself
        // a multiple of 16. (Tautological by construction; the X360 streamed the rounded
        // size into the message — "Primitive list size is not a multiple of 16 (actual
        // size is <n>)" — collapsed here to the static message.)
        CGS_ASSERT_STREAM_AT(static_cast<s32>(lpPairList->GetDataSize()) % 16 == 0,
            "Primitive list size is not a multiple of 16 (actual size is "
            << static_cast<s32>(lpPairList->GetDataSize()) << ")", CGS_ARTIST_FILE, 86);

        return true;
    }
}
}

#undef CGS_ARTIST_FILE
