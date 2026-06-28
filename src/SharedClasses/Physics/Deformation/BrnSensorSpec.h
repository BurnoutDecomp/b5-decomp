#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, VecFloat
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnSharedDeformationEnums.h"  // ENextSensorDirection

// BrnPhysics::Deformation::SensorSpec -- the streamed (resource-baked) parameters of ONE deformation
// sensor: its rest offset from the body origin, the per-direction compression limits, its collision
// radius, the six neighbour-sensor links, its scene/absorption indices and the two boundary-sensor
// links. Homed here at SharedClasses/Physics/Deformation/BrnSensorSpec.h alongside its sibling streamed
// specs (BrnTagPointSpec.h / BrnIKBodyPartSpec.h / ...).
//
// MEMBER LAYOUT is DWARF-authoritative (DWARF struct @ BrnSensorSpec.h:48; the DWARF dump itself is
// mirrored under .../DeformationPhysics/BrnSensorSpec.h, but the type belongs to the shared streamed-spec
// family, hence this SharedClasses home matching the siblings). Member sequence + names + types verbatim
// from the DWARF: mInitialOffset, maDirectionParams[6], mfRadius, maNextSensor[6], mu8SceneIndex,
// mu8AbsorbtionLevel, mau8NextBoundarySensor[2].
//
// CANONICAL-HOME / ODR NOTE: a copy of this struct previously lived inline inside
// BrnStreamedDeformationSpec.h (where StreamedDeformationSpec embeds maDeformationSensorSpecs[20] by
// value). That copy is REMOVED and that header now includes THIS one, so there is a single ODR-correct
// SensorSpec. The committed BrnStreamedDeformationSpec.cpp reads SensorSpec fields BY NAME
// (maDeformationSensorSpecs[i].mInitialOffset @ console +0, .mfRadius @ console +40); this layout keeps
// mInitialOffset at offset 0 and mfRadius immediately after the six PerDirectionParams, so those
// accesses stay correct (the +40 byte offset is the console 32-bit-pointer figure and need not
// reproduce on the 64-bit host -- access is by member name).
//
// sizeof == 64 (console): mInitialOffset(16) + maDirectionParams[6](24) + mfRadius(4) + maNextSensor[6](6)
// + mu8SceneIndex(1) + mu8AbsorbtionLevel(1) + mau8NextBoundarySensor[2](2), 16-byte aligned.
//
// FORWARD-DECLS: AddToScene takes an InSceneUpdateInterface* and GetVolumeID returns a VolumeId; both
// are referenced only by pointer/value in declared-only method signatures, so honest forward
// declarations suffice (their full homes are CgsSceneManager's IO / volume-id headers). DECLARE-ONLY
// methods have their bodies in the SensorSpec TU (a future .cpp); only the trivial read accessors the
// DWARF inlines are bodied here.

namespace CgsSceneManager
{
    struct VolumeId;                 // GetVolumeID return type (returned by value; home: CgsVolumeId.h)
    class  InSceneUpdateInterface;   // AddToScene param (home: CgsSceneManager scene-update IO)
}

namespace BrnPhysics
{
namespace Deformation
{
    // DWARF BrnSensorSpec.h:48. One streamed deformation-sensor spec.
    struct alignas(16) SensorSpec
    {
        // DWARF BrnSensorSpec.h:123 -- per-direction sensor parameters. The DWARF lists a single
        // member (mCompressionLimits); the six-element array maDirectionParams holds one per
        // ENextSensorDirection (the signed body axes).
        struct PerDirectionParams
        {
            f32 mCompressionLimits;   // DWARF BrnSensorSpec.h:125
        };

        // ---- read accessors the DWARF inlines (bodied) ----------------------------------------
        Vector3 GetInitialOffset()   const { return mInitialOffset; }   // DWARF :88
        f32     GetRadius()          const { return mfRadius; }         // DWARF :102
        u8      GetSceneIndex()      const { return mu8SceneIndex; }    // DWARF :84
        u8      GetAbsorptionLevel() const { return mu8AbsorbtionLevel; } // DWARF :111

        // ---- remaining authored API (DECLARED-ONLY; bodies in the SensorSpec TU) ----------------
        void    Construct();                                            // DWARF :51
        void    Destruct();                                             // DWARF :54
        void    Prepare(Vector3 lInitialOffset, u8 lu8SceneIndex);      // DWARF :59
        void    AddToScene(u8 lu8Index, CgsSceneManager::InSceneUpdateInterface* lpScene); // DWARF :65
        void    SetNextSensor(u8 lu8SensorIndex, ENextSensorDirection leDirection);        // DWARF :71
        u8      GetNextSensor(ENextSensorDirection leDirection) const;  // DWARF :75
        CgsSceneManager::VolumeId GetVolumeID(u8 lu8Index) const;       // DWARF :80
        void    SetInitialOffset(Vector3 lInitialOffset);              // DWARF :93
        VecFloat GetCompressionLimit(ENextSensorDirection leDirection) const; // DWARF :98
        void    FixUp(void* lpBaseAddress);                            // DWARF :105
        void    FixDown(void* lpBaseAddress);                          // DWARF :108
        u8      GetNeighbourIndex(s32 liIndex) const;                  // DWARF :115

        // Data members are public: SensorSpec is a streamed POD record and sibling code
        // (StreamedDeformationSpec::GetBoundingBox / TransformToNewCOMSpace) reads mInitialOffset /
        // mfRadius directly by name, as it does for the other streamed sub-specs.
    public:
        Vector3            mInitialOffset;             // DWARF :128  (console +0)
        PerDirectionParams maDirectionParams[6];       // DWARF :129  (console +16)
        f32                mfRadius;                    // DWARF :131  (console +40)
        u8                 maNextSensor[6];             // DWARF :133
        u8                 mu8SceneIndex;               // DWARF :134
        u8                 mu8AbsorbtionLevel;          // DWARF :135
        u8                 mau8NextBoundarySensor[2];   // DWARF :136
    };
}
}
