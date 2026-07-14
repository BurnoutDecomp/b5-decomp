#pragma once

// Single canonical definition of the empty per-module event base
//   CgsSceneManager::SceneManagerIO::Event
// (CgsModule event-queue convention: concrete events derive from it; the queue stores them by
// byte image). This was formerly defined inline in BOTH CgsSceneManagerModuleIO.h and
// CgsSceneManagerIO_EventAddForCollision.h. Once CgsSceneManagerIO_SceneUpdate.h was extended to
// include EventAddForCollision.h, any TU that pulls in CgsSceneManagerModuleIO.h (e.g. every
// OutSceneQueryResultsQueue instantiation) transitively included BOTH definitions and failed with
// C2011 (SceneManagerIO::Event redefinition). Hoisting the identical empty struct into this one
// #pragma once header removes the duplicate definition while keeping the type name/identity
// unchanged for every existing consumer.

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base. Concrete events (InEvent*/OutEvent* records) derive from it.
    struct Event {};
}
}
