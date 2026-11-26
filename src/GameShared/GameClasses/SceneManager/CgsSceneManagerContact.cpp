#include "GameShared/GameClasses/SceneManager/CgsSceneManagerContact.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//   CgsSceneManager::Contact::Construct @ 0x828A9E30
//
// Populates one Contact from contact-point luContactPointIndex of a source
// collision-pair result. Behaviour-faithful (semantic parity, not byte match).

// ARTIST's __FILE__ for these asserts (string @0x820F27F8): the original body is the
// header-inline Construct of CgsSceneManagerTypes.h, emitted out of line.
#define CGS_ARTIST_SCENE_MANAGER_TYPES_FILE \
    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\scenemanager\\CgsSceneManagerTypes.h"

namespace CgsSceneManager
{
    // -------------------------------------------------------------------------
    // Contact::Construct @ 0x828A9E30
    //
    // X360 store-for-store (r30 = this Contact, r31 = lpResult, r28 = index):
    //   assert lpResult != NULL                                  (cmplwi r31,0;  :157)
    //   assert index < lpResult->muNumPoints  (lwz r11,0x740(r31); cmplw;          :158)
    //   v13 = lpResult->mNormal               (lvx128 v13, r31, 0x4C0)
    //   v0  = 0x80000000 per lane             (vspltisw v0,-1; vslw v0,v0,v0)  ; sign mask
    //   v0  = v13 ^ v0  -> negate each lane    (vxor)            ; -mNormal
    //   this->mNormal   = v0                   (stvx128 v0, r30, 0x20)
    //   v0  = lpResult->maPositions[index]     (lvx128 v0, (index+80)*16, r31)
    //   this->mPosition = v0                   (stvx128 v0, r0,  r30)   ; offset 0
    //   v0  = lpResult->maImpulses[index]      (lvx128 v0, (index+96)*16, r31)
    //   this->mImpulse  = v0                   (stvx128 v0, r30, 0x10)
    //   this->muFieldA  = lpResult[+0x04]      (lwz r11,4(r31);  stw r11,0x38(r30))
    //   this->muFieldB  = lpResult[+0x0C]      (lwz r11,0xC(r31);stw r11,0x3C(r30))
    //
    // The vspltisw/vslw/vxor triple is the standard VMX idiom for a per-component
    // float negation (flip bit 31 of each of the four lanes) — reproduced with the
    // corresponding vector intrinsics below.
    // -------------------------------------------------------------------------
    void Contact::Construct(const SourceResult* lpResult, u32 luContactPointIndex)
    {
        CGS_ASSERT_AT(lpResult != 0, "lpResult != NULL",
                      CGS_ARTIST_SCENE_MANAGER_TYPES_FILE, 157);
        CGS_ASSERT_AT(luContactPointIndex < lpResult->muNumPoints,
                      "luContactPointIndex < lpResult->numPoints",
                      CGS_ARTIST_SCENE_MANAGER_TYPES_FILE, 158);

        // -mNormal : VMX sign-bit flip on all four lanes.
        const __vector4 lSignMask = __vslw(__vspltisw(-1), __vspltisw(-1));
        mNormal.mV = __vxor(lpResult->mNormal.mV, lSignMask);

        mPosition = lpResult->maPositions[luContactPointIndex];
        mImpulse  = lpResult->maImpulses[luContactPointIndex];

        muFieldA = lpResult->muFieldA;
        muFieldB = lpResult->muFieldB;
    }
}
