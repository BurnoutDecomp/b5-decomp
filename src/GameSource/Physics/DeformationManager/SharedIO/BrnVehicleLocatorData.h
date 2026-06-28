#pragma once

// BrnPhysics::Deformation::VehicleLocatorData -- the per-vehicle camera/light/generic
// LOCATOR table the deformation system maintains and hands to the renderer / entity
// modules. A "locator" is a named attachment frame on the deformed car body (camera
// mounts, head/tail/police lights, generic effect anchors); each is a world transform
// plus its semantic tag-point type, with a live count per category.
//
// HOMING NOTE (Task 2): the DWARF only ever FORWARD-declares VehicleLocatorData where it
// is embedded BY VALUE in DeformableObject (mLocatorData). Its REAL full member layout,
// however, IS recovered -- it is spelled out at the deformation output-interface DWARF home
// (references/DecFIGS/dwarfdump/.../SharedIO/BrnDeformationOutputInterface.h:101). It is
// homed HERE in its own header (rather than inside BrnDeformationOutputInterface.h) so that
// BrnDeformableObject.h can include just the locator type by value without pulling the whole
// output-interface (event-queue) header. VehicleLocatorOutput (the {EntityId, const
// VehicleLocatorData*} pair) keeps its home in BrnDeformationOutputInterface.h and references
// this type by pointer. NOT an opaque placeholder -- every member below is DWARF-sourced.
//
// LAYOUT (DWARF BrnDeformationOutputInterface.h:103-113). Matrix44Affine is a 64-byte affine
// frame; the per-category arrays are fixed-size with a trailing live count:
//   maCameraLocators[1]      / maCameraLocatorTypes[1]   / miNumCameraLocators
//   maLightLocators[24]      / maLightLocatorTypes[24]   / miNumLightLocators
//   maGenericLocators[15]    / maGenericLocatorTypes[15] / miNumGenericLocators
// Console pointer offsets are 32-bit and do not all hold on the 64-bit host; members are
// pinned BY NAME + SEQUENCE (the embedding DeformableObject indexes mLocatorData only by
// member, never raw offset). alignas(16): carries Matrix44Affine frames (SIMD rows).

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"  // ETagPointType

namespace BrnPhysics
{
namespace Deformation
{
    // The camera locator category has exactly one slot, light has 24, generic has 15. These
    // bounds are DWARF-authoritative (the array extents below); named here for the consuming
    // accessors (e.g. ActiveRaceCar::SetLightLocators iterates the 24 light slots).
    static const s32 KI_NUM_CAMERA_LOCATORS  = 1;    // DWARF maCameraLocators[1]
    static const s32 KI_NUM_LIGHT_LOCATORS   = 24;   // DWARF maLightLocators[24]
    static const s32 KI_NUM_GENERIC_LOCATORS = 15;   // DWARF maGenericLocators[15]

    struct alignas(16) VehicleLocatorData
    {
        // ---- camera locators (DWARF :103-105) ----------------------------------------------
        Matrix44Affine maCameraLocators[KI_NUM_CAMERA_LOCATORS];        // :103 world frames
        ETagPointType  maCameraLocatorTypes[KI_NUM_CAMERA_LOCATORS];    // :104 per-slot tag type
        s32            miNumCameraLocators;                             // :105 live camera count

        // ---- light locators (DWARF :107-109) -----------------------------------------------
        Matrix44Affine maLightLocators[KI_NUM_LIGHT_LOCATORS];          // :107 world frames
        ETagPointType  maLightLocatorTypes[KI_NUM_LIGHT_LOCATORS];      // :108 per-slot tag type
        s32            miNumLightLocators;                             // :109 live light count

        // ---- generic locators (DWARF :111-113) ---------------------------------------------
        Matrix44Affine maGenericLocators[KI_NUM_GENERIC_LOCATORS];      // :111 world frames
        ETagPointType  maGenericLocatorTypes[KI_NUM_GENERIC_LOCATORS];  // :112 per-slot tag type
        s32            miNumGenericLocators;                           // :113 live generic count
    };
}
}
