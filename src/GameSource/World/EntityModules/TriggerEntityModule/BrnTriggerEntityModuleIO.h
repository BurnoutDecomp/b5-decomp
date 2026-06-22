// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h
//
// Canonical (DWARF) home for the BrnWorld::TriggerEntityModuleIO IO buffers
// (DWARF home BrnTriggerEntityModuleIO.h). MINIMAL-COMPLETE slice: it homes ONLY
// OutputBuffer_PreScene -- the one buffer whose Construct the X360 emitted out-of-line at
// 0x822EED90 (the others -- InputBuffer_PreScene, InputBuffer_PostScene,
// OutputBuffer_PostScene, InputBuffer_PrePhysics, OutputBuffer_PrePhysics -- are NOT
// reproduced here yet; their own TUs grow this header ADDITIVELY when they land).
//
// LAYOUT (DWARF :81/:94 + X360 Construct @0x822EED90, authoritative):
//   base  CgsModule::IOBuffer                                  (1-byte status; +1..+15 pad)
//   +16   SceneInputInterface mSceneInputInterface             (InSceneUpdateInterface)  :94
// The X360 Construct sets the IOBuffer status byte to 1 (eStatusConstructed) then tail-calls
// the scene interface's Construct at `this + 0x10` (=+16), which fixes mSceneInputInterface
// at offset 16. InSceneUpdateInterface is alignas(16), so the 1-byte IOBuffer status is
// padded out to the 16-byte boundary before the member -- matching the +16 the asm uses.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                    // CgsModule::IOBuffer
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"  // InSceneUpdateInterface

namespace BrnWorld
{
namespace TriggerEntityModuleIO
{
    // BrnTrigger...IO.h:81 -- the pre-scene OUTPUT buffer: the trigger entity module's
    // scene-update commands for the scene manager.
    class OutputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        // BrnTriggerEntityModuleIO.h:40 typedef -- the scene input interface aggregate.
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterface;

        // BrnTriggerEntityModuleIO.h:84 -- this TU. @ X360 0x822EED90.
        void Construct();

        // BrnTriggerEntityModuleIO.h:87 / :90 / :91 -- declared-only (own TUs / sibling group).
        void Destruct();
        const SceneInputInterface* GetSceneInputInterface() const;
        SceneInputInterface*       GetSceneInputInterface();

        static void _AssertLayout();

    private:
        SceneInputInterface mSceneInputInterface;   // :94  (+16, alignas(16))
    };
}
}
