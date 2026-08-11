// CgsTriangleCollisionDebugComponent.cpp
//
// The triangle-collision debug component's recovered boot-trace function,
//   CgsSceneManager::TriangleCollisionDebugComponent::GetName @ 0x827DD828 -> "Collision",
// is a leaf that returns a literal and is homed inline on the class in the header (mirroring
// the committed PerfMon debug-name hooks). This translation unit exists to anchor that header
// against the canonical class declaration; the heavy component bodies (the per-test collision
// queries + Debug3DImmediate render path) are separate engine-gated TUs.
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h"

// Definitions for the file-static draw-collision-poly callback registry that backs the
// recovered accessor GetDrawCollisionPolyCallback @ 0x828AA458 (EXECUTED in the boot-trace
// milestone). On X360 these are unk_8307A880 (the slot array) and dword_830848A0 (the live
// count, siNumDrawPolyCallbacks); both live in BSS and start zeroed.
namespace CgsSceneManager
{
    DrawCollisionPolyCallback TriangleCollisionDebugComponent::siDrawPolyCallbacks[KI_MAX_NUM_COLLISION_POLY_CALLBACKS] = {};
    s32 TriangleCollisionDebugComponent::siNumDrawPolyCallbacks = 0;

    // ------------------------------------------------------------------------
    // Register -- FLAG PC-platform leaf (2026-08-10, spatial-partition wave).
    //
    // ⚠️ THIS IS A DELIBERATE NO-OP AND IT IS SAID OUT LOUD, because an empty body
    // on a live path is the [[silent-drop-stubs]] shape and must never be quiet.
    //
    // WHAT THE CONSOLE DOES: TriangleCollisionManager::Prepare @0x828B2FF0 calls
    // mDebugComponent.Register(), which is CgsDev::DebugComponent::Register
    // @0x828331D8 -- ThreadSafeAquire the DebugManager, RegisterComponent(this,
    // GetPath(), GetName() == "Collision"), ThreadSafeRelease. Its ONLY effect is
    // to thread this component onto the debug-menu list.
    //
    // WHY IT IS EMPTY HERE, and what it costs: TriangleCollisionDebugComponent does
    // NOT derive from CgsDev::DebugComponent in this tree (the header says so, and
    // says the base linkage belongs to "the tri-collision debug TU"), and the whole
    // DebugUI window family is a known blocked cascade ([[debugui-window-cascade]]:
    // Window / MenuItem / CustomWindow / StrStreamBase base layouts are not
    // reconstructed). Deriving it here to make one call real would drag that cascade
    // onto the link for zero gameplay effect.
    //
    // ⭐ THE COST IS BOUNDED AND CHECKED, not assumed: the ONLY thing skipping this
    // loses is a debug-menu ENTRY. It touches no partition state, allocates nothing
    // from mSpacialAllocator, and returns void, so Prepare's remaining work (the
    // handle array, the spatial map, the sub-allocator) is unaffected. Nothing in
    // the triangle-cache or spatial-partition path reads the debug registry.
    //
    // ⛔ TO RETIRE THIS: derive TriangleCollisionDebugComponent from
    // CgsDev::DebugComponent and delete this definition -- do NOT quietly grow a
    // body here.
    // ------------------------------------------------------------------------
    void TriangleCollisionDebugComponent::Register()
    {
    }
}
