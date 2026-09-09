// ===========================================================================
// CgsSceneManagerModule_wG_ExternalQueries.cpp
//   (GameShared/GameClasses/SceneManager)
//
// CgsSceneManager::SceneManagerModule::ExternalSceneQueriesUpdate -- the scene
// module's service point for queries submitted from OUTSIDE the world module (on
// this build, the director's camera collision / line-of-sight). Same coarse-then-
// fine machinery as the world's own pass, on a separately stacked buffer pair.
//
// The console's forwarder is a tail-jump into the module vtable's slot 17 (+68),
// ProcessSceneQueries, whose four parameters are the four forwarded here. The
// caller's fifth argument (the frame update set) is not consumed by the target and
// stops at the world-module boundary, so it is not accepted here. The whole console
// function is that one dispatch.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModule.h"

namespace CgsSceneManager
{

// ===========================================================================
// SceneManagerModule::ExternalSceneQueriesUpdate   (the vtbl+68 dispatch)
// ===========================================================================
void SceneManagerModule::ExternalSceneQueriesUpdate(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        SceneManagerIO::InputBuffer_Query* lpQueryInput,
        SceneManagerIO::OutputBuffer* lpQueryOutput )
{
    ProcessSceneQueries( lpInputBufferStack, lpOutputBufferStack,
                         lpQueryInput, lpQueryOutput );
}

} // namespace CgsSceneManager
