#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                           // Matrix44Affine / Vector3
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // VolumeInstanceId

// CgsSceneManager::VolumeInstance - one live collision-volume placement: the
// world transform, its owning entity/volume indices and the cached-triangle
// window. Member names/order verbatim from the DecFIGS DWARF
// (CgsVolumeInstance.h:22); X360 sizeof 128 (the EntityManager's
// ObjectPool<VolumeInstance,5048> stride). The flag-bit accessors (h:72-83) are
// their own ledger functions (declaration-only; the mx8Flags bit assignments are
// not pinned here).
namespace CgsSceneManager
{
    struct VolumeInstance
    {
        Matrix44Affine   mWorldSpaceTransform;       // h:48 (+0x00)
        Vector3          mPadding;                   // h:49 (+0x40; DWARF's own name)
        VolumeInstanceId mUserID;                    // h:50 (+0x50)
        f32              mfCacheSphereRadius;        // h:51
        s32              miVolumeIndex;              // h:52
        s32              miNextEntityVolumeInstance; // h:53
        u16              mu16EntityIndex;            // h:54
        u16              mu16FirstCachedTriIndex;    // h:55
        u16              mu16NumCachedTris;          // h:56
        u8               mx8Flags;                   // h:57

        // DWARF h:72-83 -- declaration-only (their own ledger functions).
        bool IsCached() const;
        void SetCached(bool lbCached);
        bool IsCollidable() const;
        void SetCollidable(bool lbCollidable);
    };
}
