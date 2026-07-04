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
    // CgsVolumeManager.h:60 (DecFIGS DWARF) -- one pooled collision-volume record.
    // Sibling of VolumeManager at namespace scope (NOT nested). X360-attested stride
    // 32 bytes: maObjectPool[5048] spans [0,161536) in the pool's AllocateObject/Clear
    // asm (maiObjectFreeQueue base @ +161536; 161536/5048 = 32). DWARF field NAMES
    // (offsets not individually attested -- the pool bodies never read element fields,
    // only sizeof(T)==32 matters; grow into named fields with real offsets when
    // VolumeManager's own add/replace TUs land, do NOT fork the pool):
    //   VolumeId                              mVolumeId;          // :62
    //   VolumeManagerVolume::VolumeTypeFlags  mxFlags;            // :63  (typedef uint8_t)
    //   ResourceHandle                        mVolumeHandle;      // :64
    //   int32_t                               miVolumeStoreIndex; // :65
    //   bool                                  mbStaticVolume;     // :66
    struct VolumeManagerVolume { u8 maBlob[32]; };

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
