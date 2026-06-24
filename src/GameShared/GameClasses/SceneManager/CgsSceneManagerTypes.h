#pragma once

// CgsSceneManager::LineTestIntersection — one resolved hit produced by the
// FineIntersectionTestModule's line test. The OutputBuffer keeps these in an
// Array<LineTestIntersection, 256> (FineIntersectionTestModule::ComputeLineTestFine
// Appends into it). Field set + types recovered from the DecFIGS DWARF
// (CgsSceneManagerTypes.h:67-75).
//
// 64-byte record (16-aligned: carries two Vector3 lanes). The Array<T,256>::Append
// for this instantiation (0x828AE378) block-copies the whole 64-byte element (8 qwords,
// slwi index<<6 == stride 64), so the trailing scalar fields pad the struct to 0x40:
//   +0x00  mPosition          (Vector3, 16B)
//   +0x10  mNormal            (Vector3, 16B)
//   +0x20  mVolumeInstanceId  (VolumeInstanceId, u64)
//   +0x28  mEntityId          (EntityId, u32)
//   +0x2C  mfLineParam        (f32)
//   +0x30  mu16MaterialTag    (u16)
//   +0x32  mu16GroupTag       (u16)
//   +0x34..0x3F  padding (16-byte alignment from the embedded Vector3 lanes)

#include "types.hpp"
#include "BrnCommonTypes.h"  // Vector3 (16-byte SIMD lane)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // VolumeInstanceId
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"          // EntityId

namespace CgsSceneManager
{
    struct LineTestIntersection
    {
        Vector3          mPosition;          // +0x00
        Vector3          mNormal;            // +0x10
        VolumeInstanceId mVolumeInstanceId;  // +0x20
        EntityId         mEntityId;          // +0x28
        f32              mfLineParam;         // +0x2C
        u16              mu16MaterialTag;    // +0x30
        u16              mu16GroupTag;       // +0x32
    };
}
