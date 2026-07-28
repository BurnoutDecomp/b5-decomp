#ifndef GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_MODULE_IO_SCENEQUERY_H
#define GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_MODULE_IO_SCENEQUERY_H

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer base

// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorModuleIOSceneQuery.h
//
// The Director module's two SCENE-QUERY IO buffers. These were previously declared
// LOCALLY inside BrnDirectorModuleIOSceneQuery.cpp (which owns their accessor bodies);
// promoted to a header here, unchanged, because DirectorModule::Update /
// ::PreSceneQueryUpdate / ::ProcessSceneQueryResults now name them in their signatures.
//
//   SceneQueryOutputBuffer -- the director's OUTGOING queries. Its single published member
//     is the SceneManager producer interface @+4; the module stages it into the per-frame
//     BrnDirector::SceneQueryInterface post office each PreSceneQueryUpdate / Update.
//       GetSceneQueryInterface() const @ 0x823B25F0 (read-lock,  DWARF :510/479)
//       GetSceneQueryInterface()       @ 0x82206B00 (write-lock, DWARF :511/480)
//
//   SceneQueryInputBuffer -- the results the SceneManager published back. Its single
//     published member is the results queue @+4, drained by
//     DirectorModule::ProcessSceneQueryResults @0x82239278.
//       GetResultsQueue()              @ 0x823B2698 (write-lock, DWARF :558/527)
//       GetResultsQueue() const        @ 0x82206BA8 (read-lock,  DWARF :557/526)
//
// Both are the recurring CgsModule::IOBuffer lock-guarded getter shape: test a lock bit on
// the status byte at this+0, CGS_ASSERT on violation, then return the address of the first
// published member @+4 (immediately after the 1-byte IOBuffer/FlagSet8 base).
//
// The two member TYPES stay FORWARD-DECLARED here exactly as the .cpp had them: the bodies
// are address-returns, so an opaque type suffices and the heavy SceneManager includes are
// kept out of every consumer. Both have committed homes under CgsSceneManager::SceneManagerIO
// (CgsSceneManagerIO_SceneQueryInterface.h / CgsSceneManagerIO_SceneQueryResultsQueue.h) --
// a consumer that needs the interior includes them itself and reinterpret-casts the returned
// address (this is what DirectorModule::Update does for the HasData tripwire).
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace DirectorIO
{
    // DWARF member types; forward-declared -- only their address (@+4) is used by the
    // accessors below.
    struct SceneQueryInterface;                            // CgsSceneManager::SceneManagerIO type
    template <int N> struct OutSceneQueryResultsQueue;     // committed home; N == 4032

    struct SceneQueryOutputBuffer : public CgsModule::IOBuffer
    {
        // DWARF: mSceneQueryInterface (type SceneQueryInterface) @+4.
        const SceneQueryInterface* GetSceneQueryInterface() const;  // this+4, read-lock
        SceneQueryInterface*       GetSceneQueryInterface();        // this+4, write-lock
    };

    struct SceneQueryInputBuffer : public CgsModule::IOBuffer
    {
        // DWARF: mResultsQueue (type OutSceneQueryResultsQueue<4032>) @+4.
        OutSceneQueryResultsQueue<4032>*       GetResultsQueue();        // this+4, write-lock
        const OutSceneQueryResultsQueue<4032>* GetResultsQueue() const;  // this+4, read-lock
    };
}
}

#endif // GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_MODULE_IO_SCENEQUERY_H
