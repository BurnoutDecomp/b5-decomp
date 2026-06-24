#pragma once

// CgsSceneManager::OverlappingPair -- one entry in the scene manager's overlap list:
// the pair of collision-volume instances that the broad-phase overlap-generation pass
// found to be touching, queued for the narrow-phase overlap-culling pass to resolve.
//
// LAYOUT (X360-authoritative, from the BaseEventQueue<OverlappingPair>::AddEvent body
// @ 0x828B8B08): the queued element is exactly 16 bytes and is appended with four
// consecutive 32-bit stores --
//     stw r,0(dst); stw r,4(dst); stw r,8(dst); stw r,0xC(dst)
// (the element base is `16*index + mpEvents`, i.e. `slwi r11,r11,4` -> stride 0x10).
// Two VolumeInstanceId handles (each a packed 64-bit scene-volume-instance id) fill the
// 16 bytes; the compiler lowered the struct copy as the four word stores above. No
// DecFIGS DWARF covers this element, so the two-handle reconstruction is taken from the
// 16-byte stride + the overlap-list semantics (a pair of overlapping volume instances).
//
// 8-byte aligned (the two u64 handles); no SIMD lanes, so it does not need alignas(16).

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // VolumeInstanceId

namespace CgsSceneManager
{
    struct OverlappingPair
    {
        VolumeInstanceId mVolumeInstanceA;  // +0x00 (words +0x00 / +0x04)
        VolumeInstanceId mVolumeInstanceB;  // +0x08 (words +0x08 / +0x0C)
    };
}
