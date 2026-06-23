#ifndef SHAREDCLASSES_DATALISTS_VEHICLELISTENTRY_H
#define SHAREDCLASSES_DATALISTS_VEHICLELISTENTRY_H

#include "types.hpp"

// VehicleListEntry.h
// Single home of BrnResource::VehicleListEntry, the per-vehicle record inside a
// serialised VehicleListResource (sizeof == 0xF0 / 240, the stride
// GetSerialisedResourceDescriptor @0x8267B540 multiplies the vehicle count by).
//
// The on-disk layout (160-byte opaque header, then the embedded attrib/voice-over
// collision keys with their inter-key padding) is recovered from
// VehicleListResourceType::FixUp @0x8267DD60, which destructs each key in turn. This
// header is the one definition; VehicleListResourceType.cpp includes it rather than
// re-declaring the struct.
//
// ELiveryType (nested enum) is the livery-kind tag used by the per-car derived-livery
// lists -- BrnProgression::DerivedCarArray::ConstructColourLiveryList /
// ConstructPatternLiveryList build an Array<VehicleListEntry::ELiveryType, 8> of the
// livery versions a car supports (X360 Append @0x8235C6A8 stores a 4-byte enum value).
// The enum is 4 bytes (the Append stores with stwx / a word).

namespace CgsSceneManager { namespace CgsCollision
{
    // The embedded attrib/voice-over key handle (8-byte storage + Destruct). Declared here
    // so VehicleListEntry can hold its keys by name; the full collision-generator type lands
    // with its own TU (GROW this forward shape then, do not fork it).
    struct BaseCollisionGenerator
    {
        void Destruct();
        u8   maStorage[8];
    };
}}

namespace BrnResource
{

struct VehicleListEntry
{
    // Livery-kind tag for a derived car's livery list. The X360 stores it as a 4-byte word
    // (Array<ELiveryType,8>::Append @0x8235C6A8 uses stwx). The concrete enumerator names/values
    // are data-driven (not literal in the asm); modelled with the two kinds the derived-livery
    // builders distinguish (colour vs pattern) plus the count sentinel.
    enum ELiveryType
    {
        E_LIVERY_TYPE_COLOUR  = 0,
        E_LIVERY_TYPE_PATTERN = 1,
        E_LIVERY_TYPE_COUNT   = 2,
    };

    // Destruct the embedded collision keys (VehicleListResourceType::FixUp @0x8267DD60).
    void FixUp();

    // ---- on-disk layout (recovered from FixUp's key destructs); sizeof == 0xF0 (240) ----
    u8 maPad0[160];                                                       // +0x00
    CgsSceneManager::CgsCollision::BaseCollisionGenerator mAttribCollectionKey;        // +0xA0
    u8 maPad168[8];                                                       // +0xA8
    CgsSceneManager::CgsCollision::BaseCollisionGenerator mExhaustEntityKey;           // +0xB0
    CgsSceneManager::CgsCollision::BaseCollisionGenerator mEngineEntityKey;            // +0xB8
    u8 maPad192[16];                                                      // +0xC0
    CgsSceneManager::CgsCollision::BaseCollisionGenerator mWonCarVoiceOverKey;         // +0xD0
    CgsSceneManager::CgsCollision::BaseCollisionGenerator mRivalReleasedVoiceOverKey;  // +0xD8
    u8 maPad224[16];                                                      // +0xE0..0xEF
};

} // namespace BrnResource

#endif // SHAREDCLASSES_DATALISTS_VEHICLELISTENTRY_H
