#pragma once

// ============================================================================
// GameShared/GameClasses/SceneManager/SharedIO/CgsInputBufferUpdate.h
//
// CgsSceneManager::SceneManagerIO::InputBuffer_Update -- the per-update scene-input IO buffer the
// physics/deformation cached-position pass reads the scene-update interface from. Reconstructed
// from the DecFIGS DWARF (CgsSceneManagerModuleIO.h:447): it derives CgsModule::IOBuffer and embeds
// one InSceneUpdateInterface (mSceneUpdateInterface), exposed via GetSceneUpdateInterface().
//
// FLAG: this is a MINIMAL additive home for the type (it was only forward-declared previously, e.g.
// as a local placeholder in BrnDeformationManager.h). The DWARF nests the typedef
//   InSmSceneUpdateInterface == InSceneUpdateInterface,
// so GetSceneUpdateInterface() returns an InSceneUpdateInterface* -- exactly what the detached
// part/wheel managers' UpdateTriangleCache(InSceneUpdateInterface*) take. The lifecycle bodies
// (Construct/Destruct/Clear) belong to the scene-manager IO TU and are declared-only here. GROW with
// the full member set / sibling IO buffers as that TU lands.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                                    // CgsModule::IOBuffer (base)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"            // CgsSceneManager::SceneManagerIO::InSceneUpdateInterface

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // DWARF CgsSceneManagerModuleIO.h:447. The per-update scene-input buffer.
    struct InputBuffer_Update : public CgsModule::IOBuffer
    {
        // DWARF :164. The embedded interface's nested alias (== InSceneUpdateInterface).
        typedef InSceneUpdateInterface InSmSceneUpdateInterface;

        // ---- lifecycle (DWARF :452-460). DECLARE-ONLY -- bodies owned by the scene-manager IO TU. ----
        void Construct();   // :452
        void Destruct();    // :456
        void Clear();       // :460

        // DWARF :462/:463. The embedded scene-update interface (const + mutable). Returns a pointer
        // to mSceneUpdateInterface -- the interface the part/wheel managers write through.
        const InSmSceneUpdateInterface* GetSceneUpdateInterface() const { return &mSceneUpdateInterface; }
        InSmSceneUpdateInterface*       GetSceneUpdateInterface()       { return &mSceneUpdateInterface; }

    private:
        // DWARF :467. The embedded scene-update interface (BY VALUE).
        InSmSceneUpdateInterface mSceneUpdateInterface;
    };
}
}
