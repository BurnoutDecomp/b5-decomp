#pragma once

// ===========================================================================
// CgsSceneManager::EntityManager
//   Home: GameShared/GameClasses/SceneManager/CgsEntityManager.{h,cpp}
//
// The SceneManager's entity / volume-instance registry. It owns the pooled
// SceneManagerEntity + VolumeInstance records and the two hash tables that map
// an EntityId / VolumeInstanceId to its pool index. Embedded BY VALUE in
// CgsSceneManager::SceneManagerModule (mEntityManager).
//
// Member set + method signatures recovered from the DecFIGS DWARF
// (CgsEntityManager.h) and gated on the X360 ARTIST ledger. The pooled storage
// is enormous (10000 entities + 5048 volume instances + two 10000-entry hash
// tables); its exact internal layout is owned by EntityManager's own TUs. This
// OWNING header models the public surface SceneManagerModule needs by name and
// reserves the bulk storage as a documented opaque buffer so the type is a
// complete, embeddable value type. (Byte offsets are NOT preserved on the x64
// PC compile -- pointers widen -- per the project's semantic-parity rule; the
// pool/hash TUs pin their own internal offsets.)
//
// X360 functions this TU (CgsSceneManagerModule.cpp) calls:
//   EntityManager::Prepare                 @ 0x828C5FC8
//   EntityManager::GetVolumeInstanceIndexByID @ 0x828CD4B8
// ===========================================================================

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // VolumeInstanceId

namespace CgsSceneManager
{
    class VolumeManager;
    class VolumeInstance;

    // Capacities the DWARF declares for the pools / index range.
    const s32 KI_MAX_NUM_ENTITIES         = 10000;
    const s32 KI_MAX_NUM_VOLUME_INSTANCES = 5048;

    class EntityManager
    {
    public:
        void Construct(VolumeManager* lpVolumeManager);

        // @ 0x828C5FC8 -- prepare the pools + hash tables. Returns success.
        bool Prepare();

        // @ 0x828CD4B8 -- map a VolumeInstanceId to its pool index (-1 if absent).
        s32  GetVolumeInstanceIndexByID(VolumeInstanceId lVolumeInstanceId) const;

        VolumeInstance*       GetVolumeInstance(s32 liIndex);
        const VolumeInstance* GetVolumeInstance(s32 liIndex) const;

    private:
        // Opaque pooled storage (entity pool + volume-instance pool + the two
        // IndexedHashTables). Its real internal layout is owned by EntityManager's
        // own TUs; modelled here as a single sized buffer so the manager is a
        // complete embeddable value with its DWARF tail member following it.
        u8 maPooledStorage[1201020];   // up to mpVolumeManager

        VolumeManager* mpVolumeManager;  // DWARF tail member
    };
}
