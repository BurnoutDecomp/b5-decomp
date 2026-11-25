#pragma once

// ARTIST RemoveEntity (0x822B12D0) writes a byte at +0x04, which the
// scene consumer reads at 0x828D2784. The queue copies two words (0x822AB180).
// DecFIGS CgsSceneManagerIO_SceneUpdate.h:80 declares EntityId and bool.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"   // CgsSceneManager::EntityId (u32 handle)

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseRemoveEntity {};

    // Four-byte alignment gives the queue an eight-byte stride.
    class InEventRemoveEntity : public EventBaseRemoveEntity
    {
    public:
        EntityId mEntityId; // +0x00 -- the scene-entity handle to remove
        bool     mbRemoveVolumeInstances; // +0x04
    };
}
}
