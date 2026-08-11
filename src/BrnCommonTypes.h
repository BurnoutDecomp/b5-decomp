#pragma once

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu vector/matrix types

// Cross-cutting engine math / identity primitives. The math types are RenderWare's
// rwmath VPU types; the game spells them unqualified, so alias them here exactly as
// the original source did (`typedef rw::math::vpu::Vector3 Vector3;` ...). Layout is
// console-faithful (16-byte SIMD registers; 64-byte matrices) — see rw/math/vpu/types.h.

typedef rw::math::vpu::Vector2        Vector2;
typedef rw::math::vpu::Vector3        Vector3;
typedef rw::math::vpu::Vector3Plus    Vector3Plus;   // Vector3 + a scalar packed in the w lane
typedef rw::math::vpu::Vector4        Vector4;
typedef rw::math::vpu::Matrix44       Matrix44;
typedef rw::math::vpu::Matrix44Affine Matrix44Affine;

// ADDITIVE GROW (flagged by BrnPhysics-bodies group): the two math aliases the physics
// bodies spell unqualified. Matrix33 is the 3x3 inverse-inertia tensor type; VecFloat is a
// single broadcast 16-byte float lane (the DWARF types many physics scalars as `VecFloat`).
// VecFloat == Vector4 here, matching the existing BrnPhysicalTrafficManager.h convention.
typedef rw::math::vpu::Matrix33       Matrix33;
typedef rw::math::vpu::Vector4        VecFloat;

// 32-bit packed identity handles (CgsSceneManager::EntityId and friends pack an
// owner/index into a single word; here we only need the storage word).
// ⭐ EntityId GENUINELY IS 32 BITS and must stay so -- see the width contrast in the banner below.
struct EntityId      { u32 muValue; };
struct CollisionTag  { u32 muValue; };

// ================================================================================================
// ⭐⭐ THE RIGID-BODY HANDLE IS EIGHT BYTES. The 4-byte `struct RigidBodyId { u32 muValue; }`
// stand-in that stood here until 2026-08-11 IS GONE; the name now aliases the real game type.
//
// WHY IT WAS WRONG, AND WHY THAT WAS INVISIBLE. On the console a 32-bit big-endian load of the
// FIRST word of the 64-bit handle yields the entity word, so a 4-byte seat "worked" everywhere the
// console read one. On x64 it does not: the low dword is the one at offset 0, and for a race car
// the low dword is IDENTICALLY ZERO (VehicleManager::ProcessCreateEvents builds the handle as
// `sldi r26,<entityWord>,32`). So routing an id through the stand-in delivered ZERO with nothing
// asserting -- a textbook silent drop, and it was live in two MOUNTED posts
// (BrnVehicleManager_MaintenanceEvents.cpp ProcessCollisionEvents, both arms).
//
// EIGHT INDEPENDENT WITNESSES, X360 asm unless stated. The load/store WIDTH is the evidence, and in
// every case there is a 32-bit op on a genuinely-32-bit field in the SAME body to contrast against:
//   1. VehicleManager::AddRaceCarDeformationModel @0x825E9118 indexes BOTH handle arrays with
//      `slwi r,idx,3` (0x825E919C / 0x825E91D8) -- STRIDE 8. A 4-byte element would be `slwi ,2`.
//   2. ...and loads its two handle arguments with `ldx` (0x825E932C mModelHandle,
//      0x825E9324 mHandlingBodyID), against `lwzx`/`lwz` at 0x825E91A0 / 0x825E921C / 0x825E9250.
//   3. VehicleManager::ProcessRemoveEvents @0x826160C8 fills the STACK EVENT RECORDS it passes to
//      AddEvent with `std`: 0x82616448 -> var_D0 (DeactivateDeformationModelEvent) and
//      0x826164B4 -> var_E8 (RemoveDeformationModelEvent). The field is 8 bytes IN THE EVENT.
//   4. Its deactivate event's two hoisted literals sit at var_C8 / var_C4, i.e. +8 and +12 of that
//      16-byte record -- so mfInitialDamageAmount is at +8 and the id owns +0..+7.
//   5. BaseEventQueue<AddDeformationModelEvent>::AddEvent's committed banner records the element
//      copy head as `stw +0, stw +4, std +8, stw +0x10` then SIMD from +0x20: that is
//      ResourceHandle(2 ptrs) / RigidBodyId(one doubleword) / EntityId(one word).
//   6. DeformationManager::ProcessAddDeformationModelEvents @0x82644828 -- the CONSUMER -- copies
//      the same head with `ld 0(evt)`, `ld 8(evt)`, **`lwz 0x10(evt)`**: eight, eight, FOUR, three
//      instructions apart. That single sequence proves the handle widens and EntityId must not.
//   7. DeformableObject::Prepare @0x82642180 stores it into the object with `std 0x6710(this)`
//      (0x826421B0) and the entity id beside it with `stw 0x6718(this)` (0x826421BC).
//   8. ProcessCollisionEvents builds the culling/collision posts as `extldi r11,r11,64,32`
//      == `(u64)entityWord << 32` -- an 8-byte value with the word in the HIGH half.
//
// ⭐ IT ALSO ANSWERS THE OBJECTION THAT KEPT THE FORK OPEN FOR SEVEN WAVES. The consumers do
// genuine 32-bit arithmetic (`>>24` owner, `(>>10)&0x3FFF` index, a straight 32-bit EntityId), which
// looked like it contradicted CgsRigidBody.h's packing. It does not: every one of them runs on the
// HIGH DWORD, extracted FIRST (`0x825E9218 srdi r11,r11,32` in the producer, `0x82644940` in the
// consumer). The arithmetic was always right; only the width was short. Spell those reads through
// GetEntityId() / GetEntityIDOwner() / GetIndex(), never `.muValue`.
// ================================================================================================
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"   // the REAL CgsPhysics::RigidBodyId (8B)
typedef CgsPhysics::RigidBodyId RigidBodyId;

// Generic 64-bit content/asset identity hash (burnout.wiki "Common Data Types").
typedef u64 CgsID;
