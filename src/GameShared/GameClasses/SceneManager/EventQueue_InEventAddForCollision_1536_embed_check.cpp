// Embed check: prove the InEventAddForCollision element home and the PropEntityID ->
// CgsSceneManager::EntityId conversion operator compile and link cleanly as headers,
// independent of their owning TUs.
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddForCollision.h"
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"

namespace
{
    // Force the element type to be a complete, instantiable queue element.
    typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventAddForCollision, 1536>
        InAddForCollisionQueue;

    void EmbedCheck()
    {
        // Sized element + queue are complete.
        char lacElement[sizeof(CgsSceneManager::SceneManagerIO::InEventAddForCollision)];
        char lacQueue[sizeof(InAddForCollisionQueue)];
        (void)lacElement;
        (void)lacQueue;

        // The conversion operator yields a CgsSceneManager::EntityId from a prop handle.
        BrnWorld::PropEntityID lPropId(BrnWorld::E_ENTITYTYPE_PROP << 24);
        EntityId lEntity = lPropId; // exercises operator EntityId()
        (void)lEntity;
    }
}
