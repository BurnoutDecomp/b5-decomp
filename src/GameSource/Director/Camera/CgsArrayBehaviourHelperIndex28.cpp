// Per-instantiation .cpp for Array<BrnDirector::Camera::BehaviourHelperIndex, 28>.
// The generic Array<T,N> body (Append / Contains / CountInstancesOf / Erase /
// EraseInstancesOf / FindFirstInstanceOf + siblings) is fully inline in CgsArray.h,
// so this TU is just the explicit class instantiation (the X360 emits one out-of-line
// copy per using-TU). The methods the X360 emitted for this instance:
//   Array<BehaviourHelperIndex,28>::Append           @ 0x821FCB20  (CgsArray.h:225/226)
//       (BehaviourManager::LockBehaviourForInterpolation / NewBehaviour<...> + 20 more)
//   Array<BehaviourHelperIndex,28>::Contains         @ 0x82211F88  (CgsArray.h:506)
//       (BehaviourManager::Lock/UnlockBehaviourForInterpolation)
//   Array<BehaviourHelperIndex,28>::CountInstancesOf @ 0x821FCC40  (CgsArray.h:453)
//       (BehaviourManager::UnlockBehaviourForInterpolation)
//   Array<BehaviourHelperIndex,28>::Erase            @ 0x821FFDE8  (CgsArray.h:380/381)
//       (Array<BehaviourHelperIndex,28>::EraseInstancesOf)
//   Array<BehaviourHelperIndex,28>::EraseInstancesOf @ 0x82211EE0  (CgsArray.h:426)
//       (BehaviourManager::ReleaseBehaviours / UnlockBehaviourForInterpolation)
//   Array<BehaviourHelperIndex,28>::FindFirstInstanceOf @ 0x821FFE98 (CgsArray.h:480)
//       (Array<BehaviourHelperIndex,28>::Contains)
//
// Layout: maElements[28] (28 * 4 = 112B; BehaviourHelperIndex is a single s32 word) +
// miCount @ +0x70, matching the X360 count word read at *(this+112) and the `4*index +
// base` (== &maElements[index]) element stride (slwi r11,r11,2 in Append/Erase).
//
// Spelled with full namespace qualification because BehaviourHelperIndex lives in
// BrnDirector::Camera (BrnBehaviourManager.h); the DWARF spells the type
// CgsContainers::Array<BrnDirector::Camera::BehaviourHelperIndex,28u>. CountInstancesOf
// was GROWN into the shared CgsArray.h generic body (grounded on the X360 0x821FCC40
// pseudocode) so every instantiation re-verifies it; it is not specialized here.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Director/Camera/BrnBehaviourManager.h"

template class Array<BrnDirector::Camera::BehaviourHelperIndex, 28>;
