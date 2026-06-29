#pragma once

// ===========================================================================
// CgsSceneManager::VolumeManager
//   Home: GameShared/GameClasses/SceneManager/CgsVolumeManager.{h,cpp}
//
// Owns the pool of collision Volumes referenced by the scene's volume instances.
// Embedded BY VALUE in CgsSceneManager::SceneManagerModule (mVolumeManager).
//
// Member set + Prepare() signature from the DecFIGS DWARF (CgsVolumeManager.h),
// gated on the X360 ARTIST ledger. The pooled volume storage is large and owned
// by VolumeManager's own TUs; this OWNING header models the public surface the
// SceneManagerModule needs and reserves the bulk as a documented opaque buffer
// so the type is a complete, embeddable value. (Byte offsets are not preserved
// on the x64 PC compile -- semantic-parity-by-name, per the project rule.)
//
// X360 function this TU (CgsSceneManagerModule.cpp) calls:
//   VolumeManager::Prepare  @ 0x828CFFA8
// ===========================================================================

#include "types.hpp"

namespace CgsSceneManager
{
    class VolumeManager
    {
    public:
        void Construct();

        // @ 0x828CFFA8 -- prepare the volume pool. Returns success.
        bool Prepare();

    private:
        // Opaque pooled volume storage; real internal layout owned by the
        // VolumeManager pool TUs.
        u8 maPooledStorage[915104];
    };
}
