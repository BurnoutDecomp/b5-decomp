#pragma once

#include "types.hpp"

// Cross-cutting engine math / identity primitives, recovered from the DecFIGS
// DWARF (member names/types) and the X360 spine (sizes/alignment). Vector3 and
// the affine matrix are SIMD types and are 16-byte aligned, which is what fixes
// the inline-buffer offset of the event queues that embed them.

struct alignas(16) Vector3
{
    f32 x;
    f32 y;
    f32 z;
};

struct alignas(16) Matrix44Affine
{
    Vector3 mRight;
    Vector3 mUp;
    Vector3 mAt;
    Vector3 mPos;
};

// 32-bit packed identity handles (CgsSceneManager::EntityId and friends pack an
// owner/index into a single word; here we only need the storage word).
struct EntityId      { u32 muValue; };
struct RigidBodyId   { u32 muValue; };
struct CollisionTag  { u32 muValue; };
