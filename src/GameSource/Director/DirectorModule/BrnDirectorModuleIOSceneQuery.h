#ifndef GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_MODULE_IO_SCENEQUERY_H
#define GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_MODULE_IO_SCENEQUERY_H

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                                     // CgsModule::IOBuffer base
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                   // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h"     // the producer + the six In-event types
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryResultsQueue.h"  // OutSceneQueryResultsQueue<N>

// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorModuleIOSceneQuery.h
//
// The Director module's two SCENE-QUERY IO buffers (DWARF BrnDirectorModuleIO.h:465 / :503).
//
//   SceneQueryOutputBuffer -- the director's OUTGOING queries: a SceneManager producer interface
//     (mSceneQueryInterface @+4) wired to six small query queues of its own. The director's
//     cameras stage their tests into it during PreSceneQueryUpdate (through
//     BrnDirector::SceneQueryInterface), and BrnGameModule::DoUpdate_Director appends its queues
//     to the external query buffer the world runs (SceneQueryInterface::Append @0x823C4FF8).
//       Construct                      @ 0x82239578
//       Destruct                       @ 0x8221B3F8
//       GetSceneQueryInterface() const @ 0x823B25F0 (read-lock,  DWARF :479)
//       GetSceneQueryInterface()       @ 0x82206B00 (write-lock, DWARF :480)
//
//   SceneQueryInputBuffer -- the ANSWERS: one OutSceneQueryResultsQueue<4032> (@+4) that
//     DoUpdate_Director fills from the world's results (VariableEventQueue<4032,16>::
//     Append<32768,16> @0x823DA090) and DirectorModule::ProcessSceneQueryResults @0x82239278 drains.
//       Construct                      @ 0x8221B310
//       Destruct                       @ 0x8221B380
//       GetResultsQueue()              @ 0x823B2698 (write-lock, DWARF :527)
//       GetResultsQueue() const        @ 0x82206BA8 (read-lock,  DWARF :526)
//
// ⭐ REAL LAYOUTS 2026-09-25 (FX-DIRECTOR2, the camera scene-query closure). Both used to be
// member-less shells whose getters returned `this + 4` typed as forward-declared opaque types --
// i.e. an address PAST the end of the 1-byte object CreateIOBuffer allocated. Nothing read through
// it while no director query was issued; the closure needs the real storage. Members and their
// console offsets are the DWARF's order pinned by the two Construct bodies (below); on this host
// the offsets differ and every access is by name.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace DirectorIO
{
    struct SceneQueryOutputBuffer : public CgsModule::IOBuffer
    {
        // The six query queues (DWARF :486..:491) -- capacities are the post offices' (10 / 40 / 10
        // / 10 / 10 / 1), and each element type is the SceneManager's In-event record.
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestFine, 10>            FineLineTestQueue;
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestNearest, 40>         FineLineTestNearestQueue;
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestFastDoubleSided, 10> FineLineTestFastDoubleSidedQueue;
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventSphereTestFast, 10>          SphereTestFastQueue;
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest, 10>       FineVolumeTestDeepestQueue;
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestFine, 1>           FineVolumeTestQueue;

        void Construct();                                                           // :473  @0x82239578
        void Destruct();                                                            // :477  @0x8221B3F8

        const CgsSceneManager::SceneManagerIO::SceneQueryInterface* GetSceneQueryInterface() const;   // :479
        CgsSceneManager::SceneManagerIO::SceneQueryInterface*       GetSceneQueryInterface();         // :480

    private:
        CgsSceneManager::SceneManagerIO::SceneQueryInterface mSceneQueryInterface;             // :484  console +0x0004
        FineLineTestQueue                                    mFineLineTestQueue;               // :486  console +0x0030
        FineLineTestNearestQueue                             mFineLineTestNearestQueue;        // :487  console +0x02C0
        FineLineTestFastDoubleSidedQueue                     mFineLineTestFastDoubleSidedQueue;// :488  console +0x0CD0
        SphereTestFastQueue                                  mSphereTestFastQueue;             // :489  console +0x0F60
        FineVolumeTestDeepestQueue                           mFineVolumeTestDeepestQueue;      // :490  console +0x1150
        FineVolumeTestQueue                                  mFineVolumeTestQueue;             // :491  console +0x1A20
    };

    struct SceneQueryInputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<4032> ResultsQueue;

        void Construct();                                                           // :508  @0x8221B310
        void Destruct();                                                            // :512  @0x8221B380

        const ResultsQueue* GetResultsQueue() const;                                // :526  read-lock
        ResultsQueue*       GetResultsQueue();                                      // :527  write-lock

    private:
        ResultsQueue mResultsQueue;                                                 // :531  console +0x04
    };
}
}

#endif // GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_MODULE_IO_SCENEQUERY_H
