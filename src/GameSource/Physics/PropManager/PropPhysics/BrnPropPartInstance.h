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
        void            Construct();                       // :47
        bool            Prepare();                          // :51
        bool            Release();                          // :55
        void            Destruct();                         // :59
        BrnWorld::PropEntityID GetEntityId();               // :62
        void            SetEntityId(BrnWorld::PropEntityID); // :65
        // :68. INLINE 2026-08-06 (bridge de-facade wave): no out-of-line emission exists;
        // PropManager::CreateContactEvent @0x825A53A0 inlines the +0x34 read directly.
        u32             GetType() { return muTypeId; }      // :68
        u8              GetPartId();                        // :71
        void            SetType(u32);                       // :74
        void            SetPartId(u8);                      // :77
        void            SetPosition(Vector3 lPosition);     // :81  X360 0x825DE798
        Vector3         GetPosition();                      // :84
        void            SetLinearVelocity(Vector3 lLinearVelocity); // :88  X360 0x825DE860
        Vector3         GetAngularVelocity();               // :91
        void            SetAngularVelocity(Vector3);        // :95
        Vector3         GetLinearVelocity();                // :98
        bool            HasBeenUpdated();                   // :101

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
