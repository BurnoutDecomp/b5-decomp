#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"          // CollisionPolicy (slice home)

#include "GameSource/Director/Camera/BrnCameraValidityAccount.h"          // ValidityAccount (the +0x138 account)

// BrnDirector::Camera::CollisionPolicy -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, class:BrnDirector::Camera::CollisionPolicy):
//   CollisionPolicy::Fail @0x82206450
//
// The shared-info reaches mirror Behaviour::Fail @0x822063E8 (see Behaviour.cpp):
// the validity account lives at info +0x138 (now the REAL committed
// ValidityAccount) and the u64 camera-request flag word at info +0x140 (still
// un-homed -- FLAG'd raw reach pending the shared-info TU).

namespace BrnDirector
{
namespace Camera
{

// @ 0x82206450
void CollisionPolicy::Fail(BehaviourSharedInfo* lpInfo, s32 liReason)
{
    // The account at info +0x138 (the Behaviour.cpp InfoValidityAccount reach,
    // now typed against the committed home).
    ValidityAccount* lpValidityAccount = reinterpret_cast<ValidityAccount*>(
        reinterpret_cast<u8*>(lpInfo) + 0x138);
    lpValidityAccount->SetFlag(liReason);

    // FLAG: the camera-request flag word at info +0x140 (bit 1 == the follow
    // request; the shared-info home has not landed). Same reach Behaviour::Fail
    // models -- `ld; and ~2; std`.
    u64* lpuRequestFlags = reinterpret_cast<u64*>(reinterpret_cast<u8*>(lpInfo) + 0x140);
    *lpuRequestFlags &= ~2ull;

    mbFailed = true;
}

}
}
