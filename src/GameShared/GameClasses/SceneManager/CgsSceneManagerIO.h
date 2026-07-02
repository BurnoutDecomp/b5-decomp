#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                    // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<32768,16>
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"  // InSceneUpdateInterface (canonical home)

// CgsSceneManager::SceneManagerIO - the scene-manager module's IO buffer slices
// the world bridges drive. FLAG: MINIMAL slices -- only the accessors the bridges
// call are modelled (per the BrnPhysicsModuleIO.h / BrnCrashModuleIO.h precedent);
// the full buffer payloads are owned by the scene-manager IO TUs. X360 anchors:
//   InputBuffer_Update::GetInSceneUpdateInterface() @ 0x825BD8C0
//     write-lock (bit 3, "Not locked for writing\n", assert line 463) -> this + 16
//   OutputBuffer::GetSceneQueryResultsQueue() const @ 0x823B1ED0
//     read-lock (bit 4, "Not locked for reading\n") -> this + 4
//     (FLAG: the getter's IDA symbol is truncated to "CgsSceneManager::SceneM";
//      named here after the queue the scene->race-car bridge Appends from)
// InputBuffer_Update's Construct/Destruct are real committed symbols
// (@0x828C7B80 / @0x828BAD00, their own TUs).
namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // InSceneUpdateInterface's canonical home is CgsSceneManagerIO_SceneUpdate.h
    // (included above); its whole-interface Append (the merge the physics->scene
    // bridge drives) is declared there.

    // The scene-manager update-phase input buffer.
    struct InputBuffer_Update : public CgsModule::IOBuffer
    {
        // @ 0x825BD8C0 -- write-lock tripwire, then the in-scene-update aggregate.
        InSceneUpdateInterface* GetInSceneUpdateInterface();   // +16, write

    private:
        u8                     maStatusPad[15];           // +1..+15 (force +16)
        InSceneUpdateInterface mInSceneUpdateInterface;   // +16
    };

    // The scene-manager output buffer (query results side).
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::VariableEventQueue<32768, 16> SceneQueryResultsQueue;

        // @ 0x823B1ED0 -- read-lock tripwire, then the query-results ring.
        const SceneQueryResultsQueue* GetSceneQueryResultsQueue() const;   // +4, read

    private:
        u8                     maStatusPad[3];            // +1..+3 (force +4)
        SceneQueryResultsQueue mSceneQueryResultsQueue;   // +4
    };
}
}
