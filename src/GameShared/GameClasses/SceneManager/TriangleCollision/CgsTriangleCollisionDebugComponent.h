#pragma once

#include "types.hpp"

// CgsTriangleCollisionDebugComponent.h — debug-UI identity hook for the triangle-
// collision debug component (the in-game overlay that draws poly soups, soup AABBs,
// poly normals and runs the interactive sphere/line/box/swept-sphere collision tests).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsSceneManager::TriangleCollisionDebugComponent::GetName @ 0x827DD828  -> "Collision"
//
// HEADER-KEYED MINIMAL SLICE: the X360 ledger attests exactly one function for this
// header TU — the GetName override, a leaf that returns a literal and touches no
// instance state. The full TriangleCollisionDebugComponent is a heavy CgsDev::
// DebugComponent subclass (DWARF lists ~60 members embedding Frustum, the tri-collision
// manager pointer, the per-test perf-mon counters and a 2 MiB spatial buffer pointer)
// whose bodies are separate engine-gated TUs (tri-collision manager + Debug3DImmediate
// render path are not yet reconstructed). Mirroring the committed sibling debug-name
// hooks (CgsDebugComponentPerfMonGpu.h / ...Cpu.h), the recovered hook is homed inline
// on a minimal standalone class so callers can compile against the name without pulling
// in the DebugComponent base cascade. GetName is the protected virtual override of
// CgsDev::DebugComponent::GetName() const; the full class layout + its base linkage are
// fleshed out IN THIS SAME FILE when the tri-collision debug TU is worked.

namespace CgsSceneManager
{
    // DWARF CgsTriangleCollisionDebugComponent.h:39
    const s32 KI_MAX_NUM_COLLISION_POLY_CALLBACKS = 4;

    class TriangleCollisionDebugComponent
    {
    public:
        // @0x827DD828. The debug-menu display name for this component.
        const char* GetName() const { return "Collision"; }
    };
}
