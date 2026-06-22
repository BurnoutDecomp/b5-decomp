// Tiny embed check for the BrnWorld group's TUs: instantiate each homed type and exercise
// its bodied accessor(s) so the headers + out-of-line bodies compile together.
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                                      // PropVolumeID
#include "SharedClasses/World/BrnCollisionTag.h"                                              // CollisionTag
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"          // WorldEntityIO::OutputBuffer_Prepare
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                                  // BaseEventQueue
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropBecamePhysicalEvent.h" // PropBecamePhysicalEvent

namespace
{
    void EmbedCheck()
    {
        // PropVolumeID — pack a (type, volume) over a prop-owned VolumeId.
        BrnWorld::PropVolumeID lVol(
            CgsSceneManager::VolumeId(static_cast<u64>(BrnWorld::E_ENTITYTYPE_PROP) << 16));
        lVol.Set(static_cast<u16>(42), static_cast<u8>(5));

        // CollisionTag — decode a traffic direction.
        BrnWorld::CollisionTag lTag;
        f32 lfAngle = 0.0f;
        BrnWorld::TrafficDirection leDir = lTag.GetTrafficInfo(&lfAngle);
        (void)leDir;
        (void)lfAngle;

        // WorldEntityIO::OutputBuffer_Prepare — the write-locked accessor.
        BrnWorld::WorldEntityIO::OutputBuffer_Prepare* lpOut = nullptr;
        (void)lpOut;

        // BaseEventQueue<PropBecamePhysicalEvent>::Append — the instantiated symbol.
        typedef CgsModule::BaseEventQueue<BrnWorld::PropEntityIO::PropBecamePhysicalEvent> QueueT;
        bool (QueueT::*lpfnAppend)(const QueueT&) = &QueueT::Append;
        (void)lpfnAppend;
    }
}

void BrnWorld_EmbedCheckEntryPoint()
{
    EmbedCheck();
}
