#pragma once

// =====================================================================================================
// BrnPhysics::ContactGenList -- the fixed 128-slot contact-generation list the deformable-object physics
// fills with collision-pair entries (two volume-instance ids + their per-volume-instance offsets) during
// world-contact generation. Reconstructed from BURNOUT_X360_ARTIST.XEX; layout + method set + line
// numbers grounded by the DecFIGS DWARF (references/DecFIGS/dwarfdump/GameSource/Physics/
// BrnContactGenerationList.h) and the AddEntry asm @0x825B58F0.
//
// Base is CgsModule::IOBuffer (a 1-byte FlagSet8); the first ContactGenEntry lands at this+8 because the
// nested entry's leading VolumeInstanceId (a u64 member) forces 8-byte alignment. Entry stride is 24
// bytes (asm slwi-by-1/add/slwi-by-3 == n*24); miNumEntries sits at this+3080 (8 + 128*24).
//
// Producers: DeformableObject::DoBodyPartWorldContactGeneration / ::DoDetachedWheelWorldContactGeneration
// (one AddEntry per emitted primitive-vs-triangle contact).
// =====================================================================================================

#include "BrnCommonTypes.h"                                       // ::EntityId
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"           // CgsModule::IOBuffer (base)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h" // CgsSceneManager::VolumeInstanceId

namespace BrnPhysics
{
    struct ContactGenList : public CgsModule::IOBuffer
    {
        // DWARF miMaxEntries -- the fixed slot count.
        static const s32 KI_MAX_ENTRIES = 128;

        // BrnContactGenerationList.h:41-46 (DWARF). One collision-pair entry; 18 real bytes padded
        // to a locked 24-byte stride (mau8Pad[6]). The two VolumeInstanceIds are 8 bytes each (u64
        // muId, no vptr); the two per-volume-instance offsets are single bytes.
        struct ContactGenEntry
        {
            CgsSceneManager::VolumeInstanceId mIdA;               // +0x00 (8B)
            CgsSceneManager::VolumeInstanceId mIdB;               // +0x08 (8B)
            u8                                mIdAVolInstOffset;  // +0x10
            u8                                mIdBVolInstOffset;  // +0x11
            u8                                mau8Pad[6];         // pad to a 24-byte stride
        };

        void Construct();

        // @0x825B58F0 -- append one collision-pair entry to the list.
        void AddEntry(CgsSceneManager::VolumeInstanceId lIdA,
                      CgsSceneManager::VolumeInstanceId lIdB,
                      u8 lu8IdAVolInstOffset,
                      u8 lu8IdBVolInstOffset);

        // The 4-byte EntityId overload is a separate console symbol -- declared, not homed by this TU.
        void AddEntry(EntityId lIdA, EntityId lIdB, u8 lu8IdAVolInstOffset, u8 lu8IdBVolInstOffset);

        s32                    GetNumEntries() const;
        const ContactGenEntry& GetEntry(s32 liIndex) const;

    private:
        ContactGenEntry maEntries[KI_MAX_ENTRIES];   // +0x0008 (stride 24)
        s32             miNumEntries;                // +0x0C08 (3080)
    };
}
