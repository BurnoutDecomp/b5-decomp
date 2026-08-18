// GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h
#pragma once

// BrnPhysics::Props::PropPartInstance -- a single physical prop-PART instance owned by
// the PropManager. Authored from the DecFIGS DWARF layout for this exact source path
// (references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/PropPhysics/
// BrnPropPartInstance.h), with the two member offsets touched by this batch pinned
// against the X360 ARTIST asm:
//   SetPosition        @ 0x825DE798  stores 16B @ +0x00 (mPos)
//   SetLinearVelocity  @ 0x825DE860  stores 16B @ +0x10 (mLinearVelocity)
//
// LAYOUT (DWARF member order + asm-attested offsets; stride 64, mEntityId @+0x30):
//   +0x00  Vector3      mPos             (16B)  :104
//   +0x10  Vector3      mLinearVelocity  (16B)  :105
//   +0x20  Vector3      mAngularVelocity (16B)  :106
//   +0x30  PropEntityID mEntityId        (4B)   :107
//   +0x34  uint32_t     muTypeId                :108
//   +0x38  uint8_t      mu8PartId               :109
//   +0x39  bool         mbUpdated               :110
//
// Only the two setters reconstructed in this batch are DEFINED (in the sibling .cpp);
// the remaining DWARF-declared trivial accessors are declared for shape.

#include "types.hpp"                                      // u8/u32 primitives
#include "BrnCommonTypes.h"                               // Vector3
#include "SharedClasses/Physics/Props/BrnPropEntityID.h" // BrnWorld::PropEntityID

namespace BrnPhysics
{
namespace Props
{
    class PropPartInstance
    {
    public:
        // ⚠️ Construct()/Prepare() are DECLARATION-ONLY: nothing in the prop-physics closure
        // calls them and no folded call site says what they do. Not invented.
        void            Construct();                       // :47
        bool            Prepare();                          // :51

        // ⭐ INLINED 2026-08-18 (breakable-props keystone), by the same measurement that
        // settles PropInstance::Release/Destruct: PropManager::Release @0x825BAC88 keeps
        // only `lbSuccess & 1` where the per-element Release() call was, and
        // PropManager::Destruct @0x825E3398 keeps no loop at all. Constant-true / empty.
        bool            Release()  { return true; }         // :55
        void            Destruct() {}                       // :59

        // ⭐ INLINED 2026-08-18: the DWARF's trivial field accessors, given bodies over the
        // offsets the banner above already pins (the X360 folds every one of them at its
        // call sites -- ReadUpdatedBodies @0x82632918 reads mEntityId at +0x30, compares
        // muTypeId, and stores mAngularVelocity with a bare `stvx128 v0, r31, 32`). Same
        // precedent as GetType below. Nothing here changes the layout.
        BrnWorld::PropEntityID GetEntityId()                { return mEntityId; }         // :62
        void            SetEntityId(BrnWorld::PropEntityID lId) { mEntityId = lId; }      // :65
        // :68. INLINE 2026-08-06 (bridge de-facade wave): no out-of-line emission exists;
        // PropManager::CreateContactEvent @0x825A53A0 inlines the +0x34 read directly.
        u32             GetType() { return muTypeId; }      // :68
        u8              GetPartId()                         { return mu8PartId; }         // :71
        void            SetType(u32 luTypeId)               { muTypeId = luTypeId; }      // :74
        void            SetPartId(u8 lu8PartId)             { mu8PartId = lu8PartId; }    // :77
        void            SetPosition(Vector3 lPosition);     // :81  X360 0x825DE798
        Vector3         GetPosition()                       { return mPos; }              // :84
        void            SetLinearVelocity(Vector3 lLinearVelocity); // :88  X360 0x825DE860
        Vector3         GetAngularVelocity()                { return mAngularVelocity; }  // :91
        // :95. ⚠️ NO out-of-line X360 emission was found for this one (its two siblings
        // SetPosition/SetLinearVelocity have real bodies at 0x825DE798/0x825DE860 with
        // IsValid asserts). ReadUpdatedBodies stores the angular velocity with a bare
        // `stvx128 v0, r31, 32`, i.e. an inlined plain store with NO assert -- reproduced
        // exactly, rather than copying the siblings' tripwire in.
        void            SetAngularVelocity(Vector3 lAngularVelocity)
        {
            mAngularVelocity = lAngularVelocity;
        }
        Vector3         GetLinearVelocity()                 { return mLinearVelocity; }   // :98
        bool            HasBeenUpdated()                    { return mbUpdated; }         // :101

    private:
        Vector3                mPos;             // +0x00 :104
        Vector3                mLinearVelocity;  // +0x10 :105
        Vector3                mAngularVelocity; // +0x20 :106
        BrnWorld::PropEntityID mEntityId;        // +0x30 :107
        u32                    muTypeId;         // +0x34 :108
        u8                     mu8PartId;        // +0x38 :109
        bool                   mbUpdated;        // +0x39 :110
    };
}
}
