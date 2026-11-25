#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSphereListWithSphereListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <new>
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\scenemanager\\collision\\ContactGenerator/JobDescription/CgsSphereListWithSphereListJobDesc.cpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
// CgsSceneManager::CgsCollision::SphereListWithSphereListJobDesc::Prepare @0x82810198.
//
// Behaviour-faithful to the X360 asm. `this` is r3 (moved to r31). Both sphere lists,
// the results list and the padding float are stored into the descriptor, then each
// sphere base pointer is asserted 16-byte aligned. Returns true (`li r3,1; blr`).

namespace CgsSceneManager
{
namespace CgsCollision
{
    bool SphereListWithSphereListJobDesc::Prepare(const SphereList*    lpSphereListA,
                                                  const SphereList*    lpSphereListB,
                                                  CollisionResultList* lpResultsList,
                                                  f32                  lfPadding)
    {
        new (maData) Data;

        // Bookkeeping (base) fields:
        //   stfs f1,0xF4(r31)  ; mfRadius   = lfPadding
        //   stw  r6,0xF0(r31)  ; mpResultsList = lpResultsList
        //   li   r11,7; stb r11,0xFF(r31) ; muJobType = 7
        //   li   r10,0; stw r10,0xF8(r31) ; mpDebugStream = 0 (DWARF mpDebugStream)
        mfRadius      = lfPadding;
        mpResultsList = lpResultsList;
        muJobType     = E_COLLISIONJOB_SPHERE_LIST_WITH_SPHERE_LIST;
        mpDebugStream = 0;   // (was misnamed miStatus; DWARF h:119)

        // Copy sphere list A (2 dwords @0,4) and B (2 dwords @8,0xC):
        //   lwz r11,0(r4); stw r11,0(r31)   ; lwz r11,4(r4); stw r11,4(r31)
        //   lwz r11,0(r5); stw r11,8(r31)   ; lwz r11,4(r5); stw r11,0xC(r31)
        GetData()->mSphereListA = *lpSphereListA;
        GetData()->mSphereListB = *lpSphereListB;

        // `if ( *a1 % 16 )` — sphere list A base pointer must be 16-byte aligned.
        const bool lbAlignedA =
            (reinterpret_cast<intptr_t>(GetData()->mSphereListA.mpSpheres) % 16) == 0;
        CGS_ASSERT_AT(lbAlignedA, "Spheres A not aligned to 16 bytes", CGS_ARTIST_FILE, 83);

        // `if ( *(a1 + 8) % 16 )` — sphere list B base pointer must be 16-byte aligned.
        const bool lbAlignedB =
            (reinterpret_cast<intptr_t>(GetData()->mSphereListB.mpSpheres) % 16) == 0;
        CGS_ASSERT_AT(lbAlignedB, "Spheres B not aligned to 16 bytes", CGS_ARTIST_FILE, 84);

        return true;
    }
}
}

#undef CGS_ARTIST_FILE
