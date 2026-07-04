#pragma once

// ===========================================================================
// CgsSceneManager::VolumeStore<N> + its element type VolumeSlot
//   Home: GameShared/GameClasses/SceneManager/CgsVolumeStore.h
//
// Recovered from the DecFIGS DWARF (CgsVolumeStore.h). The scene manager stores
// each collision Volume in a fixed 128-byte VolumeSlot (KI_VOLUME_SLOT_SIZE) and
// pools 4608 of them via ObjectPool<VolumeSlot,4608,int>. Only the VolumeSlot
// element type + the VolumeStore<N> shell are modelled here (the surface the pool
// instantiation TU needs); VolumeStore's own method bodies (Construct/Prepare/
// AddVolume/RemoveVolume/...) are their own ledger functions.
//
// DWARF authority:
//   KI_VOLUME_SLOT_SIZE       = 128        (CgsVolumeStore.h:33)
//   KI_INVALID_VOLUME_INDEX   = 4294967295 (CgsVolumeStore.h:34, int32_t = 0xFFFFFFFF)
//   struct VolumeSlot { private: uint8_t macBuffer[128]; ... } (CgsVolumeStore.h:51/64)
//   VolumeStore<4608>::mVolumePool : ObjectPool<VolumeSlot,4608,int32_t> (h:126)
// ===========================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"   // CgsContainers::ObjectPool<T,N,TIndex>

namespace CgsSceneManager
{
    // Per-volume reference produced by the broad phase (opaque; the real layout is
    // owned by the rwcollision SDK).
    namespace VolRef { struct Volume; }

    // DWARF CgsVolumeStore.h:33/34. KI_INVALID_VOLUME_INDEX is an int32_t whose bit
    // pattern is 0xFFFFFFFF (== -1); kept as the DWARF name and type.
    static const s32 KI_VOLUME_SLOT_SIZE     = 128;
    static const s32 KI_INVALID_VOLUME_INDEX = static_cast<s32>(0xFFFFFFFF);

    // One pooled collision-volume slot: an opaque 128-byte buffer that holds a
    // serialised VolRef::Volume in place (DWARF CgsVolumeStore.h:51/64). The buffer
    // sizes the ObjectPool stride (589824 / 4608 == 128 on the X360). The
    // SetVolume/GetVolume accessors are their own ledger functions (declaration-only;
    // the buffer's internal Volume layout is not pinned here).
    struct VolumeSlot
    {
        // CgsVolumeStore.h:56 -- copy a Volume of the given size into macBuffer.
        void SetVolume(const VolRef::Volume* lpVolume, s32 liSizeInBytes);
        // CgsVolumeStore.h:59 -- view macBuffer as a Volume.
        VolRef::Volume* GetVolume() const;

    private:
        u8 macBuffer[KI_VOLUME_SLOT_SIZE];   // CgsVolumeStore.h:64 (+0x00)
    };

    // VolumeStore<N> -- owns the pool of N collision-volume slots. Only the pool
    // member is modelled (the instantiation TU needs its layout); the store's own
    // method bodies are their own ledger functions.
    template <s32 tiCapacity>
    class VolumeStore
    {
    public:
        void Construct();
        void Destruct();
        bool Prepare();
        bool Release();
        void Clear();
        s32  AddVolume(const VolRef::Volume* lpVolume);
        void RemoveVolume(s32 liIndex);
        bool ReplaceVolume(s32 liIndex, const VolRef::Volume* lpVolume);
        const VolRef::Volume* GetVolume(s32 liIndex) const;

    private:
        CgsContainers::ObjectPool<VolumeSlot, tiCapacity, s32> mVolumePool;   // CgsVolumeStore.h:126
    };
}
