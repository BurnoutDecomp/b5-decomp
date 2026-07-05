#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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

    // An entry in the static "draw collision poly" callback registry. The X360 accessor
    // (GetDrawCollisionPolyCallback, see below) reads/returns these at an exact 8-byte
    // stride (`slwi r10, liIndex, 3`); each slot is the callback functor the tri-collision
    // debug overlay invokes when it wants a registered system to render a colliding poly.
    // DWARF gives no field breakdown for the slot beyond its 8-byte size, so it is homed
    // as an opaque 8-byte POD (the X360-attested stride) — the canonical {fn-ptr, context}
    // field split is filled in IN THIS SAME STRUCT when the tri-collision debug TU is worked.
    struct DrawCollisionPolyCallback
    {
        u8 mPad0[8];
    };

    class TriangleCollisionDebugComponent
    {
    public:
        // @0x827DD828. The debug-menu display name for this component.
        const char* GetName() const { return "Collision"; }

        // Register this component with the debug-UI component registry (CgsDev::
        // DebugComponent::Register @ CgsDebugComponent.h:43). Called by
        // TriangleCollisionManager::Prepare. Declaration-only here; the full base linkage +
        // body are fleshed out with the tri-collision debug TU (grow this home, do not fork).
        void Register();

        // @0x828AA458 (recovered, EXECUTED in the boot-trace milestone).
        // Indexer into the static draw-collision-poly callback registry. The X360 body is
        // the bounds-checked accessor `RegisterDrawCollisionPolyCallback` uses to reach the
        // next free slot: it asserts liIndex < siNumDrawPolyCallbacks and returns
        // &siDrawPolyCallbacks[liIndex] (8-byte stride). Both the slot array and its live
        // count are file-static (X360 dword_830848A0 == siNumDrawPolyCallbacks,
        // unk_8307A880 == siDrawPolyCallbacks[]); homed as class statics here.
        static DrawCollisionPolyCallback& GetDrawCollisionPolyCallback(s32 liIndex)
        {
            CGS_ASSERT(liIndex < siNumDrawPolyCallbacks, "liIndex < siNumDrawPolyCallbacks");
            return siDrawPolyCallbacks[liIndex];
        }

    private:
        // X360 unk_8307A880 / dword_830848A0 — the static callback registry + its live count.
        static DrawCollisionPolyCallback siDrawPolyCallbacks[KI_MAX_NUM_COLLISION_POLY_CALLBACKS];
        static s32 siNumDrawPolyCallbacks;
    };
}
