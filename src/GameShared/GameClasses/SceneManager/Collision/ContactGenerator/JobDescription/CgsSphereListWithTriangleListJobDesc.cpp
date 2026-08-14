#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSphereListWithTriangleListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
// CgsSceneManager::CgsCollision::SphereListWithTriangleListJobDesc::Prepare @0x82810100.
//
// Behaviour-faithful to the X360 asm. `this` is r3. The sphere list, triangle list,
// results list and padding float are stored into the descriptor, then the sphere base
// pointer is asserted 16-byte aligned. Returns true (`li r3,1; blr`).

namespace CgsSceneManager
{
namespace CgsCollision
{
    bool SphereListWithTriangleListJobDesc::Prepare(const SphereList*    lpSphereList,
                                                    const TriangleList*  lpTriangleList,
                                                    CollisionResultList* lpResultsList,
                                                    f32                  lfPadding)
    {
        // Bookkeeping (base) fields:
        //   li   r11,5; stb r11,0xFF(r3)  ; muJobType = 5
        //   stfs f1,0xF4(r3)              ; mfRadius  = lfPadding
        //   stw  r6,0xF0(r3)              ; mpResultsList = lpResultsList
        //   li   r10,0; stw r10,0xF8(r3)  ; mpDebugStream = 0 (DWARF mpDebugStream)
        muJobType     = E_COLLISIONJOB_SPHERE_LIST_WITH_TRIANGLE_LIST;
        mfRadius      = lfPadding;
        mpResultsList = lpResultsList;
        mpDebugStream = 0;   // (was misnamed miStatus; DWARF h:119)

        // Copy the sphere list (2 dwords @0,4) and the triangle list (2 dwords @8,0xC via
        // `addi r9,r3,8`):
        //   lwz r11,0(r4); stw r11,0(r3)   ; lwz r11,4(r4); stw r11,4(r3)
        //   lwz r11,0(r5); stw r11,0(r9)   ; lwz r11,4(r5); stw r11,4(r9)
        mSphereList   = *lpSphereList;
        mTriangleList = *lpTriangleList;

        // `if ( *a1 % 16 )` — sphere list base pointer must be 16-byte aligned.
        const bool lbAligned =
            (reinterpret_cast<uintptr_t>(mSphereList.mpSpheres) & 0xF) == 0;
        CGS_ASSERT(lbAligned, "Spheres not aligned to 16 bytes");

        return true;
    }
}
}
