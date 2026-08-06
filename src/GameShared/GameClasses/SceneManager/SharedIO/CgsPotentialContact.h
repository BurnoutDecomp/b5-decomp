#pragma once

// ============================================================================
// GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h
//
// CgsSceneManager::SceneManagerIO::PotentialContact -- the scene-manager potential
// contact record: one candidate contact between two collision volume instances,
// produced by the contact-generation pass and consumed by the physics / deformation
// modules. Reconstructed from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/SceneManager/SharedIO/
//  CgsPotentialContact.h, struct @ :26; member sequence :60-68).
//
// FLAG: this homes the FULL CgsSceneManager::SceneManagerIO::PotentialContact in its
// canonical SceneManager/SharedIO home (it was only forward-declared previously, e.g.
// in BrnDeformationManager.h). The frozen DeformationManager methods
// (ReadPotentialContact / ReadPotentialVehicleWorldContact / SetupPartContact /
// FixUp/Fixup*VehicleContact) read its muVolumeInstanceIdA/B + muPolyTagA/B fields by
// name -- the X360 asm reads them at byte +48 / +56 / +64 / +68 of the record, which
// the DWARF member sequence reproduces exactly (3x 16-byte Vector3, then two 8-byte
// VolumeInstanceId, then the two 32-bit poly tags + the two 16-bit primitive indices).
//
// NOTE (distinct type): BrnPhysics::Deformation::PotentialContact (the minimal,
// reconstructed record in BrnPhysicalBodyPartPool.h) is a DIFFERENT type used by the
// hinged-body-part joint path; this is the scene-manager record the vehicle/world
// deformation contacts route through. They are deliberately NOT unified here.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                            // Vector3
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // CgsSceneManager::VolumeInstanceId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // DWARF CgsPotentialContact.h:26. Members BY NAME + SEQUENCE (each carries its DWARF line).
    struct alignas(16) PotentialContact
    {
        Vector3          mPointOnA;             // :60  contact point on volume A (byte +0)
        Vector3          mPointOnB;             // :61  contact point on volume B (byte +16)
        Vector3          mNormal;               // :62  contact normal (A->B) (byte +32)
        VolumeInstanceId muVolumeInstanceIdA;   // :63  packed id of volume A (byte +48)
        VolumeInstanceId muVolumeInstanceIdB;   // :64  packed id of volume B (byte +56)
        u32              muPolyTagA;            // :65  poly/sub-type tag on A (byte +64)
        u32              muPolyTagB;            // :66  poly/sub-type tag on B (byte +68)
        u16              mu16PrimitiveIndexA;   // :67  primitive index on A (byte +72)
        u16              mu16PrimitiveIndexB;   // :68  primitive index on B (byte +74)

        // ---- DWARF method surface (:30-57). DECLARE-ONLY -- bodies owned by the scene-manager
        //      TU -- except SwapEntityOrder, below. ----
        void Construct();                       // :30

        // :35. ⭐ DEFINED INLINE 2026-08-06 (bridge de-facade wave): the console emits NO
        // out-of-line body -- its one recovered caller, PhysicsModule::ProcessContactSpy
        // @0x825AB8A4..0x825AB954, inlines it in full: swap mPointOnA/mPointOnB (lvx/stvx on
        // the record's +0x00/+0x10), swap muVolumeInstanceIdA/B (ld/std on +0x30/+0x38), swap
        // muPolyTagA/B (lwz/stw on +0x40/+0x44), swap mu16PrimitiveIndexA/B (lhz/sth on
        // +0x48/+0x4A), and sign-flip mNormal (vspltisw -1 ; vslw -> 0x80000000 splat ; vxor).
        // Header-inline here, exactly as the console had it.
        void SwapEntityOrder()
        {
            const Vector3 lPointOnA = mPointOnA;
            mPointOnA = mPointOnB;
            mPointOnB = lPointOnA;

            const VolumeInstanceId lIdA = muVolumeInstanceIdA;
            muVolumeInstanceIdA = muVolumeInstanceIdB;
            muVolumeInstanceIdB = lIdA;

            const u32 luPolyTagA = muPolyTagA;
            muPolyTagA = muPolyTagB;
            muPolyTagB = luPolyTagA;

            const u16 lu16PrimitiveIndexA = mu16PrimitiveIndexA;
            mu16PrimitiveIndexA = mu16PrimitiveIndexB;
            mu16PrimitiveIndexB = lu16PrimitiveIndexA;

            // vxor with the 0x80000000 lane splat == per-lane sign flip (all four lanes).
            mNormal.x = -mNormal.x;
            mNormal.y = -mNormal.y;
            mNormal.z = -mNormal.z;
            mNormal.w = -mNormal.w;
        }
        u16  GetMaterialTagA() const;           // :39
        u16  GetGroupTagA() const;              // :43
        u16  GetMaterialTagB() const;           // :47
        u16  GetGroupTagB() const;              // :51
        u16  GetPrimitiveIndexA() const;        // :54
        u16  GetPrimitiveIndexB() const;        // :57
    };
}
}
