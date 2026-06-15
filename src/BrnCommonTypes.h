#pragma once

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu vector/matrix types

// Cross-cutting engine math / identity primitives. The math types are RenderWare's
// rwmath VPU types; the game spells them unqualified, so alias them here exactly as
// the original source did (`typedef rw::math::vpu::Vector3 Vector3;` ...). Layout is
// console-faithful (16-byte SIMD registers; 64-byte matrices) — see rw/math/vpu/types.h.

typedef rw::math::vpu::Vector2        Vector2;
typedef rw::math::vpu::Vector3        Vector3;
typedef rw::math::vpu::Vector4        Vector4;
typedef rw::math::vpu::Matrix44       Matrix44;
typedef rw::math::vpu::Matrix44Affine Matrix44Affine;

// 32-bit packed identity handles (CgsSceneManager::EntityId and friends pack an
// owner/index into a single word; here we only need the storage word).
struct EntityId      { u32 muValue; };
struct RigidBodyId   { u32 muValue; };
struct CollisionTag  { u32 muValue; };

// Generic 64-bit content/asset identity hash (burnout.wiki "Common Data Types").
typedef u64 CgsID;
