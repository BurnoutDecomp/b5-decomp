#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout
// reconstructed by InSceneUpdateInterface's own TU (DWARF home
// CgsSceneManagerIO_SceneUpdate.h). Size 256 (NOMINAL -- not byte-verified, grown
// by own TU).
//
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface is the scene-input
// interface embedded BY VALUE in BrnWorld::RaceCarEntityModuleIO's
// OutputBuffer_PreScene (typedef SceneInputInterface), InputBuffer_PostPhysics and
// OutputBuffer_PostPhysics. Per the DWARF (CgsSceneManagerIO_SceneUpdate.h:307) the
// real type is a large aggregate of ~28 fixed-capacity EventQueue<> members
// (mUpdatePositionQueue ... mRemoveAllEntitiesQueue) plus Construct/Append/HasData
// accessors -- it carries EventQueues, so the slice is alignas(16). The IO header
// only ever takes &member of this payload, so a complete sized blob is sufficient
// to unlock the buffer layout; the full member set belongs to this type's own
// ledger TU (the 6-subsystem scene-update cascade is intentionally NOT pulled in).

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct alignas(16) InSceneUpdateInterface
    {
        // ADDITIVE GROW (FLAG -- body owned by InSceneUpdateInterface's own TU): the
        // X360 trigger/prop output-buffer Construct bodies (e.g.
        // BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene::Construct @0x822EED90)
        // tail-call InSceneUpdateInterface::Construct on this aggregate. Declared-only here
        // so consuming TUs compile; the owning TU emits the body. No layout change.
        void Construct();

        unsigned char maReserved[256];
    };
}
}
